 /* Copyright (C) 2006 - 2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* ScriptData
SDName: Boss_Janalai
SD%Complete: 100
SDComment:
SDCategory: Zul'Aman
EndScriptData */

#include "ScriptedPch.h"
#include "zulaman.h"
#include "GridNotifiers.h"

enum eEnums
{
    SAY_AGGRO                   = -1568000,
    SAY_FIRE_BOMBS              = -1568001,
    SAY_SUMMON_HATCHER          = -1568002,
    SAY_ALL_EGGS                = -1568003,
    SAY_BERSERK                 = -1568004,
    SAY_SLAY_1                  = -1568005,
    SAY_SLAY_2                  = -1568006,
    SAY_DEATH                   = -1568007,
    SAY_EVENT_STRANGERS         = -1568008,
    SAY_EVENT_FRIENDS           = -1568009,

// Jan'alai
    SPELL_FLAME_BREATH          = 43140,
    SPELL_FIRE_WALL             = 43113,
    SPELL_ENRAGE                = 44779,
    SPELL_SUMMON_PLAYERS        = 43097,
    SPELL_TELE_TO_CENTER        = 43098, // coord
    SPELL_HATCH_ALL             = 43144,
    SPELL_BERSERK               = 45078,

// -- Fire Bob Spells
    SPELL_FIRE_BOMB_CHANNEL     = 42621, // last forever
    SPELL_FIRE_BOMB_THROW       = 42628, // throw visual
    SPELL_FIRE_BOMB_DUMMY       = 42629, // bomb visual
    SPELL_FIRE_BOMB_DAMAGE      = 42630,

// --Summons
    MOB_AMANI_HATCHER           = 23818,
    MOB_HATCHLING               = 23598,   // 42493
    MOB_EGG                     = 23817,
    MOB_FIRE_BOMB               = 23920,

// -- Hatcher Spells
    SPELL_HATCH_EGG             = 43734,   // 42471

// -- Hatchling Spells
    SPELL_FLAMEBUFFET           = 43299
};

const int area_dx = 44;
const int area_dy = 51;

float JanalainPos[1][3] =
{
    {-33.93, 1149.27, 19}
};

float FireWallCoords[4][4] =
{
    {-10.13, 1149.27, 19, 3.1415},
    {-33.93, 1123.90, 19, 0.5*3.1415},
    {-54.80, 1150.08, 19, 0},
    {-33.93, 1175.68, 19, 1.5*3.1415}
};

float hatcherway[2][5][3] =
{
    {
        {-87.46,1170.09,6},
        {-74.41,1154.75,6},
        {-52.74,1153.32,19},
        {-33.37,1172.46,19},
        {-33.09,1203.87,19}
    },
    {
        {-86.57,1132.85,6},
        {-73.94,1146.00,6},
        {-52.29,1146.51,19},
        {-33.57,1125.72,19},
        {-34.29,1095.22,19}
    }
};

struct boss_janalaiAI : public ScriptedAI
{
    boss_janalaiAI(Creature *c) : ScriptedAI(c)
    {
        pInstance =c->GetInstanceData();

        SpellEntry *TempSpell = GET_SPELL(SPELL_HATCH_EGG);
        if (TempSpell && TempSpell->EffectImplicitTargetA[0] != 1)
        {
            TempSpell->EffectImplicitTargetA[0] = 1;
            TempSpell->EffectImplicitTargetB[0] = 0;
        }
    }

    ScriptedInstance *pInstance;

    uint32 FireBreathTimer;
    uint32 BombTimer;
    uint32 BombSequenceTimer;
    uint32 BombCount;
    uint32 HatcherTimer;
    uint32 EnrageTimer;

    bool noeggs;
    bool enraged;
    bool isBombing;

    bool isFlameBreathing;

    uint64 FireBombGUIDs[40];

    void Reset()
    {
        if (pInstance)
            pInstance->SetData(DATA_JANALAIEVENT, NOT_STARTED);

        FireBreathTimer = 8000;
        BombTimer = 30000;
        BombSequenceTimer = 1000;
        BombCount = 0;
        HatcherTimer = 10000;
        EnrageTimer = MINUTE*5*IN_MILISECONDS;

        noeggs = false;
        isBombing =false;
        enraged = false;

        isFlameBreathing = false;

        for (uint8 i = 0; i < 40; ++i)
            FireBombGUIDs[i] = 0;

        HatchAllEggs(1);
    }

    void JustDied(Unit* Killer)
    {
        DoScriptText(SAY_DEATH, me);

        if (pInstance)
            pInstance->SetData(DATA_JANALAIEVENT, DONE);
    }

    void KilledUnit(Unit* victim)
    {
        DoScriptText(RAND(SAY_SLAY_1,SAY_SLAY_2), me);
    }

    void EnterCombat(Unit *who)
    {
        if (pInstance)
            pInstance->SetData(DATA_JANALAIEVENT, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, me);
//        DoZoneInCombat();
    }

    void DamageDeal(Unit *pTarget, uint32 &damage)
    {
        if (isFlameBreathing)
        {
            if (!me->HasInArc(M_PI/6, pTarget))
                damage = 0;
        }
    }

    void FireWall()
    {
        uint8 WallNum;
        Creature* wall = NULL;
        for (uint8 i = 0; i < 4; ++i)
        {
            if (i == 0 || i == 2)
                WallNum = 3;
            else
                WallNum = 2;

            for (uint8 j = 0; j < WallNum; j++)
            {
                if (WallNum == 3)
                    wall = me->SummonCreature(MOB_FIRE_BOMB, FireWallCoords[i][0],FireWallCoords[i][1]+5*(j-1),FireWallCoords[i][2],FireWallCoords[i][3],TEMPSUMMON_TIMED_DESPAWN,15000);
                else
                    wall = me->SummonCreature(MOB_FIRE_BOMB, FireWallCoords[i][0]-2+4*j,FireWallCoords[i][1],FireWallCoords[i][2],FireWallCoords[i][3],TEMPSUMMON_TIMED_DESPAWN,15000);
                if (wall) wall->CastSpell(wall, SPELL_FIRE_WALL, true);
            }
        }
    }

    void SpawnBombs()
    {
        float dx, dy;
        for (int i(0); i < 40; ++i)
        {
            dx = irand(-area_dx/2, area_dx/2);
            dy = irand(-area_dy/2, area_dy/2);

            Creature* bomb = DoSpawnCreature(MOB_FIRE_BOMB, dx, dy, 0, 0, TEMPSUMMON_TIMED_DESPAWN, 15000);
            if (bomb) FireBombGUIDs[i] = bomb->GetGUID();
        }
        BombCount = 0;
    }

    bool HatchAllEggs(uint32 uiAction) //1: reset, 2: isHatching all
    {
        std::list<Creature*> templist;
        float x, y, z;
        me->GetPosition(x, y, z);

        {
            CellPair pair(Trinity::ComputeCellPair(x, y));
            Cell cell(pair);
            cell.data.Part.reserved = ALL_DISTRICT;
            cell.SetNoCreate();

            Trinity::AllCreaturesOfEntryInRange check(me, MOB_EGG, 100);
            Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(me, templist, check);

            TypeContainerVisitor<Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange>, GridTypeMapContainer> cSearcher(searcher);

            cell.Visit(pair, cSearcher, *(me->GetMap()));
        }

        //error_log("Eggs %d at middle", templist.size());
        if (!templist.size())
            return false;

        for (std::list<Creature*>::const_iterator i = templist.begin(); i != templist.end(); ++i)
        {
            if (uiAction == 1)
               (*i)->SetDisplayId(10056);
            else if (uiAction == 2 &&(*i)->GetDisplayId() != 11686)
               (*i)->CastSpell(*i, SPELL_HATCH_EGG, false);
        }
        return true;
    }

    void Boom()
    {
        std::list<Creature*> templist;
        float x, y, z;
        me->GetPosition(x, y, z);

        {
            CellPair pair(Trinity::ComputeCellPair(x, y));
            Cell cell(pair);
            cell.data.Part.reserved = ALL_DISTRICT;
            cell.SetNoCreate();

            Trinity::AllCreaturesOfEntryInRange check(me, MOB_FIRE_BOMB, 100);
            Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(me, templist, check);

            TypeContainerVisitor<Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange>, GridTypeMapContainer> cSearcher(searcher);

            cell.Visit(pair, cSearcher, *(me->GetMap()));
        }
        for (std::list<Creature*>::const_iterator i = templist.begin(); i != templist.end(); ++i)
        {
           (*i)->CastSpell(*i, SPELL_FIRE_BOMB_DAMAGE, true);
           (*i)->RemoveAllAuras();
        }
    }

    void HandleBombSequence()
    {
        if (BombCount < 40)
        {
            if (Unit *FireBomb = Unit::GetUnit((*me), FireBombGUIDs[BombCount]))
            {
                FireBomb->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                DoCast(FireBomb, SPELL_FIRE_BOMB_THROW, true);
                FireBomb->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            }
            ++BombCount;
            if (BombCount == 40)
            {
                BombSequenceTimer = 5000;
            } else BombSequenceTimer = 100;
        }
        else
        {
            Boom();
            isBombing = false;
            BombTimer = urand(20000,40000);
            me->RemoveAurasDueToSpell(SPELL_FIRE_BOMB_CHANNEL);
            if (EnrageTimer <= 10000)
                EnrageTimer = 0;
            else
                EnrageTimer -= 10000;
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if (isFlameBreathing)
        {
            if (!me->IsNonMeleeSpellCasted(false))
                isFlameBreathing = false;
            else
                return;
        }

        if (isBombing)
        {
            if (BombSequenceTimer <= diff)
                HandleBombSequence();
            else
                BombSequenceTimer -= diff;
            return;
        }

        if (!UpdateVictim())
            return;

        //enrage if under 25% hp before 5 min.
        if (!enraged && me->GetHealth() * 4 < me->GetMaxHealth())
            EnrageTimer = 0;

        if (EnrageTimer <= diff)
        {
            if (!enraged)
            {
                DoCast(me, SPELL_ENRAGE, true);
                enraged = true;
                EnrageTimer = 300000;
            }
            else
            {
                DoScriptText(SAY_BERSERK, me);
                DoCast(me, SPELL_BERSERK, true);
                EnrageTimer = 300000;
            }
        } else EnrageTimer -= diff;

        if (BombTimer <= diff)
        {
            DoScriptText(SAY_FIRE_BOMBS, me);

            me->AttackStop();
            me->GetMotionMaster()->Clear();
            DoTeleportTo(JanalainPos[0][0],JanalainPos[0][1],JanalainPos[0][2]);
            me->StopMoving();
            DoCast(me, SPELL_FIRE_BOMB_CHANNEL, false);
            //DoTeleportPlayer(me, JanalainPos[0][0], JanalainPos[0][1],JanalainPos[0][2], 0);
            //DoCast(me, SPELL_TELE_TO_CENTER, true);

            FireWall();
            SpawnBombs();
            isBombing = true;
            BombSequenceTimer = 100;

            //Teleport every Player into the middle
            Map* pMap = me->GetMap();
            if (!pMap->IsDungeon()) return;
            Map::PlayerList const &PlayerList = pMap->GetPlayers();
            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                if (Player* i_pl = i->getSource())
                    if (i_pl->isAlive())
                        DoTeleportPlayer(i_pl, JanalainPos[0][0]-5+rand()%10, JanalainPos[0][1]-5+rand()%10, JanalainPos[0][2], 0);
            //DoCast(Temp, SPELL_SUMMON_PLAYERS, true) // core bug, spell does not work if too far
            return;
        } else BombTimer -= diff;

        if (!noeggs)
        {
            if (100 * me->GetHealth() < 35 * me->GetMaxHealth())
            {
                DoScriptText(SAY_ALL_EGGS, me);

                me->AttackStop();
                me->GetMotionMaster()->Clear();
                DoTeleportTo(JanalainPos[0][0],JanalainPos[0][1],JanalainPos[0][2]);
                me->StopMoving();
                DoCast(me, SPELL_HATCH_ALL, false);
                HatchAllEggs(2);
                noeggs = true;
            }
            else if (HatcherTimer <= diff)
            {
                if (HatchAllEggs(0))
                {
                    DoScriptText(SAY_SUMMON_HATCHER, me);
                    me->SummonCreature(MOB_AMANI_HATCHER,hatcherway[0][0][0],hatcherway[0][0][1],hatcherway[0][0][2],0,TEMPSUMMON_CORPSE_TIMED_DESPAWN,10000);
                    me->SummonCreature(MOB_AMANI_HATCHER,hatcherway[1][0][0],hatcherway[1][0][1],hatcherway[1][0][2],0,TEMPSUMMON_CORPSE_TIMED_DESPAWN,10000);
                    HatcherTimer = 90000;
                }
                else
                    noeggs = true;
            } else HatcherTimer -= diff;
        }

        EnterEvadeIfOutOfCombatArea(diff);

        DoMeleeAttackIfReady();

        if (FireBreathTimer <= diff)
        {
            if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM,0))
            {
                me->AttackStop();
                me->GetMotionMaster()->Clear();
                DoCast(pTarget, SPELL_FLAME_BREATH, false);
                me->StopMoving();
                isFlameBreathing = true;
            }
            FireBreathTimer = 8000;
        } else FireBreathTimer -= diff;
    }
};

CreatureAI* GetAI_boss_janalaiAI(Creature* pCreature)
{
    return new boss_janalaiAI(pCreature);
}

struct mob_janalai_firebombAI : public ScriptedAI
{
    mob_janalai_firebombAI(Creature *c) : ScriptedAI(c){}

    void Reset() {}

    void SpellHit(Unit *caster, const SpellEntry *spell)
    {
        if (spell->Id == SPELL_FIRE_BOMB_THROW)
            DoCast(me, SPELL_FIRE_BOMB_DUMMY, true);
    }

    void EnterCombat(Unit* who) {}

    void AttackStart(Unit* who) {}

    void MoveInLineOfSight(Unit* who) {}

    void UpdateAI(const uint32 diff) {}
};

CreatureAI* GetAI_mob_janalai_firebombAI(Creature* pCreature)
{
    return new mob_janalai_firebombAI(pCreature);
}

struct mob_amanishi_hatcherAI : public ScriptedAI
{
    mob_amanishi_hatcherAI(Creature *c) : ScriptedAI(c)
    {
        pInstance =c->GetInstanceData();
    }

    ScriptedInstance *pInstance;

    uint32 waypoint;
    uint32 HatchNum;
    uint32 WaitTimer;

    bool side;
    bool hasChangedSide;
    bool isHatching;

    void Reset()
    {
        me->AddUnitMovementFlag(MOVEMENTFLAG_WALK_MODE);
        side =(me->GetPositionY() < 1150);
        waypoint = 0;
        isHatching = false;
        hasChangedSide = false;
        WaitTimer = 1;
        HatchNum = 0;
    }

    bool HatchEggs(uint32 num)
    {
        std::list<Creature*> templist;
        float x, y, z;
        me->GetPosition(x, y, z);

        {
            CellPair pair(Trinity::ComputeCellPair(x, y));
            Cell cell(pair);
            cell.data.Part.reserved = ALL_DISTRICT;
            cell.SetNoCreate();

            Trinity::AllCreaturesOfEntryInRange check(me, 23817, 50);
            Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(me, templist, check);

            TypeContainerVisitor<Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange>, GridTypeMapContainer> cSearcher(searcher);

            cell.Visit(pair, cSearcher, *(me->GetMap()));
        }

        //error_log("Eggs %d at %d", templist.size(), side);

        for (std::list<Creature*>::const_iterator i = templist.begin(); i != templist.end() && num > 0; ++i)
            if ((*i)->GetDisplayId() != 11686)
            {
               (*i)->CastSpell(*i, SPELL_HATCH_EGG, false);
                num--;
            }

        return num == 0;   // if num == 0, no more templist
    }

    void EnterCombat(Unit* who) {}
    void AttackStart(Unit* who) {}
    void MoveInLineOfSight(Unit* who) {}
    void MovementInform(uint32, uint32)
    {
        if (waypoint == 5)
        {
            isHatching = true;
            HatchNum = 1;
            WaitTimer = 5000;
        }
        else
            WaitTimer = 1;
    }

    void UpdateAI(const uint32 diff)
    {
        if (!pInstance || !(pInstance->GetData(DATA_JANALAIEVENT) == IN_PROGRESS))
        {
            me->DisappearAndDie();
            return;
        }

        if (!isHatching)
        {
            if (WaitTimer)
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePoint(0,hatcherway[side][waypoint][0],hatcherway[side][waypoint][1],hatcherway[side][waypoint][2]);
                ++waypoint;
                WaitTimer = 0;
            }
        }
        else
        {
            if (WaitTimer <= diff)
            {
                if (HatchEggs(HatchNum))
                {
                    ++HatchNum;
                    WaitTimer = 10000;
                }
                else if (!hasChangedSide)
                {
                    side = side ? 0 : 1;
                    isHatching = false;
                    waypoint = 3;
                    WaitTimer = 1;
                    hasChangedSide = true;
                }
                else
                    me->DisappearAndDie();

            } else WaitTimer -= diff;
        }
    }
};

CreatureAI* GetAI_mob_amanishi_hatcherAI(Creature* pCreature)
{
    return new mob_amanishi_hatcherAI(pCreature);
}

struct mob_hatchlingAI : public ScriptedAI
{
    mob_hatchlingAI(Creature *c) : ScriptedAI(c)
    {
        pInstance =c->GetInstanceData();
    }

    ScriptedInstance *pInstance;
    uint32 BuffetTimer;

    void Reset()
    {
        BuffetTimer = 7000;
        if (me->GetPositionY() > 1150)
            me->GetMotionMaster()->MovePoint(0, hatcherway[0][3][0]+rand()%4-2,1150+rand()%4-2,hatcherway[0][3][2]);
        else
            me->GetMotionMaster()->MovePoint(0,hatcherway[1][3][0]+rand()%4-2,1150+rand()%4-2,hatcherway[1][3][2]);

        me->SetUnitMovementFlags(MOVEMENTFLAG_LEVITATING);
    }

    void EnterCombat(Unit *who) {/*DoZoneInCombat();*/}

    void UpdateAI(const uint32 diff)
    {
        if (!pInstance || !(pInstance->GetData(DATA_JANALAIEVENT) == IN_PROGRESS))
        {
            me->DisappearAndDie();
            return;
        }

        if (!UpdateVictim())
            return;

        if (BuffetTimer <= diff)
        {
            DoCast(me->getVictim(), SPELL_FLAMEBUFFET, false);
            BuffetTimer = 10000;
        } else BuffetTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_hatchlingAI(Creature* pCreature)
{
    return new mob_hatchlingAI(pCreature);
}

struct mob_eggAI : public ScriptedAI
{
    mob_eggAI(Creature *c) : ScriptedAI(c){}
    void Reset() {}
    void EnterCombat(Unit* who) {}
    void AttackStart(Unit* who) {}
    void MoveInLineOfSight(Unit* who) {}
    void UpdateAI(const uint32 diff) {}

    void SpellHit(Unit *caster, const SpellEntry *spell)
    {
        if (spell->Id == SPELL_HATCH_EGG)
        {
            DoSpawnCreature(MOB_HATCHLING, 0, 0, 0, 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 60000);
            me->SetDisplayId(11686);
        }
    }
};

CreatureAI* GetAI_mob_eggAI(Creature* pCreature)
{
    return new mob_eggAI(pCreature);
}

void AddSC_boss_janalai()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_janalai";
    newscript->GetAI = &GetAI_boss_janalaiAI;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_janalai_firebomb";
    newscript->GetAI = &GetAI_mob_janalai_firebombAI;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_janalai_hatcher";
    newscript->GetAI = &GetAI_mob_amanishi_hatcherAI;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_janalai_hatchling";
    newscript->GetAI = &GetAI_mob_hatchlingAI;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_janalai_egg";
    newscript->GetAI = &GetAI_mob_eggAI;
    newscript->RegisterSelf();
}

