/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * Copyright (C) 2008-2010 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef TRINITY_INSTANCE_DATA_H
#define TRINITY_INSTANCE_DATA_H

#include "ZoneScript.h"
#include "World.h"
//#include "GameObject.h"
//#include "Map.h"

class Map;
class Unit;
class Player;
class GameObject;
class Creature;

typedef std::set<GameObject*> DoorSet;
typedef std::set<Creature*> MinionSet;

enum EncounterState
{
    NOT_STARTED   = 0,
    IN_PROGRESS   = 1,
    FAIL          = 2,
    DONE          = 3,
    SPECIAL       = 4,
    TO_BE_DECIDED = 5,
};

enum DoorType
{
    DOOR_TYPE_ROOM = 0,
    DOOR_TYPE_PASSAGE,
    MAX_DOOR_TYPES,
};

enum BoundaryType
{
    BOUNDARY_NONE = 0,
    BOUNDARY_N,
    BOUNDARY_S,
    BOUNDARY_E,
    BOUNDARY_W,
    BOUNDARY_NE,
    BOUNDARY_NW,
    BOUNDARY_SE,
    BOUNDARY_SW,
    BOUNDARY_MAX_X = BOUNDARY_N,
    BOUNDARY_MIN_X = BOUNDARY_S,
    BOUNDARY_MAX_Y = BOUNDARY_W,
    BOUNDARY_MIN_Y = BOUNDARY_E,
};

typedef std::map<BoundaryType, float> BossBoundaryMap;

struct DoorData
{
    uint32 entry, bossId;
    DoorType type;
    uint32 boundary;
};

struct MinionData
{
    uint32 entry, bossId;
};

struct BossInfo
{
    BossInfo() : state(TO_BE_DECIDED) {}
    EncounterState state;
    DoorSet door[MAX_DOOR_TYPES];
    MinionSet minion;
    BossBoundaryMap boundary;
};

struct DoorInfo
{
    explicit DoorInfo(BossInfo *_bossInfo, DoorType _type, BoundaryType _boundary)
        : bossInfo(_bossInfo), type(_type), boundary(_boundary) {}
    BossInfo *bossInfo;
    DoorType type;
    BoundaryType boundary;
};

struct MinionInfo
{
    explicit MinionInfo(BossInfo *_bossInfo) : bossInfo(_bossInfo) {}
    BossInfo *bossInfo;
};

typedef std::multimap<uint32 /*entry*/, DoorInfo> DoorInfoMap;
typedef std::map<uint32 /*entry*/, MinionInfo> MinionInfoMap;

class InstanceData : public ZoneScript
{
    public:

        explicit InstanceData(Map *map) : instance(map) {}
        virtual ~InstanceData() {}

        Map *instance;

        //On creation, NOT load.
        virtual void Initialize() {}

        //On load
        virtual void Load(const char * data) { LoadBossState(data); }

        //When save is needed, this function generates the data
        virtual std::string GetSaveData() { return GetBossSaveData(); }

        void SaveToDB();

        virtual void Update(uint32 diff) {}

        //Used by the map's CanEnter function.
        //This is to prevent players from entering during boss encounters.
        virtual bool IsEncounterInProgress() const;

        //Called when a player successfully enters the instance.
        virtual void OnPlayerEnter(Player *) {}

        //Handle open / close objects
        //use HandleGameObject(NULL,boolen,GO); in OnObjectCreate in instance scripts
        //use HandleGameObject(GUID,boolen,NULL); in any other script
        void HandleGameObject(uint64 GUID, bool open, GameObject *go = NULL);

        //change active state of doors or buttons
        void DoUseDoorOrButton(uint64 uiGuid, uint32 uiWithRestoreTime = 0, bool bUseAlternativeState = false);

        //Respawns a GO having negative spawntimesecs in gameobject-table
        void DoRespawnGameObject(uint64 uiGuid, uint32 uiTimeToDespawn = MINUTE);

        //sends world state update to all players in instance
        void DoUpdateWorldState(uint32 uiStateId, uint32 uiStateData);

        // Send Notify to all players in instance
        void DoSendNotifyToInstance(const char *format,...);

        // Complete Achievement for all players in instance
        void DoCompleteAchievement(uint32 achievement);

        // Update Achievement Criteria for all players in instance
        void DoUpdateAchievementCriteria(AchievementCriteriaTypes type, uint32 miscvalue1=0, uint32 miscvalue2=0, Unit *unit=NULL, uint32 time=0);

        // Remove Auras due to Spell on all players in instance
        void DoRemoveAurasDueToSpellOnPlayers(uint32 spell);

        // Return wether server allow two side groups or not
        bool ServerAllowsTwoSideGroups() { return sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP); }

        virtual bool SetBossState(uint32 id, EncounterState state);
        EncounterState GetBossState(uint32 id) const { return id < bosses.size() ? bosses[id].state : TO_BE_DECIDED; }
        const BossBoundaryMap * GetBossBoundary(uint32 id) const { return id < bosses.size() ? &bosses[id].boundary : NULL; }

        // Achievement criteria additional requirements check
        // NOTE: not use this if same can be checked existed requirement types from AchievementCriteriaRequirementType
        virtual bool CheckAchievementCriteriaMeet(uint32 /*criteria_id*/, Player const* /*source*/, Unit const* /*target*/ = NULL, uint32 /*miscvalue1*/ = 0);
    protected:
        void SetBossNumber(uint32 number) { bosses.resize(number); }
        void LoadDoorData(const DoorData *data);
        void LoadMinionData(const MinionData *data);

        void AddDoor(GameObject *door, bool add);
        void AddMinion(Creature *minion, bool add);

        void UpdateDoorState(GameObject *door);
        void UpdateMinionState(Creature *minion, EncounterState state);

        std::string LoadBossState(const char * data);
        std::string GetBossSaveData();
    private:
        std::vector<BossInfo> bosses;
        DoorInfoMap doors;
        MinionInfoMap minions;
};
#endif

