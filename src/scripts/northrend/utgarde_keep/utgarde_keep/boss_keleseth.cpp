/*
 * Copyright (C) 2009 Trinity <http://www.trinitycore.org/>
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

/* ScriptData
SDName: Boss_Prince_Keleseth
SD%Complete: 90
SDComment:  Needs Prince Movements, Needs adjustments to blizzlike timers, Needs Shadowbolt castbar, Needs right Ressurect Visual, Needs Some Heroic Spells
SDCategory: Utgarde Keep
EndScriptData */

#include "ScriptedPch.h"
#include "utgarde_keep.h"

enum eEnums
{
    ACHIEVEMENT_ON_THE_ROCKS                 = 1919,

    SPELL_SHADOWBOLT                         = 43667,
    SPELL_SHADOWBOLT_HEROIC                  = 59389,
    SPELL_FROST_TOMB                         = 48400,
    SPELL_FROST_TOMB_SUMMON                  = 42714,
    SPELL_DECREPIFY                          = 42702,
    SPELL_SCOURGE_RESSURRECTION              = 42704,
    CREATURE_FROSTTOMB                       = 23965,
    CREATURE_SKELETON                        = 23970,

    SAY_AGGRO                                = -1574000,
    SAY_FROST_TOMB                           = -1574001,
    SAY_SKELETONS                            = -1574002,
    SAY_KILL                                 = -1574003,
    SAY_DEATH                                = -1574004
};

#define SKELETONSPAWN_Z                          42.8668

float SkeletonSpawnPoint[5][5]=
{
    {156.2559, 259.2093},
    {156.2559, 259.2093},
    {156.2559, 259.2093},
    {156.2559, 259.2093},
    {156.2559, 259.2093},
};

float AttackLoc[3]={197.636, 194.046, 40.8164};

bool ShatterFrostTomb; // needed for achievement: On The Rocks(1919)

struct mob_frost_tombAI : public ScriptedAI
{
    mob_frost_tombAI(Creature *c) : ScriptedAI(c)
    {
        FrostTombGUID = 0;
    }

    uint64 FrostTombGUID;

    void SetPrisoner(Unit* uPrisoner)
    {
        FrostTombGUID = uPrisoner->GetGUID();
    }

    void Reset(){ FrostTombGUID = 0; }
    void EnterCombat(Unit* who) {}
    void AttackStart(Unit* who) {}
    void MoveInLineOfSight(Unit* who) {}

    void JustDied(Unit *killer)
    {
        if (killer->GetGUID() != me->GetGUID())
            ShatterFrostTomb = true;

        if (FrostTombGUID)
        {
            Unit* FrostTomb = Unit::GetUnit((*me),FrostTombGUID);
            if (FrostTomb)
                FrostTomb->RemoveAurasDueToSpell(SPELL_FROST_TOMB);
        }
    }

    void UpdateAI(const uint32 diff)
    {
        Unit* temp = Unit::GetUnit((*me),FrostTombGUID);
        if ((temp && temp->isAlive() && !temp->HasAura(SPELL_FROST_TOMB)) || !temp)
            me->DealDamage(me, me->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    }
};

struct boss_kelesethAI : public ScriptedAI
{
    boss_kelesethAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance* pInstance;

    uint32 FrostTombTimer;
    uint32 SummonSkeletonsTimer;
    uint32 RespawnSkeletonsTimer;
    uint32 ShadowboltTimer;
    uint64 SkeletonGUID[5];
    bool Skeletons;
    bool RespawnSkeletons;

    void Reset()
    {
        ShadowboltTimer = 0;
        Skeletons = false;

        ShatterFrostTomb = false;

        ResetTimer();

        if (pInstance)
            pInstance->SetData(DATA_PRINCEKELESETH_EVENT, NOT_STARTED);
    }

    void KilledUnit(Unit *victim)
    {
        if (victim == me)
            return;

        DoScriptText(SAY_KILL, me);
    }

    void JustDied(Unit* killer)
    {
        DoScriptText(SAY_DEATH, me);

        if (IsHeroic() && !ShatterFrostTomb)
        {
            AchievementEntry const *AchievOnTheRocks = GetAchievementStore()->LookupEntry(ACHIEVEMENT_ON_THE_ROCKS);
            if (AchievOnTheRocks)
            {
                Map* pMap = me->GetMap();
                if (pMap && pMap->IsDungeon())
                {
                    Map::PlayerList const &players = pMap->GetPlayers();
                    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                        itr->getSource()->CompletedAchievement(AchievOnTheRocks);
                }
            }
        }

        if (pInstance)
            pInstance->SetData(DATA_PRINCEKELESETH_EVENT, DONE);
    }

    void EnterCombat(Unit* who)
    {
        DoScriptText(SAY_AGGRO, me);
        DoZoneInCombat();

        if (pInstance)
            pInstance->SetData(DATA_PRINCEKELESETH_EVENT, IN_PROGRESS);
    }

    void ResetTimer(uint32 inc = 0)
    {
        SummonSkeletonsTimer = 5000 + inc;
        FrostTombTimer = 28000 + inc;
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (ShadowboltTimer <= diff)
        {
            Unit *pTarget = SelectUnit(SELECT_TARGET_TOPAGGRO, 0);
            if (pTarget && pTarget->isAlive() && pTarget->GetTypeId() == TYPEID_PLAYER)
                me->CastSpell(pTarget, DUNGEON_MODE(SPELL_SHADOWBOLT, SPELL_SHADOWBOLT_HEROIC), true);
            ShadowboltTimer = 10000;
        } else ShadowboltTimer -= diff;

        if (!Skeletons)
            if ((SummonSkeletonsTimer <= diff))
            {
                Creature* Skeleton;
                DoScriptText(SAY_SKELETONS, me);
                for (uint8 i = 0; i < 5; ++i)
                {
                    if (Skeleton = me->SummonCreature(CREATURE_SKELETON, SkeletonSpawnPoint[i][0], SkeletonSpawnPoint[i][1] , SKELETONSPAWN_Z, 0, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000))
                    {
                        Skeleton->RemoveUnitMovementFlag(MOVEMENTFLAG_WALK_MODE);
                        Skeleton->GetMotionMaster()->MovePoint(0, me->GetPositionX(), me->GetPositionY() , me->GetPositionZ());
                        Skeleton->AddThreat(me->getVictim(), 0.0f);
                        DoZoneInCombat(Skeleton);
                    }
                }
                Skeletons = true;
            } else SummonSkeletonsTimer -= diff;

        if (FrostTombTimer <= diff)
        {
            if (Unit *pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                if (pTarget->isAlive())
                {
                    //DoCast(pTarget, SPELL_FROST_TOMB_SUMMON, true);
                    if (Creature *pChains = me->SummonCreature(CREATURE_FROSTTOMB, pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 20000))
                    {
                        CAST_AI(mob_frost_tombAI, pChains->AI())->SetPrisoner(pTarget);
                        pChains->CastSpell(pTarget, SPELL_FROST_TOMB, true);

                        DoScriptText(SAY_FROST_TOMB, me);
                    }
                }
            FrostTombTimer = 15000;
        } else FrostTombTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

struct mob_vrykul_skeletonAI : public ScriptedAI
{
    mob_vrykul_skeletonAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance *pInstance;
    uint32 Respawn_Time;
    uint64 Target_Guid;
    uint32 Decrepify_Timer;

    bool isDead;

    void Reset()
    {
        Respawn_Time = 12000;
        Decrepify_Timer = urand(10000,20000);
        isDead = false;
    }

    void EnterCombat(Unit *who){}
    void DamageTaken(Unit *done_by, uint32 &damage)
    {
        if (done_by->GetGUID() == me->GetGUID())
            return;

        if (damage >= me->GetHealth())
        {
            PretendToDie();
            damage = 0;
        }
    }

    void PretendToDie()
    {
        isDead = true;
        me->InterruptNonMeleeSpells(true);
        me->RemoveAllAuras();
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->GetMotionMaster()->MovementExpired(false);
        me->GetMotionMaster()->MoveIdle();
        me->SetStandState(UNIT_STAND_STATE_DEAD);
    };

    void Resurrect()
    {
        isDead = false;
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        DoCast(me, SPELL_SCOURGE_RESSURRECTION, true);

        if (me->getVictim())
        {
            me->GetMotionMaster()->MoveChase(me->getVictim());
            me->AI()->AttackStart(me->getVictim());
        }
        else
            me->GetMotionMaster()->Initialize();
    };

    void UpdateAI(const uint32 diff)
    {
        if (pInstance && pInstance->GetData(DATA_PRINCEKELESETH_EVENT) == IN_PROGRESS)
        {
            if (isDead)
            {
                if (Respawn_Time <= diff)
                {
                    Resurrect();
                    Respawn_Time = 12000;
                } else Respawn_Time -= diff;
            }
            else
            {
                if (!UpdateVictim())
                    return;

                if (Decrepify_Timer <= diff)
                {
                    DoCast(me->getVictim(), SPELL_DECREPIFY);
                    Decrepify_Timer = 30000;
                } else Decrepify_Timer -= diff;

                DoMeleeAttackIfReady();
            }
        }else
        {
            if (me->isAlive())
                me->DealDamage(me, me->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
        }

    }
};

CreatureAI* GetAI_mob_frost_tomb(Creature* pCreature)
{
    return new mob_frost_tombAI(pCreature);
}

CreatureAI* GetAI_boss_keleseth(Creature* pCreature)
{
    return new boss_kelesethAI (pCreature);
}

CreatureAI* GetAI_mob_vrykul_skeleton(Creature* pCreature)
{
    return new mob_vrykul_skeletonAI (pCreature);
}

void AddSC_boss_keleseth()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "boss_keleseth";
    newscript->GetAI = &GetAI_boss_keleseth;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_frost_tomb";
    newscript->GetAI = &GetAI_mob_frost_tomb;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_vrykul_skeleton";
    newscript->GetAI = &GetAI_mob_vrykul_skeleton;
    newscript->RegisterSelf();
}
