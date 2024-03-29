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
SDName: Boss_Ayamiss
SD%Complete: 50
SDComment: VERIFY SCRIPT
SDCategory: Ruins of Ahn'Qiraj
EndScriptData */

#include "ScriptedPch.h"
#include "ruins_of_ahnqiraj.h"

/*
To do:
make him fly from 70-100%
*/

enum Spells
{
    SPELL_STINGERSPRAY                 = 25749,
    SPELL_POISONSTINGER                = 25748,  //only used in phase1
    SPELL_SUMMONSWARMER                = 25844,  //might be 25708
    SPELL_PARALYZE                     = 23414   //doesnt work correct (core)
};

struct boss_ayamissAI : public ScriptedAI
{
    boss_ayamissAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    uint32 STINGERSPRAY_Timer;
    uint32 POISONSTINGER_Timer;
    uint32 SUMMONSWARMER_Timer;
    uint32 phase;

    ScriptedInstance *pInstance;

    void Reset()
    {
        STINGERSPRAY_Timer = 30000;
        POISONSTINGER_Timer = 30000;
        SUMMONSWARMER_Timer = 60000;
        phase=1;

        if (pInstance)
            pInstance->SetData(DATA_AYAMISS_EVENT, NOT_STARTED);
    }

    void EnterCombat(Unit *who)
    {
        if (pInstance)
            pInstance->SetData(DATA_AYAMISS_EVENT, IN_PROGRESS);
    }

    void JustDied(Unit *killer)
    {
        if (pInstance)
            pInstance->SetData(DATA_AYAMISS_EVENT, DONE);
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        //If he is 70% start phase 2
        if (phase == 1 && me->GetHealth()*100 / me->GetMaxHealth() <= 70 && !me->IsNonMeleeSpellCasted(false))
        {
            phase=2;
        }

        //STINGERSPRAY_Timer (only in phase2)
        if (phase == 2 && STINGERSPRAY_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_STINGERSPRAY);
            STINGERSPRAY_Timer = 30000;
        } else STINGERSPRAY_Timer -= diff;

        //POISONSTINGER_Timer (only in phase1)
        if (phase == 1 && POISONSTINGER_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_POISONSTINGER);
            POISONSTINGER_Timer = 30000;
        } else POISONSTINGER_Timer -= diff;

        //SUMMONSWARMER_Timer (only in phase1)
        if (SUMMONSWARMER_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_SUMMONSWARMER);
            SUMMONSWARMER_Timer = 60000;
        } else SUMMONSWARMER_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_ayamiss(Creature* pCreature)
{
    return new boss_ayamissAI (pCreature);
}

void AddSC_boss_ayamiss()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_ayamiss";
    newscript->GetAI = &GetAI_boss_ayamiss;
    newscript->RegisterSelf();
}

