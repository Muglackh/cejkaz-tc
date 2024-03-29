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
SDName: Boss_NexusPrince_Shaffar
SD%Complete: 80
SDComment: Need more tuning of spell timers, it should not be as linear fight as current. Also should possibly find a better way to deal with his three initial beacons to make sure all aggro.
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

/* ContentData
boss_nexusprince_shaffar
mob_ethereal_beacon
EndContentData */

#include "ScriptedPch.h"

enum ePrince
{
    SAY_INTRO                       = -1557000,
    SAY_AGGRO_1                     = -1557001,
    SAY_AGGRO_2                     = -1557002,
    SAY_AGGRO_3                     = -1557003,
    SAY_SLAY_1                      = -1557004,
    SAY_SLAY_2                      = -1557005,
    SAY_SUMMON                      = -1557006,
    SAY_DEAD                        = -1557007,

    SPELL_BLINK                     = 34605,
    SPELL_FROSTBOLT                 = 32364,
    SPELL_FIREBALL                  = 32363,
    SPELL_FROSTNOVA                 = 32365,

    SPELL_ETHEREAL_BEACON           = 32371,                // Summons NPC_BEACON
    SPELL_ETHEREAL_BEACON_VISUAL    = 32368,

    NPC_BEACON                      = 18431,
    NPC_SHAFFAR                     = 18344,

    NR_INITIAL_BEACONS              = 3
};

struct boss_nexusprince_shaffarAI : public ScriptedAI
{
    boss_nexusprince_shaffarAI(Creature *c) : ScriptedAI(c), summons(me) { HasTaunted = false; }

    uint32 Blink_Timer;
    uint32 Beacon_Timer;
    uint32 FireBall_Timer;
    uint32 Frostbolt_Timer;
    uint32 FrostNova_Timer;

    SummonList summons;

    bool HasTaunted;
    bool CanBlink;

    void Reset()
    {
        Blink_Timer = 1500;
        Beacon_Timer = 10000;
        FireBall_Timer = 8000;
        Frostbolt_Timer = 4000;
        FrostNova_Timer = 15000;

        CanBlink = false;

        float dist = 8.0f;
        float posX, posY, posZ, angle;
        me->GetHomePosition(posX, posY, posZ, angle);

        me->SummonCreature(NPC_BEACON, posX - dist, posY - dist, posZ, angle, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 7200000);
        me->SummonCreature(NPC_BEACON, posX - dist, posY + dist, posZ, angle, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 7200000);
        me->SummonCreature(NPC_BEACON, posX + dist, posY, posZ, angle, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 7200000);
    }

    void EnterEvadeMode()
    {
        summons.DespawnAll();
        ScriptedAI::EnterEvadeMode();
    }

    void MoveInLineOfSight(Unit *who)
    {
        if (!HasTaunted && who->GetTypeId() == TYPEID_PLAYER && me->IsWithinDistInMap(who, 100.0f))
        {
            DoScriptText(SAY_INTRO, me);
            HasTaunted = true;
        }
    }

    void EnterCombat(Unit *who)
    {
        DoScriptText(RAND(SAY_AGGRO_1,SAY_AGGRO_2,SAY_AGGRO_3), me);

        DoZoneInCombat();
        summons.DoZoneInCombat();
    }

    void JustSummoned(Creature *summoned)
    {
        if (summoned->GetEntry() == NPC_BEACON)
        {
            summoned->CastSpell(summoned,SPELL_ETHEREAL_BEACON_VISUAL,false);

            if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM,0))
                summoned->AI()->AttackStart(pTarget);
        }

        summons.Summon(summoned);
    }

    void SummonedCreatureDespawn(Creature *summon)
    {
        summons.Despawn(summon);
    }

    void KilledUnit(Unit* victim)
    {
        DoScriptText(RAND(SAY_SLAY_1,SAY_SLAY_2), me);
    }

    void JustDied(Unit* Killer)
    {
        DoScriptText(SAY_DEAD, me);
        summons.DespawnAll();
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (FrostNova_Timer <= diff)
        {
            if (me->IsNonMeleeSpellCasted(false))
                me->InterruptNonMeleeSpells(true);

            DoCast(me, SPELL_FROSTNOVA);
            FrostNova_Timer  = 17500 + rand()%7500;
            CanBlink = true;
        } else FrostNova_Timer -= diff;

        if (Frostbolt_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_FROSTBOLT);
            Frostbolt_Timer = 4500 + rand()%1500;
        } else Frostbolt_Timer -= diff;

        if (FireBall_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_FIREBALL);
            FireBall_Timer = 4500 + rand()%1500;
        } else FireBall_Timer -= diff;

        if (CanBlink)
        {
            if (Blink_Timer <= diff)
            {
                if (me->IsNonMeleeSpellCasted(false))
                    me->InterruptNonMeleeSpells(true);

                //expire movement, will prevent from running right back to victim after cast
                //(but should MoveChase be used again at a certain time or should he not move?)
                if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == TARGETED_MOTION_TYPE)
                    me->GetMotionMaster()->MovementExpired();

                DoCast(me, SPELL_BLINK);
                Blink_Timer = 1000 + rand()%1500;
                CanBlink = false;
            } else Blink_Timer -= diff;
        }

        if (Beacon_Timer <= diff)
        {
            if (me->IsNonMeleeSpellCasted(false))
                me->InterruptNonMeleeSpells(true);

            if (!urand(0,3))
                DoScriptText(SAY_SUMMON, me);

            DoCast(me, SPELL_ETHEREAL_BEACON, true);

            Beacon_Timer = 10000;
        } else Beacon_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_nexusprince_shaffar(Creature* pCreature)
{
    return new boss_nexusprince_shaffarAI (pCreature);
}

enum eEnums
{
    SPELL_ARCANE_BOLT               = 15254,
    SPELL_ETHEREAL_APPRENTICE       = 32372                 // Summon 18430
};

struct mob_ethereal_beaconAI : public ScriptedAI
{
    mob_ethereal_beaconAI(Creature *c) : ScriptedAI(c)
    {
    }

    uint32 Apprentice_Timer;
    uint32 ArcaneBolt_Timer;
    uint32 Check_Timer;

    void KillSelf()
    {
        me->Kill(me);
    }

    void Reset()
    {
        Apprentice_Timer = DUNGEON_MODE(20000, 10000);
        ArcaneBolt_Timer = 1000;
        Check_Timer = 1000;
    }

    void EnterCombat(Unit *who)
    {
        // Send Shaffar to fight
        Creature* Shaffar = me->FindNearestCreature(NPC_SHAFFAR, 100);
        if (!Shaffar || Shaffar->isDead())
        {
            KillSelf();
            return;
        }
        if (!Shaffar->isInCombat())
            Shaffar->AI()->AttackStart(who);
    }

    void JustSummoned(Creature *summoned)
    {
        summoned->AI()->AttackStart(me->getVictim());
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (Check_Timer <= diff)
        {
            Creature *Shaffar = me->FindNearestCreature(NPC_SHAFFAR, 100);
            if (!Shaffar || Shaffar->isDead() || !Shaffar->isInCombat())
            {
                KillSelf();
                return;
            }
            Check_Timer = 1000;
        } else Check_Timer -= diff;

        if (ArcaneBolt_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_ARCANE_BOLT);
            ArcaneBolt_Timer = 2000 + rand()%2500;
        } else ArcaneBolt_Timer -= diff;

        if (Apprentice_Timer <= diff)
        {
            if (me->IsNonMeleeSpellCasted(false))
                me->InterruptNonMeleeSpells(true);

            DoCast(me, SPELL_ETHEREAL_APPRENTICE, true);
            me->ForcedDespawn();
            return;
        } else Apprentice_Timer -= diff;
    }
};

CreatureAI* GetAI_mob_ethereal_beacon(Creature* pCreature)
{
    return new mob_ethereal_beaconAI (pCreature);
}

enum eEthereal
{
    SPELL_ETHEREAL_APPRENTICE_FIREBOLT          = 32369,
    SPELL_ETHEREAL_APPRENTICE_FROSTBOLT         = 32370
};

struct mob_ethereal_apprenticeAI : public ScriptedAI
{
    mob_ethereal_apprenticeAI(Creature *c) : ScriptedAI(c) {}

    uint32 Cast_Timer;

    bool isFireboltTurn;

    void Reset()
    {
        Cast_Timer = 3000;
        isFireboltTurn = true;
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (Cast_Timer <= diff)
        {
            if (isFireboltTurn)
            {
                DoCast(me->getVictim(), SPELL_ETHEREAL_APPRENTICE_FIREBOLT, true);
                isFireboltTurn = false;
            }else{
                DoCast(me->getVictim(), SPELL_ETHEREAL_APPRENTICE_FROSTBOLT, true);
                isFireboltTurn = true;
            }
            Cast_Timer = 3000;
        } else Cast_Timer -= diff;
    }
};

CreatureAI* GetAI_mob_ethereal_apprentice(Creature* pCreature)
{
    return new mob_ethereal_apprenticeAI (pCreature);
}

void AddSC_boss_nexusprince_shaffar()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "boss_nexusprince_shaffar";
    newscript->GetAI = &GetAI_boss_nexusprince_shaffar;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_ethereal_beacon";
    newscript->GetAI = &GetAI_mob_ethereal_beacon;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_ethereal_apprentice";
    newscript->GetAI = &GetAI_mob_ethereal_apprentice;
    newscript->RegisterSelf();
}

