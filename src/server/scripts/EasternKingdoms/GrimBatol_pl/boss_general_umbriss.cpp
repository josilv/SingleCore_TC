/*
 * Copyright (C) 2016-2017 ProjectLegion <https://bitbucket.org/projectlegion/projectlegion/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "grim_batol.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"

enum Yells
{
    SAY_AGGRO   = 0,
    SAY_KILL    = 1,
    SAY_ADDS    = 4
};

enum Spells
{
    SPELL_BLEEDING_WOUND        = 74846,
    SPELL_BLEEDING_WOUND_H      = 91937,
    SPELL_GROUND_SIEGE_DUMMY    = 74636,
    SPELL_GROUND_SIEGE          = 74634,
    SPELL_GROUND_SIEGE_H        = 90249,
    SPELL_BLITZ                 = 74670,
    SPELL_BLITZ_H               = 90250,
    SPELL_BLITZ_DMG             = 74675,
    SPELL_BLITZ_DMG_H           = 90251,
    SPELL_FRENZY                = 74853,
    SPELL_CLAW_PUNCTURE         = 76507,
    SPELL_CLAW_PUNCTURE_H       = 90212,
    SPELL_MODGUD_MALICE         = 74699,
    SPELL_MODGUD_MALICE_H       = 90169,
    SPELL_MODGUD_MALICE_AURA    = 90170,
    SPELL_MODGUD_MALADY         = 74837,
    SPELL_MODGUD_MALADY_H       = 90179
};

enum Events
{
    EVENT_BLEEDING_WOUND        = 1,
    EVENT_GROUND_SIEGE          = 2,
    EVENT_BLITZ                 = 3,
    EVENT_CLAW_PUNCTURE         = 4,
    EVENT_ADDS                  = 5
};

enum CreatureIds
{
    NPC_GROUND_SIEGE_STALKER    = 40030,
    NPC_BLITZ_STALKER           = 40040,
    NPC_MALIGNANT_TROGG         = 39984,
    NPC_TROGG_DWELLER           = 45467
};

const Position troggPos[2]=
{
    { -722.15f, -442.53f, 268.77f, 0.54f },
    { -702.22f, -450.9f, 268.77f, 1.34f  }
};

class boss_general_umbriss : public CreatureScript
{
    public:

        boss_general_umbriss() : CreatureScript("boss_general_umbriss"){ }

        struct boss_general_umbrissAI : public ScriptedAI
        {
            boss_general_umbrissAI(Creature* creature) : ScriptedAI(creature), summons(me)
            {
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_STUN, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_ROOT, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FREEZE, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SAPPED, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_CHARM, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISORIENTED, true);
                instance = creature->GetInstanceScript();
            }

            void Reset() override
            {
                if (!instance)
                    return;

                Enrage = false;
                summons.DespawnAll();
                events.Reset();
                instance->SetData(DATA_GENERAL_UMBRISS, NOT_STARTED);
            }

            void EnterCombat(Unit* /*who*/) override
            {
                if (!instance)
                    return;

                events.ScheduleEvent(EVENT_ADDS, 30000);
                events.ScheduleEvent(EVENT_GROUND_SIEGE, 10000);
                events.ScheduleEvent(EVENT_BLEEDING_WOUND, 5000);
                events.ScheduleEvent(EVENT_BLITZ, 23000);
                Talk(SAY_AGGRO);
                instance->SetData(DATA_GENERAL_UMBRISS, IN_PROGRESS);
            }

            void JustDied(Unit* /*killer*/) override
            {
                if (!instance)
                    return;
                
                summons.DespawnAll();
                instance->SetData(DATA_GENERAL_UMBRISS, DONE);
            }

            void KilledUnit(Unit* /*victim*/) override
            {
                Talk(SAY_KILL);
            }
            
            void JustSummoned(Creature* summon) override
            {
                summons.Summon(summon);
                switch (summon->GetEntry())
                {
                case NPC_MALIGNANT_TROGG:
                case NPC_TROGG_DWELLER:
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                        summon->AI()->AttackStart(target);
                    break;
                }
            }

            void SummonedCreatureDespawn(Creature* summon) override
            {
                summons.Despawn(summon);
            }

            void UpdateAI(uint32 diff) override
            {
                if (!instance || !UpdateVictim())
                    return;

                if (me->GetDistance(me->GetHomePosition()) > 60.0f)
                {
                    EnterEvadeMode();
                    return;
                }

                if (HealthBelowPct(30) && !Enrage)
                {
                    Enrage = true;
                    DoCast(me, SPELL_FRENZY);
                    return;
                }

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while(uint32 eventId = events.ExecuteEvent())
                {
                    switch(eventId)
                    {
                    case EVENT_BLEEDING_WOUND:
                        DoCastVictim(SPELL_BLEEDING_WOUND);
                        events.ScheduleEvent(EVENT_BLEEDING_WOUND, 25000);
                        break;
                    case EVENT_GROUND_SIEGE:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1, 0.0f, true))
                        {
                            if (Creature* stalker = me->SummonCreature(NPC_GROUND_SIEGE_STALKER, 
                                target->GetPositionX(),
                                target->GetPositionY(),
                                target->GetPositionZ(),
                                0.0f, TEMPSUMMON_TIMED_DESPAWN, 6000))
                                DoCast(stalker, SPELL_GROUND_SIEGE);
                        }
                        events.ScheduleEvent(EVENT_GROUND_SIEGE, 18000);
                        break;
                    case EVENT_BLITZ:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1, 0, true))
                        {
                            if (Creature* _blitz = me->SummonCreature(NPC_BLITZ_STALKER,
                                target->GetPositionX(),
                                target->GetPositionY(),
                                target->GetPositionZ(),
                                0.0f, TEMPSUMMON_TIMED_DESPAWN, 6000))
                                DoCast(_blitz, SPELL_BLITZ);
                        }
                        events.ScheduleEvent(EVENT_BLITZ, 23000);
                        break;
                    case EVENT_ADDS:
                        if (!Enrage)
                        {
                            for(uint8 i = 0; i < 3; i++)
                                me->SummonCreature(NPC_TROGG_DWELLER, troggPos[0]);
                            me->SummonCreature(NPC_MALIGNANT_TROGG, troggPos[1]);
                            Talk(SAY_ADDS);
                        }
                        events.ScheduleEvent(EVENT_ADDS, 45000);
                        break;
                    }
                }
                DoMeleeAttackIfReady();
            }

        private:
            InstanceScript* instance;
            EventMap events;
            SummonList summons;
            bool Enrage;
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return new boss_general_umbrissAI(creature);
        }
};

class npc_malignant_trogg : public CreatureScript
{
    public:

        npc_malignant_trogg() : CreatureScript("npc_malignant_trogg"){ }

        struct npc_malignant_troggAI : public ScriptedAI
        {
            npc_malignant_troggAI(Creature* creature) : ScriptedAI(creature)
            {
            }

            void Reset() override
            {
                DoCast(me, SPELL_MODGUD_MALICE);
            }

            void EnterCombat(Unit* /*attacker*/) override
            {
                events.ScheduleEvent(EVENT_CLAW_PUNCTURE, 5000);
            }

            void JustDied(Unit* /*killer*/) override
            {
                DoCast(me, SPELL_MODGUD_MALADY, true);
                if (IsHeroic())
                    DoCast(me, SPELL_MODGUD_MALICE_AURA, true);
            }

            void UpdateAI(uint32 diff) override
            {
                if (!UpdateVictim())
                    return;

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while(uint32 eventId = events.ExecuteEvent())
                {
                    switch(eventId)
                    {
                    case EVENT_CLAW_PUNCTURE:
                        DoCastVictim(SPELL_CLAW_PUNCTURE);
                        events.ScheduleEvent(EVENT_CLAW_PUNCTURE, urand(5000, 10000));
                        break;
                    }
                }
                DoMeleeAttackIfReady();
            }

        private:
            EventMap events;
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return new npc_malignant_troggAI(creature);
        }
};

class npc_umbriss_trogg_dweller : public CreatureScript
{
    public:

        npc_umbriss_trogg_dweller() : CreatureScript("npc_umbriss_trogg_dweller"){ }

        struct npc_umbriss_trogg_dwellerAI : public ScriptedAI
        {
            npc_umbriss_trogg_dwellerAI(Creature* creature) : ScriptedAI(creature)
            {
            }
            
            void EnterCombat(Unit* /*attacker*/) override
            {
                events.ScheduleEvent(EVENT_CLAW_PUNCTURE, 5000);
            }

            void UpdateAI(uint32 diff) override
            {
                if (!UpdateVictim())
                    return;

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while(uint32 eventId = events.ExecuteEvent())
                {
                    switch(eventId)
                    {
                    case EVENT_CLAW_PUNCTURE:
                        DoCastVictim(SPELL_CLAW_PUNCTURE);
                        events.ScheduleEvent(EVENT_CLAW_PUNCTURE, urand(5000, 10000));
                        break;
                    }
                }
                DoMeleeAttackIfReady();
            }

        private:
            EventMap events;
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return new npc_umbriss_trogg_dwellerAI(creature);
        }
};

void AddSC_boss_general_umbriss()
{
    new boss_general_umbriss();
    new npc_malignant_trogg();
    new npc_umbriss_trogg_dweller();
}