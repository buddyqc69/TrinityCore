/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: go_commandscript
%Complete: 100
Comment: All go related commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Language.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RBAC.h"
#include "SupportMgr.h"
#include "Transport.h"
#include "Util.h"
#include "WorldSession.h"
#include <sstream>

using namespace Trinity::ChatCommands;
class go_commandscript : public CommandScript
{
public:
    go_commandscript() : CommandScript("go_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> goCreatureCommandTable =
        {
            { "id",     rbac::RBAC_PERM_COMMAND_GO,     false,      &HandleGoCreatureCIdCommand,            "" },
            { "",       rbac::RBAC_PERM_COMMAND_GO,     false,      &HandleGoCreatureSpawnIdCommand,        "" }
        };

        static std::vector<ChatCommand> goGameObjectCommandTable =
        {
            { "id",     rbac::RBAC_PERM_COMMAND_GO,     false,      &HandleGoGameObjectGOIdCommand,         "" },
            { "",       rbac::RBAC_PERM_COMMAND_GO,     false,      &HandleGoGameObjectSpawnIdCommand,      "" }
        };

        static std::vector<ChatCommand> goCommandTable =
        {
            { "creature",           rbac::RBAC_PERM_COMMAND_GO,             false, nullptr,                                     "", goCreatureCommandTable },
            { "gameobject",         rbac::RBAC_PERM_COMMAND_GO,             false, nullptr,                                     "", goGameObjectCommandTable },
            { "graveyard",          rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoGraveyardCommand,                   "" },
            { "grid",               rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoGridCommand,                        "" },
            { "quest",              rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoQuestCommand,                       "" },
            { "taxinode",           rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoTaxinodeCommand,                    "" },
            { "areatrigger",        rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoAreaTriggerCommand,                 "" },
            { "zonexy",             rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoZoneXYCommand,                      "" },
            { "xyz",                rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoXYZCommand,                         "" },
            { "bugticket",          rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoTicketCommand<BugTicket>,           "" },
            { "complaintticket",    rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoTicketCommand<ComplaintTicket>,     "" },
            { "suggestionticket",   rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoTicketCommand<SuggestionTicket>,    "" },
            { "offset",             rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoOffsetCommand,                      "" },
            { "instance",           rbac::RBAC_PERM_COMMAND_GO,             false, &HandleGoInstanceCommand,                    "" }
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "go", rbac::RBAC_PERM_COMMAND_GO, false, nullptr, "", goCommandTable },
        };
        return commandTable;
    }

    static bool DoTeleport(ChatHandler* handler, WorldLocation loc)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!MapManager::IsValidMapCoord(loc) || sObjectMgr->IsTransportMap(loc.GetMapId()))
        {
            handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, loc.GetPositionX(), loc.GetPositionY(), loc.GetPositionZ());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // stop flight if need
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition(); // save only in non-flight case

        player->TeleportTo(loc);
        return true;
    }

    static bool HandleGoCreatureSpawnIdCommand(ChatHandler* handler, Variant<Hyperlink<creature>, ObjectGuid::LowType> spawnId)
    {
        CreatureData const* spawnpoint = sObjectMgr->GetCreatureData(spawnId);
        if (!spawnpoint)
        {
            handler->SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return DoTeleport(handler, spawnpoint->spawnPoint);
    }

    static bool HandleGoCreatureCIdCommand(ChatHandler* handler, Variant<Hyperlink<creature_entry>, uint32> cId)
    {
        CreatureData const* spawnpoint = nullptr;
        for (auto const& pair : sObjectMgr->GetAllCreatureData())
        {
            if (pair.second.id != *cId)
                continue;

            if (!spawnpoint)
                spawnpoint = &pair.second;
            else
            {
                handler->SendSysMessage(LANG_COMMAND_GOCREATMULTIPLE);
                break;
            }
        }

        if (!spawnpoint)
        {
            handler->SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return DoTeleport(handler, spawnpoint->spawnPoint);
    }

    static bool HandleGoGameObjectSpawnIdCommand(ChatHandler* handler, uint32 spawnId)
    {
        GameObjectData const* spawnpoint = sObjectMgr->GetGameObjectData(spawnId);
        if (!spawnpoint)
        {
            handler->SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return DoTeleport(handler, spawnpoint->spawnPoint);
    }

    static bool HandleGoGameObjectGOIdCommand(ChatHandler* handler, uint32 goId)
    {
        GameObjectData const* spawnpoint = nullptr;
        for (auto const& pair : sObjectMgr->GetAllGameObjectData())
        {
            if (pair.second.id != goId)
                continue;

            if (!spawnpoint)
                spawnpoint = &pair.second;
            else
            {
                handler->SendSysMessage(LANG_COMMAND_GOCREATMULTIPLE);
                break;
            }
        }

        if (!spawnpoint)
        {
            handler->SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return DoTeleport(handler, spawnpoint->spawnPoint);
    }

    static bool HandleGoGraveyardCommand(ChatHandler* handler, uint32 gyId)
    {
        WorldSafeLocsEntry const* gy = sObjectMgr->GetWorldSafeLoc(gyId);
        if (!gy)
        {
            handler->PSendSysMessage(LANG_COMMAND_GRAVEYARDNOEXIST, gyId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!MapManager::IsValidMapCoord(gy->Loc))
        {
            handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, gy->Loc.GetPositionX(), gy->Loc.GetPositionY(), gy->Loc.GetMapId());
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        // stop flight if need
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition(); // save only in non-flight case

        player->TeleportTo(gy->Loc);
        return true;
    }

    //teleport to grid
    static bool HandleGoGridCommand(ChatHandler* handler, float gridX, float gridY, Optional<uint32> oMapId)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 mapId = oMapId.get_value_or(player->GetMapId());

        // center of grid
        float x = (gridX - CENTER_GRID_ID + 0.5f) * SIZE_OF_GRIDS;
        float y = (gridY - CENTER_GRID_ID + 0.5f) * SIZE_OF_GRIDS;

        if (!MapManager::IsValidMapCoord(mapId, x, y))
        {
            handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // stop flight if need
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition(); // save only in non-flight case

        Map* map = sMapMgr->CreateBaseMap(mapId);
        float z = std::max(map->GetStaticHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, MAX_HEIGHT), map->GetWaterLevel(PhasingHandler::GetEmptyPhaseShift(), x, y));

        player->TeleportTo(mapId, x, y, z, player->GetOrientation());
        return true;
    }

    static bool HandleGoQuestCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();

        char* id = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!id)
            return false;

        uint32 questID = atoul(id);
        if (!questID)
            return false;

        if (!sObjectMgr->GetQuestTemplate(questID))
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, questID);
            handler->SetSentErrorMessage(true);
            return false;
        }

        float x, y, z;
        uint32 mapId;

        if (QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(questID))
        {
            QuestPOIBlobData const& data = poiData->Blobs.front();

            mapId = data.MapID;

            x = data.Points.front().X;
            y = data.Points.front().Y;
            z = data.Points.front().Z;
        }
        else
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, questID);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!MapManager::IsValidMapCoord(mapId, x, y) || sObjectMgr->IsTransportMap(mapId))
        {
            handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // stop flight if need
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition(); // save only in non-flight case

        Map* map = sMapMgr->CreateBaseMap(mapId);
        z = std::max(map->GetStaticHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, MAX_HEIGHT), map->GetWaterLevel(PhasingHandler::GetEmptyPhaseShift(), x, y));

        player->TeleportTo(mapId, x, y, z, 0.0f);
        return true;
    }

    static bool HandleGoTaxinodeCommand(ChatHandler* handler, Variant<Hyperlink<taxinode>, uint32> nodeId)
    {
        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(nodeId);
        if (!node)
        {
            handler->PSendSysMessage(LANG_COMMAND_GOTAXINODENOTFOUND, nodeId);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return DoTeleport(handler, { node->ContinentID, { node->Pos.X, node->Pos.Y, node->Pos.Z } });
    }

    static bool HandleGoAreaTriggerCommand(ChatHandler* handler, Variant<Hyperlink<areatrigger>, uint32> areaTriggerId)
    {
        AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(areaTriggerId);
        if (!at)
        {
            handler->PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, areaTriggerId);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return DoTeleport(handler, { uint32(at->ContinentID), { at->Pos.X, at->Pos.Y, at->Pos.Z } });
    }

    //teleport at coordinates
    static bool HandleGoZoneXYCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();

        char* zoneX = strtok((char*)args, " ");
        char* zoneY = strtok(nullptr, " ");
        char* tail = strtok(nullptr, "");

        char* id = handler->extractKeyFromLink(tail, "Harea");       // string or [name] Shift-click form |color|Harea:area_id|h[name]|h|r

        if (!zoneX || !zoneY)
            return false;

        float x = (float)atof(zoneX);
        float y = (float)atof(zoneY);

        // prevent accept wrong numeric args
        if ((x == 0.0f && *zoneX != '0') || (y == 0.0f && *zoneY != '0'))
            return false;

        uint32 areaId = id ? atoul(id) : player->GetZoneId();

        AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(areaId);

        if (x < 0 || x > 100 || y < 0 || y > 100 || !areaEntry)
        {
            handler->PSendSysMessage(LANG_INVALID_ZONE_COORD, x, y, areaId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // update to parent zone if exist (client map show only zones without parents)
        AreaTableEntry const* zoneEntry = areaEntry->ParentAreaID ? sAreaTableStore.LookupEntry(areaEntry->ParentAreaID) : areaEntry;
        ASSERT(zoneEntry);

        Map* map = sMapMgr->CreateBaseMap(zoneEntry->ContinentID);

        if (map->Instanceable())
        {
            handler->PSendSysMessage(LANG_INVALID_ZONE_MAP, areaId, areaEntry->AreaName[handler->GetSessionDbcLocale()], map->GetId(), map->GetMapName());
            handler->SetSentErrorMessage(true);
            return false;
        }

        x /= 100.0f;
        y /= 100.0f;

        sDB2Manager.Zone2MapCoordinates(areaEntry->ParentAreaID ? uint32(areaEntry->ParentAreaID) : areaId, x, y);

        if (!MapManager::IsValidMapCoord(zoneEntry->ContinentID, x, y))
        {
            handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, uint32(zoneEntry->ContinentID));
            handler->SetSentErrorMessage(true);
            return false;
        }

        // stop flight if need
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition(); // save only in non-flight case

        float z = std::max(map->GetStaticHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, MAX_HEIGHT), map->GetWaterLevel(PhasingHandler::GetEmptyPhaseShift(), x, y));

        player->TeleportTo(zoneEntry->ContinentID, x, y, z, player->GetOrientation());
        return true;
    }

    //teleport at coordinates, including Z and orientation
    static bool HandleGoXYZCommand(ChatHandler* handler, float x, float y, Optional<float> z, Optional<uint32> id, Optional<float> o)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 mapId = id.get_value_or(player->GetMapId());
        if (z)
        {
            if (!MapManager::IsValidMapCoord(mapId, x, y, *z))
            {
                handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapId);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            if (!MapManager::IsValidMapCoord(mapId, x, y))
            {
                handler->PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapId);
                handler->SetSentErrorMessage(true);
                return false;
            }
            Map* map = sMapMgr->CreateBaseMap(mapId);
            z = std::max(map->GetStaticHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, MAX_HEIGHT), map->GetWaterLevel(PhasingHandler::GetEmptyPhaseShift(), x, y));
        }

        return DoTeleport(handler, { mapId, { x, y, *z, o.get_value_or(0.0f) } });
    }

    template<typename T>
    static bool HandleGoTicketCommand(ChatHandler* handler, uint32 ticketId)
    {
        T* ticket = sSupportMgr->GetTicket<T>(ticketId);
        if (!ticket)
        {
            handler->SendSysMessage(LANG_COMMAND_TICKETNOTEXIST);
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();

        // stop flight if need
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition(); // save only in non-flight case

        ticket->TeleportTo(player);
        return true;
    }

    static bool HandleGoOffsetCommand(ChatHandler* handler, float dX, Optional<float> dY, Optional<float> dZ, Optional<float> dO)
    {
        WorldLocation loc = handler->GetSession()->GetPlayer()->GetWorldLocation();
        loc.RelocateOffset({ dX, dY.get_value_or(0.0f), dZ.get_value_or(0.0f), dO.get_value_or(0.0f) });

        return DoTeleport(handler, loc);
    }

    static bool HandleGoInstanceCommand(ChatHandler* handler, std::vector<std::string> const& labels)
    {
        uint32 mapid = 0;

        std::multimap<uint32, std::pair<uint32, std::string>> matches;
        for (auto const& pair : sObjectMgr->GetInstanceTemplates())
        {
            uint32 count = 0;
            std::string const& scriptName = sObjectMgr->GetScriptName(pair.second.ScriptId);
            for (auto const& label : labels)
                if (StringContainsStringI(scriptName, label))
                    ++count;

            if (count)
                matches.emplace(count, decltype(matches)::mapped_type({ pair.first, scriptName }));
        }
        if (matches.empty())
        {
            handler->SendSysMessage(LANG_COMMAND_NO_INSTANCES_MATCH);
            return false;
        }
        auto it = matches.rbegin();
        uint32 maxCount = it->first;
        mapid = it->second.first;
        if (++it != matches.rend() && it->first == maxCount)
        {
            handler->SendSysMessage(LANG_COMMAND_MULTIPLE_INSTANCES_MATCH);
            --it;
            do
                handler->PSendSysMessage(LANG_COMMAND_MULTIPLE_INSTANCES_ENTRY, it->second.first, it->second.second);
            while (++it != matches.rend() && it->first == maxCount);
            handler->SetSentErrorMessage(true);
            return false;
        }

        ASSERT(mapid);

        InstanceTemplate const* temp = sObjectMgr->GetInstanceTemplate(mapid);
        if (!temp)
        {
            handler->PSendSysMessage(LANG_COMMAND_MAP_NOT_INSTANCE, mapid);
            handler->SetSentErrorMessage(true);
            return false;
        }
        std::string const& scriptname = sObjectMgr->GetScriptName(temp->ScriptId);

        Player* player = handler->GetSession()->GetPlayer();
        if (player->IsInFlight())
            player->FinishTaxiFlight();
        else
            player->SaveRecallPosition();

        // try going to entrance
        AreaTriggerStruct const* exit = sObjectMgr->GetGoBackTrigger(mapid);
        if (!exit)
            handler->PSendSysMessage(LANG_COMMAND_INSTANCE_NO_EXIT, mapid, scriptname.c_str());

        if (exit && player->TeleportTo(exit->target_mapId, exit->target_X, exit->target_Y, exit->target_Z, exit->target_Orientation + M_PI))
        {
            handler->PSendSysMessage(LANG_COMMAND_WENT_TO_INSTANCE_GATE, mapid, scriptname.c_str());
            return true;
        }

        // try going to start
        AreaTriggerStruct const* entrance = sObjectMgr->GetMapEntranceTrigger(mapid);
        if (!entrance)
            handler->PSendSysMessage(LANG_COMMAND_INSTANCE_NO_ENTRANCE, mapid, scriptname.c_str());

        if (entrance && player->TeleportTo(entrance->target_mapId, entrance->target_X, entrance->target_Y, entrance->target_Z, entrance->target_Orientation))
        {
            handler->PSendSysMessage(LANG_COMMAND_WENT_TO_INSTANCE_START, mapid, scriptname.c_str());
            return true;
        }

        handler->PSendSysMessage(LANG_COMMAND_GO_INSTANCE_FAILED, mapid, scriptname.c_str(), exit->target_mapId);
        handler->SetSentErrorMessage(true);
        return false;
    }
};

void AddSC_go_commandscript()
{
    new go_commandscript();
}
