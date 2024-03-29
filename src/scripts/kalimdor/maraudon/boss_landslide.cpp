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
SDName: Boss_Landslide
SD%Complete: 100
SDComment:
SDCategory: Maraudon
EndScriptData */

#include "ScriptedPch.h"

#define SPELL_KNOCKAWAY         18670
#define SPELL_TRAMPLE           5568
#define SPELL_LANDSLIDE         21808

struct boss_landslideAI : public ScriptedAI
{
    boss_landslideAI(Creature *c) : ScriptedAI(c) {}

    uint32 KnockAway_Timer;
    uint32 Trample_Timer;
    uint32 Landslide_Timer;

    void Reset()
    {
        KnockAway_Timer = 8000;
        Trample_Timer = 2000;
        Landslide_Timer = 0;
    }

    void EnterCombat(Unit *who)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        //KnockAway_Timer
        if (KnockAway_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_KNOCKAWAY);
            KnockAway_Timer = 15000;
        } else KnockAway_Timer -= diff;

        //Trample_Timer
        if (Trample_Timer <= diff)
        {
            DoCast(me, SPELL_TRAMPLE);
            Trample_Timer = 8000;
        } else Trample_Timer -= diff;

        //Landslide
        if (me->GetHealth()*100 / me->GetMaxHealth() < 50)
        {
            if (Landslide_Timer <= diff)
            {
                me->InterruptNonMeleeSpells(false);
                DoCast(me, SPELL_LANDSLIDE);
                Landslide_Timer = 60000;
            } else Landslide_Timer -= diff;
        }

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_landslide(Creature* pCreature)
{
    return new boss_landslideAI (pCreature);
}

void AddSC_boss_landslide()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_landslide";
    newscript->GetAI = &GetAI_boss_landslide;
    newscript->RegisterSelf();
}

