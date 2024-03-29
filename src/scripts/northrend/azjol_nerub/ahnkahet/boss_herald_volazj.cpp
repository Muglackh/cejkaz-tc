/*
* Copyright (C) 2009 - 2010 TrinityCore <http://www.trinitycore.org/>
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

/*
 * Comment: Missing AI for Twisted Visages
 */

#include "ScriptedPch.h"
#include "ahnkahet.h"

enum Spells
{
    SPELL_INSANITY                                = 57496, //Dummy
    INSANITY_VISUAL                               = 57561,
    SPELL_INSANITY_TARGET                         = 57508,
    SPELL_MIND_FLAY                               = 57941,
    H_SPELL_MIND_FLAY                             = 59974,
    SPELL_SHADOW_BOLT_VOLLEY                      = 57942,
    H_SPELL_SHADOW_BOLT_VOLLEY                    = 59975,
    SPELL_SHIVER                                  = 57949,
    H_SPELL_SHIVER                                = 59978,
    SPELL_CLONE_PLAYER                            = 57507, //casted on player during insanity
    SPELL_INSANITY_PHASING_1                      = 57508,
    SPELL_INSANITY_PHASING_2                      = 57509,
    SPELL_INSANITY_PHASING_3                      = 57519,
    SPELL_INSANITY_PHASING_4                      = 57511,
    SPELL_INSANITY_PHASING_5                      = 57512
};

enum Creatures
{
    MOB_TWISTED_VISAGE                            = 30625
};

//not in db
enum Yells
{
    SAY_AGGRO                                     = -1619030,
    SAY_SLAY_1                                    = -1619031,
    SAY_SLAY_2                                    = -1619032,
    SAY_SLAY_3                                    = -1619033,
    SAY_DEATH_1                                   = -1619034,
    SAY_DEATH_2                                   = -1619035,
    SAY_PHASE                                     = -1619036
};

enum Achievements
{
    ACHIEV_QUICK_DEMISE                           = 1862
};

struct boss_volazjAI : public ScriptedAI
{
    boss_volazjAI(Creature* pCreature) : ScriptedAI(pCreature),Summons(me)
    {
        pInstance = pCreature->GetInstanceData();
    }

    ScriptedInstance *pInstance;

    uint32 uiMindFlayTimer;
    uint32 uiShadowBoltVolleyTimer;
    uint32 uiShiverTimer;
    uint32 uiEncounterTimer;
    uint32 insanityHandled;
    SummonList Summons;

    uint32 GetHealthPct(uint32 damage)
    {
        if (damage > me->GetHealth())
            return 0;
        return 100*(me->GetHealth()-damage)/me->GetMaxHealth();
    }

    void DamageTaken(Unit *pAttacker, uint32 &damage)
    {
        if (me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))
            damage = 0;

        if ((GetHealthPct(0) >= 66 && GetHealthPct(damage) < 66)||
            (GetHealthPct(0) >= 33 && GetHealthPct(damage) < 33))
        {
            me->InterruptNonMeleeSpells(false);
            DoCast(me, SPELL_INSANITY, false);
        }
    }

    void SpellHitTarget(Unit *pTarget, const SpellEntry *spell)
    {
        if (spell->Id == SPELL_INSANITY)
        {
            // Not good target or too many players
            if (pTarget->GetTypeId() != TYPEID_PLAYER || insanityHandled > 4)
                return;
            // First target - start channel visual and set self as unnattackable
            if (!insanityHandled)
            {
                // Channel visual
                DoCast(me, INSANITY_VISUAL, true);
                // Unattackable
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->SetControlled(true, UNIT_STAT_STUNNED);
            }
            // phase mask
            pTarget->CastSpell(pTarget, SPELL_INSANITY_TARGET+insanityHandled, true);
            // summon twisted party members for this target
            Map::PlayerList const &players = me->GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
            {
                Player *plr = i->getSource();
                if (!plr || !plr->isAlive())
                    continue;
                // Summon clone
                if (Unit *summon = me->SummonCreature(MOB_TWISTED_VISAGE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation(),TEMPSUMMON_CORPSE_DESPAWN,0))
                {
                    // required for correct visual
                    // Fixme: allow mirror image query to send other guid to get rid of minion status
                    summon->SetUInt64Value(UNIT_FIELD_CREATEDBY, plr->GetGUID());
                    // clone
                    plr->CastSpell(summon, SPELL_CLONE_PLAYER, true);
                    // set phase
                    summon->SetPhaseMask((1<<(4+insanityHandled)),true);
                }
            }
            ++insanityHandled;
        }
    }

    void Reset()
    {
        uiEncounterTimer = 0;
        uiMindFlayTimer = 8*IN_MILISECONDS;
        uiShadowBoltVolleyTimer = 5*IN_MILISECONDS;
        uiShiverTimer = 15*IN_MILISECONDS;

        if (pInstance)
            pInstance->SetData(DATA_HERALD_VOLAZJ, NOT_STARTED);

        // Visible for all players in insanity
        me->SetPhaseMask((1|16|32|64|128|256),true);
        // Used for Insanity handling
        insanityHandled = 0;

        // Cleanup
        Summons.DespawnAll();
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    void EnterCombat(Unit* who)
    {
        DoScriptText(SAY_AGGRO, me);

        if (pInstance)
            pInstance->SetData(DATA_HERALD_VOLAZJ, IN_PROGRESS);
    }

    void JustSummoned(Creature *summon)
    {
        Summons.Summon(summon);
    }

    uint32 GetSpellForPhaseMask(uint32 phase)
    {
        uint32 spell = 0;
        switch (phase)
        {
            case 16:
                spell = SPELL_INSANITY_PHASING_1;
                break;
            case 32:
                spell = SPELL_INSANITY_PHASING_2;
                break;
            case 64:
                spell = SPELL_INSANITY_PHASING_3;
                break;
            case 128:
                spell = SPELL_INSANITY_PHASING_4;
                break;
            case 256:
                spell = SPELL_INSANITY_PHASING_5;
                break;
        }
        return spell;
    }

    void SummonedCreatureDespawn(Creature *summon)
    {
        uint32 phase= summon->GetPhaseMask();
        uint32 nextPhase = 0;
        Summons.Despawn(summon);

        // Check if all summons in this phase killed
        for (SummonList::const_iterator iter = Summons.begin(); iter != Summons.end(); ++iter)
        {
            if (Creature *visage = Unit::GetCreature(*me, *iter))
            {
                // Not all are dead
                if (phase == visage->GetPhaseMask())
                    return;
                else
                    nextPhase = visage->GetPhaseMask();
            }
        }

        // Roll Insanity
        uint32 spell = GetSpellForPhaseMask(phase);
        uint32 spell2 = GetSpellForPhaseMask(nextPhase);
        Map* pMap = me->GetMap();
        if (!pMap)
            return;

        Map::PlayerList const &PlayerList = pMap->GetPlayers();
        if (!PlayerList.isEmpty())
        {
            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            {
                if (Player* pPlayer = i->getSource())
                {
                    if (pPlayer->HasAura(spell))
                    {
                        pPlayer->RemoveAurasDueToSpell(spell);
                        if (spell2) // if there is still some different mask cast spell for it
                            pPlayer->CastSpell(pPlayer, spell2, true);
                    }
                }
            }
        }
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        if (insanityHandled)
        {
            if (!Summons.empty())
                return;

            insanityHandled = 0;
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetControlled(false, UNIT_STAT_STUNNED);
            me->RemoveAurasDueToSpell(INSANITY_VISUAL);
        }

        if (uiMindFlayTimer <= diff)
        {
            DoCast(me->getVictim(), SPELL_MIND_FLAY);
            uiMindFlayTimer = 20*IN_MILISECONDS;
        } else uiMindFlayTimer -= diff;

        if (uiShadowBoltVolleyTimer <= diff)
        {
            DoCast(me->getVictim(), SPELL_SHADOW_BOLT_VOLLEY);
            uiShadowBoltVolleyTimer = 5*IN_MILISECONDS;
        } else uiShadowBoltVolleyTimer -= diff;

        if (uiShiverTimer <= diff)
        {
            if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM, 0))
                DoCast(pTarget, SPELL_SHIVER);
            uiShiverTimer = 15*IN_MILISECONDS;
        } else uiShiverTimer -= diff;

        uiEncounterTimer += diff;

        DoMeleeAttackIfReady();
    }
    void JustDied(Unit* killer)
    {
        DoScriptText(SAY_DEATH_1, me);

        if (pInstance)
        {
            pInstance->SetData(DATA_HERALD_VOLAZJ, DONE);
            if (IsHeroic() && uiEncounterTimer < 120*IN_MILISECONDS)
                pInstance->DoCompleteAchievement(ACHIEV_QUICK_DEMISE);
        }

        Summons.DespawnAll();
    }

    void KilledUnit(Unit *victim)
    {
        DoScriptText(RAND(SAY_SLAY_1,SAY_SLAY_2,SAY_SLAY_3), me);
    }
};

CreatureAI* GetAI_boss_volazj(Creature* pCreature)
{
    return new boss_volazjAI (pCreature);
}

void AddSC_boss_volazj()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "boss_volazj";
    newscript->GetAI = &GetAI_boss_volazj;
    newscript->RegisterSelf();
}
