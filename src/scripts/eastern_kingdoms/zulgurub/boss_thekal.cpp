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
SDName: Boss_Thekal
SD%Complete: 95
SDComment: Almost finished.
SDCategory: Zul'Gurub
EndScriptData */

#include "ScriptedPch.h"
#include "zulgurub.h"

#define SAY_AGGRO               -1309009
#define SAY_DEATH               -1309010

#define SPELL_MORTALCLEAVE        22859
#define SPELL_SILENCE             23207
#define SPELL_FRENZY              23342
#define SPELL_FORCEPUNCH          24189
#define SPELL_CHARGE              24408
#define SPELL_ENRAGE              23537
#define SPELL_SUMMONTIGERS        24183
#define SPELL_TIGER_FORM          24169
#define SPELL_RESURRECT           24173                     //We will not use this spell.

//Zealot Lor'Khan Spells
#define SPELL_SHIELD              25020
#define SPELL_BLOODLUST           24185
#define SPELL_GREATERHEAL         24208
#define SPELL_DISARM              22691

//Zealot Lor'Khan Spells
#define SPELL_SWEEPINGSTRIKES     18765
#define SPELL_SINISTERSTRIKE      15667
#define SPELL_GOUGE               24698
#define SPELL_KICK                15614
#define SPELL_BLIND               21060

struct boss_thekalAI : public ScriptedAI
{
    boss_thekalAI(Creature *c) : ScriptedAI(c)
    {
        m_pInstance = c->GetInstanceData();
    }

    uint32 MortalCleave_Timer;
    uint32 Silence_Timer;
    uint32 Frenzy_Timer;
    uint32 ForcePunch_Timer;
    uint32 Charge_Timer;
    uint32 Enrage_Timer;
    uint32 SummonTigers_Timer;
    uint32 Check_Timer;
    uint32 Resurrect_Timer;

    ScriptedInstance *m_pInstance;
    bool Enraged;
    bool PhaseTwo;
    bool WasDead;

    void Reset()
    {
        MortalCleave_Timer = 4000;
        Silence_Timer = 9000;
        Frenzy_Timer = 30000;
        ForcePunch_Timer = 4000;
        Charge_Timer = 12000;
        Enrage_Timer = 32000;
        SummonTigers_Timer = 25000;
        Check_Timer = 10000;
        Resurrect_Timer = 10000;

        Enraged = false;
        PhaseTwo = false;
        WasDead = false;
    }

    void EnterCombat(Unit *who)
    {
        DoScriptText(SAY_AGGRO, me);
    }

    void JustDied(Unit* Killer)
    {
        DoScriptText(SAY_DEATH, me);
        if (m_pInstance)
            m_pInstance->SetData(TYPE_THEKAL, DONE);
    }

    void JustReachedHome()
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_THEKAL, NOT_STARTED);
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

            //Check_Timer for the death of LorKhan and Zath.
            if (!WasDead && Check_Timer <= diff)
            {
                if (m_pInstance)
                {
                    if (m_pInstance->GetData(TYPE_LORKHAN) == SPECIAL)
                    {
                        //Resurrect LorKhan
                        if (Unit *pLorKhan = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_LORKHAN)))
                        {
                            pLorKhan->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
                            pLorKhan->setFaction(14);
                            pLorKhan->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                            pLorKhan->SetHealth(int(pLorKhan->GetMaxHealth()*1.0));

                            m_pInstance->SetData(TYPE_LORKHAN, DONE);
                        }
                    }

                    if (m_pInstance->GetData(TYPE_ZATH) == SPECIAL)
                    {
                        //Resurrect Zath
                        Unit *pZath = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_ZATH));
                        if (pZath)
                        {
                            pZath->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
                            pZath->setFaction(14);
                            pZath->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                            pZath->SetHealth(int(pZath->GetMaxHealth()*1.0));

                            m_pInstance->SetData(TYPE_ZATH, DONE);
                        }
                    }
                }

                Check_Timer = 5000;
            } else Check_Timer -= diff;

            if (!PhaseTwo && MortalCleave_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_MORTALCLEAVE);
                MortalCleave_Timer = 15000 + rand()%5000;
            } else MortalCleave_Timer -= diff;

            if (!PhaseTwo && Silence_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_SILENCE);
                Silence_Timer = 20000 + rand()%5000;
            } else Silence_Timer -= diff;

            if (!PhaseTwo && !WasDead && me->GetHealth() <= me->GetMaxHealth() * 0.05)
            {
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->SetStandState(UNIT_STAND_STATE_SLEEP);
                me->AttackStop();

                if (m_pInstance)
                    m_pInstance->SetData(TYPE_THEKAL, SPECIAL);

                WasDead=true;
            }

            //Thekal will transform to Tiger if he died and was not resurrected after 10 seconds.
            if (!PhaseTwo && WasDead)
            {
                if (Resurrect_Timer <= diff)
                {
                    DoCast(me, SPELL_TIGER_FORM);
                    me->SetFloatValue(OBJECT_FIELD_SCALE_X, 2.00f);
                    me->SetStandState(UNIT_STAND_STATE_STAND);
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    me->SetHealth(int(me->GetMaxHealth()*1.0));
                    const CreatureInfo *cinfo = me->GetCreatureInfo();
                    me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, (cinfo->mindmg +((cinfo->mindmg/100) * 40)));
                    me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, (cinfo->maxdmg +((cinfo->maxdmg/100) * 40)));
                    me->UpdateDamagePhysical(BASE_ATTACK);
                    DoResetThreat();
                    PhaseTwo = true;
                } else Resurrect_Timer -= diff;
            }

            if ((me->GetHealth()*100 / me->GetMaxHealth() == 100) && WasDead)
            {
                WasDead = false;
            }

            if (PhaseTwo)
            {
                if (Charge_Timer <= diff)
                {
                    if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM,0))
                    {
                        DoCast(pTarget, SPELL_CHARGE);
                        DoResetThreat();
                        AttackStart(pTarget);
                    }

                    Charge_Timer = 15000 + rand()%7000;
                } else Charge_Timer -= diff;

                if (Frenzy_Timer <= diff)
                {
                    DoCast(me, SPELL_FRENZY);
                    Frenzy_Timer = 30000;
                } else Frenzy_Timer -= diff;

                if (ForcePunch_Timer <= diff)
                {
                    DoCast(me->getVictim(), SPELL_SILENCE);
                    ForcePunch_Timer = 16000 + rand()%5000;
                } else ForcePunch_Timer -= diff;

                if (SummonTigers_Timer <= diff)
                {
                    DoCast(me->getVictim(), SPELL_SUMMONTIGERS);
                    SummonTigers_Timer = 10000 + rand()%4000;
                } else SummonTigers_Timer -= diff;

                if ((me->GetHealth()*100 / me->GetMaxHealth() < 11) && !Enraged)
                {
                    DoCast(me, SPELL_ENRAGE);
                    Enraged = true;
                }
            }

            DoMeleeAttackIfReady();

    }
};

//Zealot Lor'Khan
struct mob_zealot_lorkhanAI : public ScriptedAI
{
    mob_zealot_lorkhanAI(Creature *c) : ScriptedAI(c)
    {
        m_pInstance = c->GetInstanceData();
    }

    uint32 Shield_Timer;
    uint32 BloodLust_Timer;
    uint32 GreaterHeal_Timer;
    uint32 Disarm_Timer;
    uint32 Check_Timer;

    bool FakeDeath;

    ScriptedInstance *m_pInstance;

    void Reset()
    {
        Shield_Timer = 1000;
        BloodLust_Timer = 16000;
        GreaterHeal_Timer = 32000;
        Disarm_Timer = 6000;
        Check_Timer = 10000;

        FakeDeath = false;

        if (m_pInstance)
            m_pInstance->SetData(TYPE_LORKHAN, NOT_STARTED);

        me->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    void EnterCombat(Unit *who)
    {
    }

    void UpdateAI (const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        //Shield_Timer
        if (Shield_Timer <= diff)
        {
            DoCast(me, SPELL_SHIELD);
            Shield_Timer = 61000;
        } else Shield_Timer -= diff;

        //BloodLust_Timer
        if (BloodLust_Timer <= diff)
        {
            DoCast(me, SPELL_BLOODLUST);
            BloodLust_Timer = 20000+rand()%8000;
        } else BloodLust_Timer -= diff;

        //Casting Greaterheal to Thekal or Zath if they are in meele range.
        if (GreaterHeal_Timer <= diff)
        {
            if (m_pInstance)
            {
                Unit *pThekal = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_THEKAL));
                Unit *pZath = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_ZATH));

                if (!pThekal || !pZath)
                    return;

                switch (urand(0,1))
                {
                    case 0:
                        if (me->IsWithinMeleeRange(pThekal))
                            DoCast(pThekal, SPELL_GREATERHEAL);
                        break;
                    case 1:
                        if (me->IsWithinMeleeRange(pZath))
                            DoCast(pZath, SPELL_GREATERHEAL);
                        break;
                }
            }

            GreaterHeal_Timer = 15000+rand()%5000;
        } else GreaterHeal_Timer -= diff;

        //Disarm_Timer
        if (Disarm_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_DISARM);
            Disarm_Timer = 15000+rand()%10000;
        } else Disarm_Timer -= diff;

        //Check_Timer for the death of LorKhan and Zath.
        if (!FakeDeath && Check_Timer <= diff)
        {
            if (m_pInstance)
            {
                if (m_pInstance->GetData(TYPE_THEKAL) == SPECIAL)
                {
                    //Resurrect Thekal
                    if (Unit *pThekal = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_THEKAL)))
                    {
                        pThekal->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
                        pThekal->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        pThekal->setFaction(14);
                        pThekal->SetHealth(int(pThekal->GetMaxHealth()*1.0));
                    }
                }

                if (m_pInstance->GetData(TYPE_ZATH) == SPECIAL)
                {
                    //Resurrect Zath
                    if (Unit *pZath = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_ZATH)))
                    {
                        pZath->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
                        pZath->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        pZath->setFaction(14);
                        pZath->SetHealth(int(pZath->GetMaxHealth()*1.0));
                    }
                }
            }

            Check_Timer = 5000;
        } else Check_Timer -= diff;

        if (me->GetHealth() <= me->GetMaxHealth() * 0.05)
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetStandState(UNIT_STAND_STATE_SLEEP);
            me->setFaction(35);
            me->AttackStop();

            if (m_pInstance)
                m_pInstance->SetData(TYPE_LORKHAN, SPECIAL);

            FakeDeath = true;
        }

        DoMeleeAttackIfReady();
    }
};

//Zealot Zath
struct mob_zealot_zathAI : public ScriptedAI
{
    mob_zealot_zathAI(Creature *c) : ScriptedAI(c)
    {
        m_pInstance = c->GetInstanceData();
    }

    uint32 SweepingStrikes_Timer;
    uint32 SinisterStrike_Timer;
    uint32 Gouge_Timer;
    uint32 Kick_Timer;
    uint32 Blind_Timer;
    uint32 Check_Timer;

    bool FakeDeath;

    ScriptedInstance *m_pInstance;

    void Reset()
    {
        SweepingStrikes_Timer = 13000;
        SinisterStrike_Timer = 8000;
        Gouge_Timer = 25000;
        Kick_Timer = 18000;
        Blind_Timer = 5000;
        Check_Timer = 10000;

        FakeDeath = false;

        if (m_pInstance)
            m_pInstance->SetData(TYPE_ZATH, NOT_STARTED);

        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    void EnterCombat(Unit *who)
    {
    }

    void UpdateAI (const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        //SweepingStrikes_Timer
        if (SweepingStrikes_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_SWEEPINGSTRIKES);
            SweepingStrikes_Timer = 22000+rand()%4000;
        } else SweepingStrikes_Timer -= diff;

        //SinisterStrike_Timer
        if (SinisterStrike_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_SINISTERSTRIKE);
            SinisterStrike_Timer = 8000+rand()%8000;
        } else SinisterStrike_Timer -= diff;

        //Gouge_Timer
        if (Gouge_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_GOUGE);

            if (DoGetThreat(me->getVictim()))
                DoModifyThreatPercent(me->getVictim(),-100);

            Gouge_Timer = 17000+rand()%10000;
        } else Gouge_Timer -= diff;

        //Kick_Timer
        if (Kick_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_KICK);
            Kick_Timer = 15000+rand()%10000;
        } else Kick_Timer -= diff;

        //Blind_Timer
        if (Blind_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_BLIND);
            Blind_Timer = 10000+rand()%10000;
        } else Blind_Timer -= diff;

        //Check_Timer for the death of LorKhan and Zath.
        if (!FakeDeath && Check_Timer <= diff)
        {
            if (m_pInstance)
            {
                if (m_pInstance->GetData(TYPE_LORKHAN) == SPECIAL)
                {
                    //Resurrect LorKhan
                    if (Unit *pLorKhan = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_LORKHAN)))
                    {
                        pLorKhan->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
                        pLorKhan->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        pLorKhan->setFaction(14);
                        pLorKhan->SetHealth(int(pLorKhan->GetMaxHealth()*1.0));
                    }
                }

                if (m_pInstance->GetData(TYPE_THEKAL) == SPECIAL)
                {
                    //Resurrect Thekal
                    if (Unit *pThekal = Unit::GetUnit((*me), m_pInstance->GetData64(DATA_THEKAL)))
                    {
                        pThekal->SetUInt32Value(UNIT_FIELD_BYTES_1, 0);
                        pThekal->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        pThekal->setFaction(14);
                        pThekal->SetHealth(int(pThekal->GetMaxHealth()*1.0));
                    }
                }
            }

            Check_Timer = 5000;
        } else Check_Timer -= diff;

        if (me->GetHealth() <= me->GetMaxHealth() * 0.05)
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetStandState(UNIT_STAND_STATE_SLEEP);
            me->setFaction(35);
            me->AttackStop();

            if (m_pInstance)
                m_pInstance->SetData(TYPE_ZATH, SPECIAL);

            FakeDeath = true;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_thekal(Creature* pCreature)
{
    return new boss_thekalAI (pCreature);
}

CreatureAI* GetAI_mob_zealot_lorkhan(Creature* pCreature)
{
    return new mob_zealot_lorkhanAI (pCreature);
}

CreatureAI* GetAI_mob_zealot_zath(Creature* pCreature)
{
    return new mob_zealot_zathAI (pCreature);
}

void AddSC_boss_thekal()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "boss_thekal";
    newscript->GetAI = &GetAI_boss_thekal;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_zealot_lorkhan";
    newscript->GetAI = &GetAI_mob_zealot_lorkhan;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_zealot_zath";
    newscript->GetAI = &GetAI_mob_zealot_zath;
    newscript->RegisterSelf();
}

