/* Copyright (C) 2006 - 2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
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
SDName: Boss_Nethermancer_Sepethrea
SD%Complete: 90
SDComment: Need adjustments to initial summons
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

#include "ScriptedPch.h"
#include "mechanar.h"

#define SAY_AGGRO                       -1554013
#define SAY_SUMMON                      -1554014
#define SAY_DRAGONS_BREATH_1            -1554015
#define SAY_DRAGONS_BREATH_2            -1554016
#define SAY_SLAY1                       -1554017
#define SAY_SLAY2                       -1554018
#define SAY_DEATH                       -1554019

#define SPELL_SUMMON_RAGIN_FLAMES       35275
#define H_SPELL_SUMMON_RAGIN_FLAMES     39084

#define SPELL_FROST_ATTACK              35263
#define SPELL_ARCANE_BLAST              35314
#define SPELL_DRAGONS_BREATH            35250
#define SPELL_KNOCKBACK                 37317
#define SPELL_SOLARBURN                 35267

struct boss_nethermancer_sepethreaAI : public ScriptedAI
{
    boss_nethermancer_sepethreaAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance *pInstance;

    uint32 frost_attack_Timer;
    uint32 arcane_blast_Timer;
    uint32 dragons_breath_Timer;
    uint32 knockback_Timer;
    uint32 solarburn_Timer;

    void Reset()
    {
        frost_attack_Timer = 7000 + rand()%3000;
        arcane_blast_Timer = 12000 + rand()%6000;
        dragons_breath_Timer = 18000 + rand()%4000;
        knockback_Timer = 22000 + rand()%6000;
        solarburn_Timer = 30000;

        if (pInstance)
            pInstance->SetData(DATA_NETHERMANCER_EVENT, NOT_STARTED);
    }

    void EnterCombat(Unit *who)
    {
        if (pInstance)
            pInstance->SetData(DATA_NETHERMANCER_EVENT, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, me);
        DoCast(who, SPELL_SUMMON_RAGIN_FLAMES);
        DoScriptText(SAY_SUMMON, me);
    }

    void KilledUnit(Unit* victim)
    {
        DoScriptText(RAND(SAY_SLAY1,SAY_SLAY2), me);
    }

    void JustDied(Unit* Killer)
    {
        DoScriptText(SAY_DEATH, me);

        if (pInstance)
            pInstance->SetData(DATA_NETHERMANCER_EVENT, DONE);
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        //Frost Attack
        if (frost_attack_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_FROST_ATTACK);
            frost_attack_Timer = 7000 + rand()%3000;
        } else frost_attack_Timer -= diff;

        //Arcane Blast
        if (arcane_blast_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_ARCANE_BLAST);
            arcane_blast_Timer = 15000;
        } else arcane_blast_Timer -= diff;

        //Dragons Breath
        if (dragons_breath_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_DRAGONS_BREATH);
            {
                if (rand()%2)
                    return;

                DoScriptText(RAND(SAY_DRAGONS_BREATH_1,SAY_DRAGONS_BREATH_2), me);
            }
            dragons_breath_Timer = 12000 + rand()%10000;
        } else dragons_breath_Timer -= diff;

        //Knockback
        if (knockback_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_KNOCKBACK);
            knockback_Timer = 15000 + rand()%10000;
        } else knockback_Timer -= diff;

        //Solarburn
        if (solarburn_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_SOLARBURN);
            solarburn_Timer = 30000;
        } else solarburn_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_nethermancer_sepethrea(Creature* pCreature)
{
    return new boss_nethermancer_sepethreaAI (pCreature);
}

#define SPELL_INFERNO                   35268
#define H_SPELL_INFERNO                 39346
#define SPELL_FIRE_TAIL                 35278

struct mob_ragin_flamesAI : public ScriptedAI
{
    mob_ragin_flamesAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance *pInstance;

    uint32 inferno_Timer;
    uint32 flame_timer;
    uint32 Check_Timer;

    bool onlyonce;

    void Reset()
    {
        inferno_Timer = 10000;
        flame_timer = 500;
        Check_Timer = 2000;
        onlyonce = false;
        me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_MAGIC, true);
        me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, true);
        me->SetSpeed(MOVE_RUN, DUNGEON_MODE(0.5f, 0.7f));
    }

    void EnterCombat(Unit* who)
    {
    }

    void UpdateAI(const uint32 diff)
    {
         //Check_Timer
        if (Check_Timer <= diff)
        {
            if (pInstance)
            {
                if (pInstance->GetData(DATA_NETHERMANCER_EVENT) != IN_PROGRESS)
                {
                    //remove
                    me->setDeathState(JUST_DIED);
                    me->RemoveCorpse();
                }
            }
            Check_Timer = 1000;
        } else Check_Timer -= diff;

        if (!UpdateVictim())
            return;

        if (!onlyonce)
        {
            if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM,0))
                me->GetMotionMaster()->MoveChase(pTarget);
            onlyonce = true;
        }

        if (inferno_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_INFERNO);
            me->TauntApply(me->getVictim());
            inferno_Timer = 10000;
        } else inferno_Timer -= diff;

        if (flame_timer <= diff)
        {
            DoCast(me, SPELL_FIRE_TAIL);
            flame_timer = 500;
        } else flame_timer -=diff;

        DoMeleeAttackIfReady();
    }

};
CreatureAI* GetAI_mob_ragin_flames(Creature* pCreature)
{
    return new mob_ragin_flamesAI (pCreature);
}
void AddSC_boss_nethermancer_sepethrea()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_nethermancer_sepethrea";
    newscript->GetAI = &GetAI_boss_nethermancer_sepethrea;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_ragin_flames";
    newscript->GetAI = &GetAI_mob_ragin_flames;
    newscript->RegisterSelf();
}

