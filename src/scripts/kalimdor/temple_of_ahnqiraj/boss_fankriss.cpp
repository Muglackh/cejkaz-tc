/* Copyright (C) 2006 - 2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
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
SDName: Boss_Fankriss
SD%Complete: 100
SDComment: sound not implemented
SDCategory: Temple of Ahn'Qiraj
EndScriptData */

#include "ScriptedPch.h"

#define SOUND_SENTENCE_YOU 8588
#define SOUND_SERVE_TO     8589
#define SOUND_LAWS         8590
#define SOUND_TRESPASS     8591
#define SOUND_WILL_BE      8592

#define SPELL_MORTAL_WOUND 28467
#define SPELL_ROOT         28858

// Enrage for his spawns
#define SPELL_ENRAGE       28798

struct boss_fankrissAI : public ScriptedAI
{
    boss_fankrissAI(Creature *c) : ScriptedAI(c) {}

    uint32 MortalWound_Timer;
    uint32 SpawnHatchlings_Timer;
    uint32 SpawnSpawns_Timer;
    int Rand;
    int RandX;
    int RandY;

    Creature* Hatchling;
    Creature* Spawn;

    void Reset()
    {
        MortalWound_Timer = 10000 + rand()%5000;
        SpawnHatchlings_Timer = 6000 + rand()%6000;
        SpawnSpawns_Timer = 15000 + rand()%30000;
    }

    void SummonSpawn(Unit* victim)
    {
        if (!victim)
            return;

        Rand = 10 + (rand()%10);
        switch (rand()%2)
        {
            case 0: RandX = 0 - Rand; break;
            case 1: RandX = 0 + Rand; break;
        }
        Rand = 0;
        Rand =  10 + (rand()%10);
        switch (rand()%2)
        {
            case 0: RandY = 0 - Rand; break;
            case 1: RandY = 0 + Rand; break;
        }
        Rand = 0;
        Spawn = DoSpawnCreature(15630, RandX, RandY, 0, 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30000);
        if (Spawn)
            Spawn->AI()->AttackStart(victim);
    }

    void EnterCombat(Unit *who)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        //MortalWound_Timer
        if (MortalWound_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_MORTAL_WOUND);
            MortalWound_Timer = 10000 + rand()%10000;
        } else MortalWound_Timer -= diff;

        //Summon 1-3 Spawns of Fankriss at random time.
        if (SpawnSpawns_Timer <= diff)
        {
            switch (urand(0,2))
            {
                case 0:
                    SummonSpawn(SelectUnit(SELECT_TARGET_RANDOM,0));
                    break;
                case 1:
                    SummonSpawn(SelectUnit(SELECT_TARGET_RANDOM,0));
                    SummonSpawn(SelectUnit(SELECT_TARGET_RANDOM,0));
                    break;
                case 2:
                    SummonSpawn(SelectUnit(SELECT_TARGET_RANDOM,0));
                    SummonSpawn(SelectUnit(SELECT_TARGET_RANDOM,0));
                    SummonSpawn(SelectUnit(SELECT_TARGET_RANDOM,0));
                    break;
            }
            SpawnSpawns_Timer = 30000 + rand()%30000;
        } else SpawnSpawns_Timer -= diff;

        // Teleporting Random Target to one of the three tunnels and spawn 4 hatchlings near the gamer.
        //We will only telport if fankriss has more than 3% of hp so teleported gamers can always loot.
        if (me->GetHealth()*100 / me->GetMaxHealth() > 3)
        {
            if (SpawnHatchlings_Timer <= diff)
            {
                Unit *pTarget = NULL;
                pTarget = SelectUnit(SELECT_TARGET_RANDOM,0);
                if (pTarget && pTarget->GetTypeId() == TYPEID_PLAYER)
                {
                    DoCast(pTarget, SPELL_ROOT);

                    if (DoGetThreat(pTarget))
                        DoModifyThreatPercent(pTarget, -100);

                    switch(urand(0,2))
                    {
                        case 0:
                            DoTeleportPlayer(pTarget, -8106.0142,1289.2900,-74.419533,5.112);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-3, pTarget->GetPositionY()-3, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-3, pTarget->GetPositionY()+3, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-5, pTarget->GetPositionY()-5, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-5, pTarget->GetPositionY()+5, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            break;
                        case 1:
                            DoTeleportPlayer(pTarget, -7990.135354,1155.1907,-78.849319,2.608);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-3, pTarget->GetPositionY()-3, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-3, pTarget->GetPositionY()+3, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-5, pTarget->GetPositionY()-5, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-5, pTarget->GetPositionY()+5, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            break;
                        case 2:
                            DoTeleportPlayer(pTarget,-8159.7753,1127.9064,-76.868660,0.675);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-3, pTarget->GetPositionY()-3, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-3, pTarget->GetPositionY()+3, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-5, pTarget->GetPositionY()-5, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            Hatchling = me->SummonCreature(15962, pTarget->GetPositionX()-5, pTarget->GetPositionY()+5, pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                            if (Hatchling)
                                Hatchling->AI()->AttackStart(pTarget);
                            break;
                    }
                }
                SpawnHatchlings_Timer = 45000 + rand()%15000;
            } else SpawnHatchlings_Timer -= diff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_fankriss(Creature* pCreature)
{
    return new boss_fankrissAI (pCreature);
}

void AddSC_boss_fankriss()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_fankriss";
    newscript->GetAI = &GetAI_boss_fankriss;
    newscript->RegisterSelf();
}

