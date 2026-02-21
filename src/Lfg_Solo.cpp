/*
** Made by Traesh https://github.com
** AzerothCore 2019 http://www.azerothcore.org
** Conan513 https://github.com
** Made into a module by Micrah https://github.com
** CriteriaCheck by yani9o https://github.com
*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Group.h"
#include "LFGMgr.h"
#include "AchievementMgr.h"
#include <sstream>

// ----------------- ANNOUNCE / XP -----------------
class lfg_solo_announce : public PlayerScript
{
public:
    lfg_solo_announce() : PlayerScript("lfg_solo_announce") {}

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("SoloLFG.Announce", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "This server is running the |cff4CFF00Solo Dungeon Finder |rmodule."
            );
        }
    }

    void OnPlayerRewardKillRewarder(Player* /*player*/, KillRewarder* /*rewarder*/, bool isDungeon, float& rate) override
    {
        if (!isDungeon
            || !sConfigMgr->GetOption<bool>("SoloLFG.Enable", true)
            || !sConfigMgr->GetOption<bool>("SoloLFG.FixedXP", true))
            return;

        rate = sConfigMgr->GetOption<float>("SoloLFG.FixedXPRate", 0.2f);
    }
};

// ----------------- CRITERIA LOCK -----------------
class lfg_criteria_lock : public PlayerScript
{
public:
    lfg_criteria_lock() : PlayerScript("lfg_criteria_lock") {}

    bool OnPlayerCanJoinLfg(Player* player, uint8 /*roles*/, std::set<uint32>& dungeons, const std::string& /*comment*/) override
    {
        if (!sConfigMgr->GetOption<bool>("SoloLFG.Enable", true) ||
            !sConfigMgr->GetOption<bool>("SoloLFG.CriteriaCheck", true))
            return true;

        Group* group = player->GetGroup();
        uint32 memberCount = group ? group->GetMembersCount() : 1;

        for (uint32 dungeonId : dungeons)
        {
            if (dungeonId >= 258 && dungeonId <= 262)
            {
                if (memberCount < 5)
                {
                    ChatHandler(player->GetSession()).SendSysMessage("Zufällige Dungeons sind nur für komplette Gruppen verfügbar.");
                    return false;
                }
            }

            std::string configKey = "SoloLFG.DungeonCriteria." + std::to_string(dungeonId);
            std::string value = sConfigMgr->GetOption<std::string>(configKey, "");

            if (value.empty())
                continue;

            std::stringstream ss(value);
            std::string token;
            bool anyoneHasIt = false;
            std::string missingBossName = "Unbekannter Boss";

            while (std::getline(ss, token, ','))
            {
                token.erase(0, token.find_first_not_of(" \t\r\n"));
                token.erase(token.find_last_not_of(" \t\r\n") + 1);

                if (token.empty()) continue;

                size_t colonPos = token.find(':');
                uint32 criteriaId = 0;
                std::string bossNameFromConfig = "Boss-Kill";

                try {
                    if (colonPos != std::string::npos) {
                        criteriaId = std::stoul(token.substr(0, colonPos));
                        bossNameFromConfig = token.substr(colonPos + 1);
                    } else {
                        criteriaId = std::stoul(token);
                    }
                } catch (...) { continue; }

                if (criteriaId == 0) continue;

                AchievementCriteriaEntry const* ce = sAchievementCriteriaStore.LookupEntry(criteriaId);
                if (!ce) continue;

                missingBossName = bossNameFromConfig;

                auto CheckPlayer = [&](Player* p) -> bool {
                    if (!p) return false;
                    
                    AchievementMgr* achMgr = p->GetAchievementMgr();
                    if (!achMgr) return false;

                    if (CriteriaProgress const* progress = achMgr->GetCriteriaProgress(ce))
                    {
                        if (progress->counter >= 1) 
                            return true;
                    }
                    
                    if (p->HasAchieved(ce->referredAchievement))
                        return true;

                    return false;
                };

                if (group) {
                    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next()) {
                        if (Player* member = ref->GetSource()) {
                            if (CheckPlayer(member)) {
                                anyoneHasIt = true;
                                break;
                            }
                        }
                    }
                } else {
                    if (CheckPlayer(player))
                        anyoneHasIt = true;
                }

                if (anyoneHasIt) break;
            }

            if (!anyoneHasIt)
            {
                std::string finalMsg = "Fortschritt fehlt: |cffff0000" + missingBossName + "|r";
                ChatHandler(player->GetSession()).SendSysMessage(finalMsg.c_str());
                return false;
            }
        }
        return true;
    }
};

// ----------------- WORLDSCRIPT -----------------
class lfg_solo : public WorldScript
{
public:
    lfg_solo() : WorldScript("lfg_solo") {}

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        bool enable = sConfigMgr->GetOption<bool>("SoloLFG.Enable", true);

        if (enable != sLFGMgr->IsTesting())
            sLFGMgr->ToggleTesting();
    }
};

// ----------------- REGISTER -----------------
void AddLfgSoloScripts()
{
    new lfg_solo_announce();
    new lfg_solo();
    new lfg_criteria_lock();
}
