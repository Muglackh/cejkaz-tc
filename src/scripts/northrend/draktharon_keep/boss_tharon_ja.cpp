/*
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

#include "ScriptedPch.h"
#include "drak_tharon_keep.h"

enum Spells
{
    //skeletal spells (phase 1)
    SPELL_CURSE_OF_LIFE                           = 49527,
    H_SPELL_CURSE_OF_LIFE                         = 59972,
    SPELL_RAIN_OF_FIRE                            = 49518,
    H_SPELL_RAIN_OF_FIRE                          = 59971,
    SPELL_SHADOW_VOLLEY                           = 49528,
    H_SPELL_SHADOW_VOLLEY                         = 59973,
    SPELL_DECAY_FLESH                             = 49356, //casted at end of phase 1, starts phase 2
    //flesh spells (phase 2)
    SPELL_GIFT_OF_THARON_JA                       = 52509,
    SPELL_EYE_BEAM                                = 49544,
    H_SPELL_EYE_BEAM                              = 59965,
    SPELL_LIGHTNING_BREATH                        = 49537,
    H_SPELL_LIGHTNING_BREATH                      = 59963,
    SPELL_POISON_CLOUD                            = 49548,
    H_SPELL_POISON_CLOUD                          = 59969,
    SPELL_RETURN_FLESH                            = 53463, //Channeled spell ending phase two and returning to phase 1. This ability will stun the party for 6 seconds.
    SPELL_ACHIEVEMENT_CHECK                       = 61863,
};

enum Yells
{
    SAY_AGGRO                                     = -1600011,
    SAY_KILL_1                                    = -1600012,
    SAY_KILL_2                                    = -1600013,
    SAY_FLESH_1                                   = -1600014,
    SAY_FLESH_2                                   = -1600015,
    SAY_SKELETON_1                                = -1600016,
    SAY_SKELETON_2                                = -1600017,
    SAY_DEATH                                     = -1600018
};
enum Models
{
    MODEL_FLESH                                   = 27073,
    MODEL_SKELETON                                = 27511
};
enum CombatPhase
{
    SKELETAL,
    GOING_FLESH,
    FLESH,
    GOING_SKELETAL
};

struct boss_tharon_jaAI : public ScriptedAI
{
    boss_tharon_jaAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    uint32 uiPhaseTimer;
    uint32 uiCurseOfLifeTimer;
    uint32 uiRainOfFireTimer;
    uint32 uiShadowVolleyTimer;
    uint32 uiEyeBeamTimer;
    uint32 uiLightningBreathTimer;
    uint32 uiPoisonCloudTimer;

    CombatPhase Phase;

    ScriptedInstance* pInstance;

    void Reset()
    {
        uiPhaseTimer = 20*IN_MILISECONDS;
        uiCurseOfLifeTimer = 1*IN_MILISECONDS;
        uiRainOfFireTimer = urand(14*IN_MILISECONDS,18*IN_MILISECONDS);
        uiShadowVolleyTimer = urand(8*IN_MILISECONDS,10*IN_MILISECONDS);
        Phase = SKELETAL;
        me->SetDisplayId(me->GetNativeDisplayId());
        if (pInstance)
            pInstance->SetData(DATA_THARON_JA_EVENT, NOT_STARTED);
    }

    void EnterCombat(Unit* who)
    {
        DoScriptText(SAY_AGGRO, me);

        if (pInstance)
            pInstance->SetData(DATA_THARON_JA_EVENT, IN_PROGRESS);
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        switch (Phase)
        {
            case SKELETAL:
                if (uiCurseOfLifeTimer < diff)
                {
                    if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                        DoCast(pTarget, SPELL_CURSE_OF_LIFE);
                    uiCurseOfLifeTimer = urand(10*IN_MILISECONDS,15*IN_MILISECONDS);
                } else uiCurseOfLifeTimer -= diff;

                if (uiShadowVolleyTimer < diff)
                {
                    DoCastVictim(SPELL_SHADOW_VOLLEY);
                    uiShadowVolleyTimer = urand(8*IN_MILISECONDS,10*IN_MILISECONDS);
                } else uiShadowVolleyTimer -= diff;

                if (uiRainOfFireTimer < diff)
                {
                    DoCastAOE(SPELL_RAIN_OF_FIRE);
                    uiRainOfFireTimer = urand(14*IN_MILISECONDS,18*IN_MILISECONDS);
                } else uiRainOfFireTimer -= diff;

                if (uiPhaseTimer < diff)
                {
                    DoCast(SPELL_DECAY_FLESH);
                    Phase = GOING_FLESH;
                    uiPhaseTimer = 6*IN_MILISECONDS;
                } else uiPhaseTimer -= diff;

                DoMeleeAttackIfReady();
                break;
            case GOING_FLESH:
                if (uiPhaseTimer < diff)
                {
                    DoScriptText(RAND(SAY_FLESH_1,SAY_FLESH_2),me);
                    me->SetDisplayId(MODEL_FLESH);
                    std::list<HostileReference*>& threatlist = me->getThreatManager().getThreatList();
                    for (std::list<HostileReference*>::const_iterator itr = threatlist.begin(); itr != threatlist.end(); ++itr)
                    {
                        Unit *pTemp = Unit::GetUnit((*me),(*itr)->getUnitGuid());
                        if (pTemp && pTemp->GetTypeId() == TYPEID_PLAYER)
                        {
                            me->AddAura(SPELL_GIFT_OF_THARON_JA,pTemp);
                            pTemp->SetDisplayId(MODEL_SKELETON);
                        }
                    }
                    uiPhaseTimer = 20*IN_MILISECONDS;
                    uiLightningBreathTimer = urand(3*IN_MILISECONDS,4*IN_MILISECONDS);
                    uiEyeBeamTimer = urand(4*IN_MILISECONDS,8*IN_MILISECONDS);
                    uiPoisonCloudTimer = urand(6*IN_MILISECONDS,7*IN_MILISECONDS);
                    Phase = FLESH;
                } else uiPhaseTimer -= diff;
                break;
            case FLESH:
                if (uiLightningBreathTimer < diff)
                {
                    if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                        DoCast(pTarget, SPELL_LIGHTNING_BREATH);
                    uiLightningBreathTimer = urand(6*IN_MILISECONDS,7*IN_MILISECONDS);
                } else uiLightningBreathTimer -= diff;

                if (uiEyeBeamTimer < diff)
                {
                    if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                        DoCast(pTarget, SPELL_EYE_BEAM);
                    uiEyeBeamTimer = urand(4*IN_MILISECONDS,6*IN_MILISECONDS);
                } else uiEyeBeamTimer -= diff;

                if (uiPoisonCloudTimer < diff)
                {
                    DoCastAOE(SPELL_POISON_CLOUD);
                    uiPoisonCloudTimer = urand(10*IN_MILISECONDS,12*IN_MILISECONDS);
                } else uiPoisonCloudTimer -= diff;

                if (uiPhaseTimer < diff)
                {
                    DoCast(SPELL_RETURN_FLESH);
                    Phase = GOING_SKELETAL;
                    uiPhaseTimer = 6*IN_MILISECONDS;
                } else uiPhaseTimer -= diff;
                DoMeleeAttackIfReady();
                break;
            case GOING_SKELETAL:
                if (uiPhaseTimer < diff)
                {
                    DoScriptText(RAND(SAY_SKELETON_1,SAY_SKELETON_2), me);
                    me->DeMorph();
                    Phase = SKELETAL;
                    uiPhaseTimer = 20*IN_MILISECONDS;
                    uiCurseOfLifeTimer = 1*IN_MILISECONDS;
                    uiRainOfFireTimer = urand(14*IN_MILISECONDS,18*IN_MILISECONDS);
                    uiShadowVolleyTimer = urand(8*IN_MILISECONDS,10*IN_MILISECONDS);
                    std::list<HostileReference*>& threatlist = me->getThreatManager().getThreatList();
                    for (std::list<HostileReference*>::const_iterator itr = threatlist.begin(); itr != threatlist.end(); ++itr)
                    {
                        Unit *pTemp = Unit::GetUnit((*me),(*itr)->getUnitGuid());
                        if (pTemp && pTemp->GetTypeId() == TYPEID_PLAYER)
                        {
                            if (pTemp->HasAura(SPELL_GIFT_OF_THARON_JA))
                                pTemp->RemoveAura(SPELL_GIFT_OF_THARON_JA);
                            pTemp->DeMorph();
                        }
                    }
                } else uiPhaseTimer -= diff;
                break;
        }
    }

    void KilledUnit(Unit *victim)
    {
        DoScriptText(RAND(SAY_KILL_1,SAY_KILL_2),me);
    }

    void JustDied(Unit* killer)
    {
        DoScriptText(SAY_DEATH,me);

        if (pInstance)
        {
            // cast is not rewarding the achievement.
            // DoCast(SPELL_ACHIEVEMENT_CHECK);
            pInstance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2, SPELL_ACHIEVEMENT_CHECK);

            pInstance->SetData(DATA_THARON_JA_EVENT, DONE);
        }
    }
};

CreatureAI* GetAI_boss_tharon_ja(Creature* pCreature)
{
    return new boss_tharon_jaAI (pCreature);
}

void AddSC_boss_tharon_ja()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "boss_tharon_ja";
    newscript->GetAI = &GetAI_boss_tharon_ja;
    newscript->RegisterSelf();
}
