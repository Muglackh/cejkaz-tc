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

/* ScriptData
SDName: Boss Mr.Smite
SD%Complete:
SDComment: Timers and say taken from acid script
EndScriptData */

#include "ScriptedPch.h"
#include "deadmines.h"

enum eSpels
{
    SPELL_TRASH             = 3391,
    SPELL_SMITE_STOMP       = 6432,
    SPELL_SMITE_SLAM        = 6435,
    SPELL_NIMBLE_REFLEXES   = 6264,

    EQUIP_SWORD             = 5191,
    EQUIP_MACE              = 7230,

    SAY_AGGRO               = -1036001
};

struct boss_mr_smiteAI : public ScriptedAI
{
    boss_mr_smiteAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        pInstance = pCreature->GetInstanceData();
    }

    ScriptedInstance* pInstance;

    uint32 uiTrashTimer;
    uint32 uiSlamTimer;
    uint32 uiNimbleReflexesTimer;

    uint8 uiHealth;

    uint32 uiPhase;
    uint32 uiTimer;

    void Reset()
    {
        uiTrashTimer = urand(5000,9000);
        uiSlamTimer = 9000;
        uiNimbleReflexesTimer = urand(15500,31600);

        uiHealth = 0;

        uiPhase = 0;
        uiTimer = 0;

        SetEquipmentSlots(false, EQUIP_SWORD, EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
    }

    void EnterCombat(Unit* pWho)
    {
       DoScriptText(SAY_AGGRO, me);
    }

    bool bCheckChances()
    {
        uint32 uiChances = urand(0,99);
        if (uiChances <= 15)
            return false;
        else
            return true;
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (!UpdateVictim())
            return;

    /*START ACID-AI*/
        if (uiTrashTimer <= uiDiff)
        {
            if (bCheckChances())
                DoCast(me, SPELL_TRASH);
            uiTrashTimer = urand(6000,15500);
        } else uiTrashTimer -= uiDiff;

        if (uiSlamTimer <= uiDiff)
        {
            if (bCheckChances())
                DoCast(me->getVictim(), SPELL_SMITE_SLAM);
            uiSlamTimer = 11000;
        } else uiSlamTimer -= uiDiff;

        if (uiNimbleReflexesTimer <= uiDiff)
        {
            if (bCheckChances())
                DoCast(me, SPELL_NIMBLE_REFLEXES);
            uiNimbleReflexesTimer = urand(27300,60100);
        } else uiNimbleReflexesTimer -= uiDiff;
    /*END ACID-AI*/

        if (uiHealth == 0 && me->GetHealth()*100 / me->GetMaxHealth() <= 66 || uiHealth == 1 && me->GetHealth()*100 / me->GetMaxHealth() <= 33)
        {
            ++uiHealth;
            DoCastAOE(SPELL_SMITE_STOMP,false);
            SetCombatMovement(false);
            if (pInstance)
                if (GameObject* pGo = GameObject::GetGameObject((*me),pInstance->GetData64(DATA_SMITE_CHEST)))
                {
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MovePoint(1,-3.00+pGo->GetPositionX(),pGo->GetPositionY(),pGo->GetPositionZ());
                }
        }

        if (uiPhase)
        {
            if (uiTimer <= uiDiff)
            {
                switch(uiPhase)
                {
                    case 1:
                        me->HandleEmoteCommand(EMOTE_STATE_KNEEL); //dosen't work?
                        uiTimer = 1000;
                        uiPhase = 2;
                        break;
                    case 2:
                        if (uiHealth == 1)
                            SetEquipmentSlots(false, EQUIP_SWORD, EQUIP_SWORD, EQUIP_NO_CHANGE);
                        else
                            SetEquipmentSlots(false, EQUIP_MACE, EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
                        uiTimer = 500;
                        uiPhase = 3;
                        break;
                    case 3:
                        SetCombatMovement(true);
                        me->GetMotionMaster()->MoveChase(me->getVictim(), me->m_CombatDistance);
                        uiPhase = 0;
                        break;

                }
            } else uiTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }

    void MovementInform(uint32 uiType, uint32 uiId)
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        uiTimer = 1500;
        uiPhase = 1;
    }

};
CreatureAI* GetAI_boss_mr_smite(Creature* pCreature)
{
    return new boss_mr_smiteAI (pCreature);
}

void AddSC_boss_mr_smite()
{
    Script* newscript;
    newscript = new Script;
    newscript->Name = "boss_mr_smite";
    newscript->GetAI = &GetAI_boss_mr_smite;
    newscript->RegisterSelf();
}
