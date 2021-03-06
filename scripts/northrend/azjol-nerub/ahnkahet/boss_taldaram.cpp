/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
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
SDName: Boss_Taldaram
SD%Complete: 90%
SDComment: Timers;
SDCategory: Ahn'kahet
EndScriptData */

#include "precompiled.h"
#include "ahnkahet.h"

enum
{
    SAY_AGGRO                       = -1619008,
    SAY_VANISH_1                    = -1619009,
    SAY_VANISH_2                    = -1619010,
    SAY_FEED_1                      = -1619011,
    SAY_FEED_2                      = -1619012,
    SAY_SLAY_1                      = -1619013,
    SAY_SLAY_2                      = -1619014,
    SAY_SLAY_3                      = -1619015,
    SAY_DEATH                       = -1619016,

    SPELL_BEAM_VISUAL               = 60342,        // Visual spell, used before Taltaram is lowered to the ground
    SPELL_CONJURE_FLAME_SPHERE      = 55931,
    SPELL_FLAME_ORB_SUMMON          = 57752,        // summons 30702
    SPELL_BLOODTHIRST               = 55968,
    SPELL_VANISH                    = 55964,
    SPELL_EMBRACE_OF_THE_VAMPYR     = 55959,
    SPELL_EMBRACE_OF_THE_VAMPYR_H   = 59513,

    // Spells used by the Flame Orb
    SPELL_FLAME_ORB                 = 57750,
    SPELL_FLAME_ORB_H               = 58937,
    SPELL_FLAME_SPHERE_SPAWN_EFFECT = 55891,
    SPELL_FLAME_SPHERE_VISUAL       = 55928,
    SPELL_FLAME_SPHERE_DEATH_EFFECT = 55947,

    MAX_FLAME_ORBS                  = 3,
};

/*######
## boss_taldaram
######*/

struct MANGOS_DLL_DECL boss_taldaramAI : public ScriptedAI
{
    boss_taldaramAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_ahnkahet*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        // Don't set the visual timers if the devices are already activated (reload case)
        m_uiVisualTimer = m_pInstance->GetData(TYPE_TALDARAM) == SPECIAL ? 0 : 1000;
        Reset();
    }

    instance_ahnkahet* m_pInstance;
    bool m_bIsRegularMode;

    bool m_bIsFirstAggro;
    uint32 m_uiVisualTimer;
    uint32 m_uiBloodthirstTimer;
    uint32 m_uiFlameOrbTimer;
    uint32 m_uiVanishTimer;
    uint32 m_uiVanishExpireTimer;
    uint32 m_uiEmbraceTimer;

    GUIDList m_lFlameOrbsGuidList;

    void Reset()
    {
        // Timers seem to be very random...
        m_uiBloodthirstTimer    = urand(20000, 25000);
        m_uiFlameOrbTimer       = urand(15000, 20000);
        m_uiVanishTimer         = 0;
        m_uiEmbraceTimer        = 0;
        m_uiVanishExpireTimer   = 0;
        m_bIsFirstAggro         = false;
    }

    void Aggro(Unit* pWho)
    {
        // Aggro is called after the boss vanish expires. There is no need to call this multiple times
        if (m_bIsFirstAggro)
            return;

        DoScriptText(SAY_AGGRO, m_creature);
        m_bIsFirstAggro = true;

        if (m_pInstance)
            m_pInstance->SetData(TYPE_TALDARAM, IN_PROGRESS);
    }

    void KilledUnit(Unit* pVictim)
    {
        switch(urand(0, 2))
        {
            case 0: DoScriptText(SAY_SLAY_1, m_creature); break;
            case 1: DoScriptText(SAY_SLAY_2, m_creature); break;
            case 2: DoScriptText(SAY_SLAY_3, m_creature); break;
        }
    }

    void JustDied(Unit* pKiller)
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_TALDARAM, DONE);
    }

    void JustReachedHome()
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_TALDARAM, FAIL);
    }

    void EnterEvadeMode()
    {
        // Don't allow him to evade during vanish
        if (m_uiVanishExpireTimer)
            return;

        m_creature->RemoveAllAuras();
        m_creature->DeleteThreatList();
        m_creature->CombatStop(true);
        m_creature->LoadCreatureAddon(true);

        // should evade on the ground
        if (m_creature->isAlive())
            m_creature->GetMotionMaster()->MovePoint(1, aTaldaramLandingLoc[0], aTaldaramLandingLoc[1], aTaldaramLandingLoc[2]);

        m_creature->SetLootRecipient(NULL);

        Reset();
    }

    void MovementInform(uint32 uiMoveType, uint32 uiPointId)
    {
        if (uiMoveType != POINT_MOTION_TYPE)
            return;

        // Adjust orientation
        if (uiPointId)
        {
            m_creature->SetLevitate(false);
            m_creature->SetFacingTo(aTaldaramLandingLoc[3]);
        }
    }

    void JustSummoned(Creature* pSummoned)
    {
        pSummoned->CastSpell(pSummoned, SPELL_FLAME_SPHERE_SPAWN_EFFECT, true);
        pSummoned->CastSpell(pSummoned, SPELL_FLAME_SPHERE_VISUAL, true);

        m_lFlameOrbsGuidList.push_back(pSummoned->GetObjectGuid());
    }

    void SummonedCreatureDespawn(Creature* pSummoned)
    {
        pSummoned->CastSpell(pSummoned, SPELL_FLAME_SPHERE_DEATH_EFFECT, true);
    }

    // Wrapper which sends each orb in a different direction
    void DoSetOrbsInMotion()
    {
        float fX, fY;
        for (GUIDList::const_iterator itr = m_lFlameOrbsGuidList.begin(); itr != m_lFlameOrbsGuidList.end(); ++itr)
        {
            if (Creature* pOrb = m_creature->GetMap()->GetCreature(*itr))
            {
                pOrb->CastSpell(pOrb, m_bIsRegularMode ? SPELL_FLAME_ORB : SPELL_FLAME_ORB_H, true);

                pOrb->GetNearPoint2D(fX, fY, 70.0f, (2*M_PI_F/3)*(*itr).GetCounter());
                pOrb->GetMotionMaster()->MovePoint(0, fX, fY, pOrb->GetPositionZ());
            }
        }
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (m_uiVisualTimer)
        {
            if (m_uiVisualTimer <= uiDiff)
            {
                GUIDList lControllersList;
                if (m_pInstance)
                    m_pInstance->GetJedogaControllersList(lControllersList);

                for (GUIDList::const_iterator itr = lControllersList.begin(); itr != lControllersList.end(); ++itr)
                {
                    if (Creature* pTemp = m_creature->GetMap()->GetCreature(*itr))
                        pTemp->CastSpell(m_creature, SPELL_BEAM_VISUAL, false);
                }
                m_uiVisualTimer = 0;
            }
            else
                m_uiVisualTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiVanishExpireTimer)
        {
            if (m_uiVanishExpireTimer <= uiDiff)
            {
                m_uiEmbraceTimer      = 1000;
                m_uiVanishExpireTimer = 0;
            }
            else
                m_uiVanishExpireTimer -= uiDiff;

            // do not use other abilities during vanish
            return;
        }

        if (m_uiVanishTimer)
        {
            if (m_uiVanishTimer <= uiDiff)
            {
                if (DoCastSpellIfCan (m_creature, SPELL_VANISH) == CAST_OK)
                {
                    DoScriptText(urand(0, 1) ? SAY_VANISH_1 : SAY_VANISH_2, m_creature);
                    m_uiVanishTimer       = 0;
                    m_uiVanishExpireTimer = 2500;
                }
            }
            else
                m_uiVanishTimer -= uiDiff;
        }

        // Cast Embrace of the Vampyr after Vanish expires
        if (m_uiEmbraceTimer)
        {
            if (m_uiEmbraceTimer <= uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                {
                    if (DoCastSpellIfCan(pTarget, m_bIsRegularMode ? SPELL_EMBRACE_OF_THE_VAMPYR : SPELL_EMBRACE_OF_THE_VAMPYR_H) == CAST_OK)
                    {
                        DoScriptText(urand(0, 1) ? SAY_FEED_1 : SAY_FEED_2, m_creature);
                        m_uiEmbraceTimer = 0;
                    }
                }
            }
            else
                m_uiEmbraceTimer -= uiDiff;
        }

        if (m_uiBloodthirstTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_BLOODTHIRST) == CAST_OK)
                m_uiBloodthirstTimer = urand(20000, 25000);
        }
        else
            m_uiBloodthirstTimer -= uiDiff;

        if (m_uiFlameOrbTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_CONJURE_FLAME_SPHERE) == CAST_OK)
            {
                m_lFlameOrbsGuidList.clear();

                // Flame orbs are summoned above the boss
                for (uint8 i = 0; i < MAX_FLAME_ORBS; ++i)
                    m_creature->CastSpell(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ() + 5.0f, SPELL_FLAME_ORB_SUMMON, true);

                m_uiFlameOrbTimer = urand(50000, 60000);
                m_uiVanishTimer   = 12000;
            }
        }
        else
            m_uiFlameOrbTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_taldaram(Creature* pCreature)
{
    return new boss_taldaramAI(pCreature);
}

bool EffectDummyCreature_spell_conjure_flame_orbs(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Creature* pCreatureTarget)
{
    //always check spellid and effectindex
    if (uiSpellId == SPELL_CONJURE_FLAME_SPHERE && uiEffIndex == EFFECT_INDEX_0)
    {
        if (boss_taldaramAI* pBossAI = dynamic_cast<boss_taldaramAI*>(pCreatureTarget->AI()))
            pBossAI->DoSetOrbsInMotion();

        //always return true when we are handling this spell and effect
        return true;
    }

    return false;
}

/*######
## go_nerubian_device
######*/

bool GOUse_go_nerubian_device(Player* pPlayer, GameObject* pGo)
{
    ScriptedInstance* pInstance = (ScriptedInstance*)pGo->GetInstanceData();

    if (!pInstance)
        return false;

    // Don't allow players to use the devices if encounter is already finished or in progress (reload case)
    if (pInstance->GetData(TYPE_TALDARAM) == SPECIAL || pInstance->GetData(TYPE_TALDARAM) == DONE)
        return false;

    pInstance->SetData(TYPE_TALDARAM, SPECIAL);
    return false;
}

void AddSC_boss_taldaram()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_taldaram";
    pNewScript->GetAI = &GetAI_boss_taldaram;
    pNewScript->pEffectDummyNPC = &EffectDummyCreature_spell_conjure_flame_orbs;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_nerubian_device";
    pNewScript->pGOUse = &GOUse_go_nerubian_device;
    pNewScript->RegisterSelf();
}
