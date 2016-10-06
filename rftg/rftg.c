/*
 * Race for the Galaxy AI
 *
 * Copyright (C) 2009-2011 Keldon Jones
 *
 * Source file modified by B. Nordli, August 2014.
 *
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

#include "rftg.h"
/*
 * Reasons to restart main loop.
 */
#define RESTART_NEW        1
#define RESTART_NONE       2
#define RESTART_LOAD       3
#define RESTART_RESTORE    4
#define RESTART_UNDO       5
#define RESTART_UNDO_ROUND 6
#define RESTART_UNDO_GAME  7
#define RESTART_REDO       8
#define RESTART_REDO_ROUND 9
#define RESTART_REDO_GAME  10
#define RESTART_REPLAY     11
#define RESTART_CURRENT    12

/*
 * User options.
 */
typedef struct options
{
	/* Number of players */
	int num_players;

	/* Expansion level */
	int expanded;

	/* Player name */
	char *player_name;

	/* Two-player advanced game */
	int advanced;

	/* Disable goals */
	int disable_goal;

	/* Disable takeovers */
	int disable_takeover;

	/* Customize seed */
	int customize_seed;

	/* Seed value */
	unsigned int seed;

	/* Campaign name */
	char *campaign_name;

	/* Display the VP value for cards in hand */
	int vp_in_hand;

	/* Autosave */
	int auto_save;

} options;

extern void reset_status(game *g, int who);
#define FALSE 0
#define TRUE 1

/*
 * Our default options.
 */
static options opt =
{
	2, // num_players
};

/*
 * Current (real) game state.
 */
static game real_game;

/*
 * Current undo position.
 */
static int num_undo;

/*
 * Total number of undo positions saved.
 */
static int max_undo;

/*
 * Whether the game is replaying or not.
 */
static int game_replaying;

/*
 * Choice logs for each player.
 */
static int *orig_log[MAX_PLAYER];

/*
 * Original log sizes for each player.
 */
static int orig_log_size[MAX_PLAYER];

/*
 * Games started (used for random sampling)
 */
static int games_started;

/*
 * Player we're playing as.
 */
static int player_us;

/*
 * We have restarted the main game loop.
 */
static int restart_loop;

static char *goal_description[MAX_GOAL] =
{
	"First to have five VP chips",
	"First to have worlds of all four kinds",
	"First to have three Alien cards",
	"First to discard at end of round",
	"First to have powers in all phases, plus Trade",
	"First to place a 6-cost development giving ? VPs",
	"First to have three Uplift cards",
	"First to have four goods",
	"First to have eight cards",
	"First to have negative Military and two worlds\n"
	  " or a takeover attack power and two Military worlds",
	"First to have two prestige chips and three VP chips",
	"First to have three Imperium cards\n"
	  " or four Military worlds",

	"Most total military",
	"Most Novelty and/or Rare worlds",
	"Most developments",
	"Most production worlds",
	"Most cards with Explore powers",
	"Most Rebel Military worlds",
	"Most prestige chips",
	"Most cards with Consume powers",
};

/*
 * Player names.
 */
static char *player_names[MAX_PLAYER] =
{
	"Blue",
	"Red",
	"Green",
	"Yellow",
	"Cyan",
	"Purple",
};


typedef struct discounts
{
	/* The base discount */
	int base;

	/* The current temporary discount */
	int bonus;

	/* Additional specific discount */
	int specific[6];

	/* May discard to place at zero count */
	int zero;

	/* Additional discount when paying for military */
	int pay_discount;

	/* May pay for military with 0 discount (Rebel Cantina) */
	int non_alien_mil_0;

	/* May pay for military with 1 discount (Contact Specialist) */
	int non_alien_mil_1;

	/* May pay for rebel worlds with 2 discount (Rebel Alliance) */
	int rebel_mil_2;

	/* May pay for chromosome worlds (Ravaged Uplift World) */
	int chromo_mil;

	/* May pay for alien worlds (Alien Research Team) */
	int alien_mil;

	/* May discard to conquer with 0 discount (Imperium Invasion Fleet) */
	int conquer_settle_0;

	/* May discard to conquer with 2 discount (Imperium Cloaking Tech) */
	int conquer_settle_2;

	/* Any value is set */
	int has_data;

} discounts;

typedef struct mil_strength
{
	/* Base military */
	int base;

	/* Current temporary military */
	int bonus;

	/* Maximum additional temporary military */
	int max_bonus;

	/* Additional military against rebel worlds */
	int rebel;

	/* Additional specific military */
	int specific[6];

	/* Additional extra defense during takeovers */
	int defense;

	/* Additional military when using attack imperium TO power */
	int attack_imperium;

	/* Name of attack imperium TO power */
	char imp_card[64];

	/* Imperium world played */
	int imperium;

	/* Rebel military world played */
	int military_rebel;

	/* Any value is set */
	int has_data;

} mil_strength;


/*
 * Restriction on action button.
 */
static int action_min, action_max, action_payment_which, action_payment_mil;
static int action_payment_bonus;
static int action_cidx, action_oidx;



/*
 * Check whether a log position marks a round boundary.
 */
int is_round_boundary(int advanced, int *p)
{
	/* Only start and action choices are boundary */
	if (*p != CHOICE_START && *p != CHOICE_ACTION) return FALSE;

	/* Second choice of Psi-Crystal is not a boundary */
	/* XXX This only works in newer save games */
	if (advanced && *(p + 1) == 2) return FALSE;

	/* Everything else is */
	return TRUE;
}

/*
 * Add text to the message buffer.
 */
void message_add(game *g, char *msg)
{
	printf("message: %s",msg);
}

/*
 * Add formatted text to the message buffer.
 */
void message_add_formatted(game *g, char *msg, char *tag)
{
	printf("message <%s>: %s",tag,msg);
}
/*
 * Add a private message to the message buffer.
 */
void message_add_private(game *g, int who, char *msg, char *tag)
{
	/* Verify we are the correct player */
	if (who == player_us)
	{
		/* Add message */
		message_add_formatted(g, msg, tag);
	}
}

/*
 * Handle an error dialog with a message.
 */
void display_error(char *msg)
{
	printf("error: %s\n",msg);
}
/*
 * Use simple random number generator.
 */
int game_rand(game *g)
{
	/* Call simple random number generator */
	return simple_rand(&g->random_seed);
}

/*
 * Compute the military/cost needed for a military world.
 */
static void military_world_payment(game *g, int who, int which,
                                   int mil_only, int mil_bonus, discounts *d_ptr,
                                   int *military, int *cost, char **cost_card)
{
	card *c_ptr;
	int strength, pay_for_mil;

	/* Get card */
	c_ptr = &g->deck[which];

	/* Get current strength */
	strength = strength_against(g, who, which, -1, 0) + mil_bonus;

	/* Compute extra military needed */
	*military = c_ptr->d_ptr->cost - strength;

	/* Do not reduce below 0 */
	if (*military <= 0) *military = 0;

	/* Reset cost and pay-for-military */
	pay_for_mil = *cost = -1;

	/* Check for no pay-for-military available */
	if (mil_only) return;

	/* Check for Rebel Alliance */
	if (d_ptr->rebel_mil_2 && (c_ptr->d_ptr->flags & FLAG_REBEL))
	{
		/* Set reduction to 2 */
		pay_for_mil = 2;

		/* Save card name */
		*cost_card = "Rebel Alliance";
	}

	/* Check for Contact Specialist */
	else if (d_ptr->non_alien_mil_1 &&
	         c_ptr->d_ptr->good_type != GOOD_ALIEN)
	{
		/* Set reduction to 1 */
		pay_for_mil = 1;

		/* Save card name */
		*cost_card = "Contact Specialist";
	}

	/* Check for Rebel Cantina */
	else if (d_ptr->non_alien_mil_0 &&
	         c_ptr->d_ptr->good_type != GOOD_ALIEN)
	{
		/* Set reduction to 0 */
		pay_for_mil = 0;

		/* Save card name */
		*cost_card = "Rebel Cantina";
	}

	/* Check for Alien Research Team */
	else if (d_ptr->alien_mil &&
	         c_ptr->d_ptr->good_type == GOOD_ALIEN)
	{
		/* Set reduction to 0 */
		pay_for_mil = 0;

		/* Save card name */
		*cost_card = "Alien Research Team";
	}

	/* Check for Ravaged Uplift World */
	else if (d_ptr->chromo_mil && c_ptr->d_ptr->flags & FLAG_CHROMO)
	{
		/* Set reduction to 0 */
		pay_for_mil = 0;

		/* Save card name */
		*cost_card = "Ravaged Uplift World";
	}

	/* Check for any pay-for-military power */
	if (pay_for_mil >= 0)
	{
		/* Compute cost */
		*cost = c_ptr->d_ptr->cost - d_ptr->base - d_ptr->bonus -
		        d_ptr->specific[c_ptr->d_ptr->good_type] -
		        pay_for_mil - d_ptr->pay_discount;

		/* Do not reduce below 0 */
		if (*cost < 0) *cost = 0;
	}
}

/*
 * Compute the cost/military needed for a non-military world.
 */
static void peaceful_world_payment(game *g, int who, int which,
                                   int mil_only, discounts *d_ptr,
                                   int *cost, int *ict_mil, int *iif_mil)
{
	card *c_ptr;
	int strength;

	/* Get card */
	c_ptr = &g->deck[which];

	/* Check for no normal payment available */
	if (mil_only)
	{
		/* Disable payment */
		*cost = -1;
	}
	else
	{
		/* Compute cost */
		*cost = c_ptr->d_ptr->cost - d_ptr->base - d_ptr->bonus -
				d_ptr->specific[c_ptr->d_ptr->good_type];

		/* Do not reduce below 0 */
		if (*cost < 0) *cost = 0;
	}

	/* Compute strength */
	strength = strength_against(g, who, which, -1, 0);

	/* Reset ICT/IIF military */
	*ict_mil = *iif_mil = -1;

	/* Check for Imperium Cloaking Technology */
	if (d_ptr->conquer_settle_2)
	{
		/* Compute extra military needed */
		*ict_mil = c_ptr->d_ptr->cost - strength - 2;

		/* Do not reduce below 0 */
		if (*ict_mil < 0) *ict_mil = 0;
	}

	/* Check for Imperium Invasion Fleet */
	if (d_ptr->conquer_settle_0)
	{
		/* Compute extra military needed */
		*iif_mil = c_ptr->d_ptr->cost - strength;

		/* Do not reduce below 0 */
		if (*iif_mil < 0) *iif_mil = 0;
	}
}

int callbuffer[4096];
int *get_callbuffer(void) {
	return callbuffer;
}

/*
 * Function to determine whether selected cards meet payment.
 */
int action_check_payment(int which, int n, int ns, int mil, int bonus)
{
	game sim;
	int i;
	int *list = callbuffer, *special = callbuffer + n;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;
	sim.game_over = 0;

	/* Loop over players */
	for (i = 0; i < sim.num_players; i++)
	{
		/* Have AI make any pending decisions for this player */
		sim.p[i].control = &ai_func;
	}

	/* Try to make payment */
	return payment_callback(&sim, player_us, which,
	                        list, n, special, ns, mil,
	                        bonus);
}

/*
 * Function to determine whether selected goods can be consumed.
 */
int action_check_goods(int cidx, int oidx, int n)
{
	game sim;
	int *list = callbuffer;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;

	/* Try to make payment */
	return good_chosen(&sim, player_us, cidx, oidx, list, n);
}

/*
 * Function to determine whether selected card can be taken over.
 */
int action_check_takeover(int target, int special)
{
	game sim;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;
        sim.game_over = 0;

	/* Check takeover legality */
	return takeover_callback(&sim, special, target);
}

/*
 * Function to determine whether selected cards are a legal defense.
 */
int action_check_defend(int n, int ns)
{
	game sim;
	int *list = callbuffer, *special = callbuffer + n;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;
        sim.game_over = 0;

	/* Try to defend (we don't care about win/lose, just legality */
	return defend_callback(&sim, player_us, 0, list, n, special, ns);
}

/*
 * Function to determine whether selected cards are a legal world upgrade.
 */
int action_check_upgrade(int upgrade, int upgraded)
{
	game sim;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;
        sim.game_over = 0;

	/* Try to upgrade */
	return upgrade_chosen(&sim, player_us, upgrade, upgraded);
}

/*
 * Function to determine whether selected cards are legal to consume.
 */
int action_check_consume(int cidx, int oidx, int n)
{
	game sim;
	int *list = callbuffer;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;

	/* Try to consume */
	return consume_hand_chosen(&sim, player_us, cidx, oidx, list, n);
}

/*
 * Return whether the selected world and hand is a valid start.
 */
int action_check_start(int n, int ns)
{
	game sim;
	int *list = callbuffer, *special = callbuffer + n;

	/* Check for exactly 1 world selected */
	if (ns != 1) return 0;

	/* Copy game */
	sim = real_game;

	/* Set simulation flag */
	sim.simulation = 1;

	/* Try to start */
	return start_callback(&sim, player_us, list, n, special, ns);
}



/*
 * Compute settle discounts for a player.
 */
static void compute_discounts(game *g, int who, discounts *d_ptr)
{
	power_where w_list[100];
	power *o_ptr;
	int i, n;

	/* Clear discounts */
	memset(d_ptr, 0, sizeof(discounts));

	/* Set bonus discounts */
	d_ptr->bonus = g->p[who].bonus_reduce;

	/* Check for prestige settle */
	if ((g->cur_action == ACT_SETTLE || g->cur_action == ACT_SETTLE2) &&
	    player_chose(g, who, ACT_PRESTIGE | g->cur_action))
	{
		/* Add prestige bonus */
		d_ptr->bonus += 3;
	}

	/* Get settle phase powers */
	n = get_powers(g, who, PHASE_SETTLE, w_list);

	/* Loop over powers */
	for (i = 0; i < n; i++)
	{
		/* Get power pointer */
		o_ptr = w_list[i].o_ptr;

		/* Check discard for 0 */
		if (o_ptr->code == (P3_DISCARD | P3_REDUCE_ZERO))
			d_ptr->zero += 1;

		/* Check for reduce power */
		if (o_ptr->code & P3_REDUCE)
		{
			/* Check for general discount */
			if (o_ptr->code == P3_REDUCE)
				d_ptr->base += o_ptr->value;

			/* Check for discount against Novelty worlds */
			if (o_ptr->code & P3_NOVELTY)
				d_ptr->specific[GOOD_NOVELTY] += o_ptr->value;

			/* Check for discount against Rare worlds */
			if (o_ptr->code & P3_RARE)
				d_ptr->specific[GOOD_RARE] += o_ptr->value;

			/* Check for discount against Genes worlds */
			if (o_ptr->code & P3_GENE)
				d_ptr->specific[GOOD_GENE] += o_ptr->value;

			/* Check for discount against Alien worlds */
			if (o_ptr->code & P3_ALIEN)
				d_ptr->specific[GOOD_ALIEN] += o_ptr->value;
		}

		/* Check for pay-for-military powers */
		if (o_ptr->code & P3_PAY_MILITARY)
		{
			/* Check for non-alien power without discount */
			if (o_ptr->code == P3_PAY_MILITARY && o_ptr->value == 0)
				d_ptr->non_alien_mil_0 = TRUE;

			/* Check for non-alien power with discount */
			if (o_ptr->code == P3_PAY_MILITARY && o_ptr->value == 1)
				d_ptr->non_alien_mil_1 = TRUE;

			/* Check for rebel flag */
			if (o_ptr->code & P3_AGAINST_REBEL)
				d_ptr->rebel_mil_2 = TRUE;

			/* Check for chromo flag */
			if (o_ptr->code & P3_AGAINST_CHROMO)
				d_ptr->chromo_mil = TRUE;

			/* Check for alien flag */
			if (o_ptr->code & P3_ALIEN)
				d_ptr->alien_mil = TRUE;
		}

		/* Check for pay-for-military discount */
		if (o_ptr->code & P3_PAY_DISCOUNT)
			d_ptr->pay_discount += o_ptr->value;

		/* Check for conquer settle without discount */
		if ((o_ptr->code & P3_CONQUER_SETTLE) && o_ptr->value == 0)
			d_ptr->conquer_settle_0 = TRUE;

		/* Check for conquer settle with discount */
		if ((o_ptr->code & P3_CONQUER_SETTLE) && o_ptr->value == 2)
			d_ptr->conquer_settle_2 = TRUE;
	}

	/* Check for any modifiers */
	d_ptr->has_data = d_ptr->base || d_ptr->bonus ||
		d_ptr->specific[GOOD_NOVELTY] || d_ptr->specific[GOOD_RARE] ||
		d_ptr->specific[GOOD_GENE] || d_ptr->specific[GOOD_ALIEN] ||
		d_ptr->zero || d_ptr->pay_discount ||
		d_ptr->non_alien_mil_0 || d_ptr->non_alien_mil_1 ||
		d_ptr->rebel_mil_2 || d_ptr->chromo_mil || d_ptr->alien_mil ||
		d_ptr->conquer_settle_0 || d_ptr->conquer_settle_2;
}

/*
 * Compute military strength for a player.
 */
static void compute_military(game *g, int who, mil_strength *m_ptr)
{
	card *c_ptr;
	power *o_ptr;
	int x, i, hand_size, hand_military = 0, rare_goods;

	/* Start strengths at 0 */
	memset(m_ptr, 0, sizeof(mil_strength));

	/* Begin with base military strength */
	m_ptr->base = total_military(g, who);

	/* Set bonus military */
	m_ptr->bonus = g->p[who].bonus_military;

	/* Get first active card */
	x = g->p[who].start_head[WHERE_ACTIVE];

	/* Count number of rare goods */
	rare_goods = get_goods(g, who, NULL, GOOD_RARE);

	/* Loop over cards */
	for ( ; x != -1; x = g->deck[x].start_next)
	{
		/* Get card pointer */
		c_ptr = &g->deck[x];

		/* Loop over card's powers */
		for (i = 0; i < c_ptr->d_ptr->num_power; i++)
		{
			/* Get power pointer */
			o_ptr = &c_ptr->d_ptr->powers[i];

			/* Skip incorrect phase */
			if (o_ptr->phase != PHASE_SETTLE) continue;

			/* Check for discard power */
			if ((o_ptr->code & P3_DISCARD) && c_ptr->where == WHERE_DISCARD)
				continue;

			/* Check for defense power */
			if (o_ptr->code & P3_TAKEOVER_DEFENSE && takeovers_enabled(g))
			{
				/* Add defense for military worlds */
				m_ptr->defense +=
					count_active_flags(g, who, FLAG_MILITARY);

				/* Add extra defense for Rebel military worlds */
				m_ptr->defense +=
					count_active_flags(g, who, FLAG_REBEL | FLAG_MILITARY);
			}

			/* Check for takeover imperium power */
			if (o_ptr->code & P3_TAKEOVER_IMPERIUM && takeovers_enabled(g))
			{
				/* Set imperium attack */
				m_ptr->attack_imperium =
					2 * count_active_flags(g, who, FLAG_REBEL | FLAG_MILITARY);

				/* Check if card name already set */
				if (strlen(m_ptr->imp_card))
				{
					/* XXX Use name of both cards */
					strcpy(m_ptr->imp_card, "Rebel Alliance/Rebel Sneak Attack");
				}
				else
				{
					/* Remember name of card */
					strcpy(m_ptr->imp_card, c_ptr->d_ptr->name);
				}
			}

			/* Skip used powers */
			if (c_ptr->misc & (1 << (MISC_USED_SHIFT + i))) continue;

			/* Check for military from hand */
			if (o_ptr->code & P3_MILITARY_HAND)
				hand_military += o_ptr->value;

			/* Skip non-military powers */
			if (!(o_ptr->code & P3_EXTRA_MILITARY)) continue;

			/* Check for discard for military */
			if (o_ptr->code & P3_DISCARD)
				m_ptr->max_bonus += o_ptr->value;

			/* Check for prestige for military */
			if ((o_ptr->code & P3_CONSUME_PRESTIGE) && g->p[who].prestige)
				m_ptr->max_bonus += o_ptr->value;

			/* Check for good for military */
			if ((o_ptr->code & P3_CONSUME_RARE) && rare_goods)
			{
				m_ptr->max_bonus += o_ptr->value;
				--rare_goods;
			}

			/* Check for strength against rebels */
			if (o_ptr->code & P3_AGAINST_REBEL)
				m_ptr->rebel += o_ptr->value;

			/* Check for strength against Novelty worlds */
			if (o_ptr->code & P3_NOVELTY)
				m_ptr->specific[GOOD_NOVELTY] += o_ptr->value;

			/* Check for strength against Rare worlds */
			if (o_ptr->code & P3_RARE)
				m_ptr->specific[GOOD_RARE] += o_ptr->value;

			/* Check for strength against Genes worlds */
			if (o_ptr->code & P3_GENE)
				m_ptr->specific[GOOD_GENE] += o_ptr->value;

			/* Check for strength against Alien worlds */
			if (o_ptr->code & P3_ALIEN)
				m_ptr->specific[GOOD_ALIEN] += o_ptr->value;
		}
	}

	/* Get player hand size */
	hand_size = count_player_area(g, who, WHERE_HAND);

	/* Reduce maximum military from hand */
	if (hand_size < hand_military) hand_military = hand_size;

	/* Add military from hand to max temporary military */
	m_ptr->max_bonus += hand_military;

	/* Check for takeovers enabled and imperium card played */
	m_ptr->imperium = takeovers_enabled(g) &&
		count_active_flags(g, who, FLAG_IMPERIUM);

	/* Check for takeovers enabled and rebel military world played */
	m_ptr->military_rebel = takeovers_enabled(g) &&
		count_active_flags(g, who, FLAG_MILITARY | FLAG_REBEL);

	/* Check for any modifiers */
	m_ptr->has_data = m_ptr->base || m_ptr->bonus || m_ptr->rebel ||
		m_ptr->specific[GOOD_NOVELTY] || m_ptr->specific[GOOD_RARE] ||
		m_ptr->specific[GOOD_GENE] || m_ptr->specific[GOOD_ALIEN] ||
		m_ptr->defense || m_ptr->attack_imperium || m_ptr->imperium ||
		m_ptr->military_rebel || m_ptr->max_bonus;
}


/*
 * Reset list of displayed cards on the table for the given player.
 */


/*
 * Number of action buttons pressed.
 */
static int actions_chosen;

/*
 * Action which is receiving prestige boost.
 */
static int prestige_action;


/*
 * Return a "score" for sorting consume powers.
 */
static int score_consume(power *o_ptr)
{
	int vp = 0, card = 0, prestige = 0, goods = 1;
	int vp_mult = 1, score = 0;

	/* Check for discard form hand */
	if (o_ptr->code & P4_DISCARD_HAND)
	{
		/* Always discard from hand last */
		score -= 1000;

		/* Check for VP awarded */
		if (o_ptr->code & P4_GET_VP) vp += o_ptr->value;

		/* Check for card awarded */
		if (o_ptr->code & P4_GET_CARD) card += o_ptr->value;

		/* Check for prestige awarded */
		if (o_ptr->code & P4_GET_PRESTIGE) prestige += o_ptr->value;

		/* Check for consuming two cards */
		if (o_ptr->code & P4_CONSUME_TWO) goods = 2;

		/* Compute score */
		score += (card * 150 + prestige * 100 + vp * 75) / goods;

		/* Use multi-use powers later */
		if (o_ptr->times > 1) score -= 2 * o_ptr->times;

		/* Return score */
		return score;
	}

	/* Check for consume prestige */
	if (o_ptr->code & P4_CONSUME_PRESTIGE)
	{
		/* Consume prestige next to last */
		score -= 500;

		/* Check for VP awarded */
		if (o_ptr->code & P4_GET_VP) score += o_ptr->value * 2;

		/* Check for cards awarded */
		if (o_ptr->code & P4_GET_CARD) score += o_ptr->value;

		/* Return score */
		return score;
	}

	/* Check for free VP */
	if (o_ptr->code & P4_VP) return o_ptr->value * 1000;

	/* Check for free card draw */
	if (o_ptr->code & P4_DRAW) return o_ptr->value * 750;

	/* Check for VP awarded */
	if (o_ptr->code & P4_GET_VP) vp += o_ptr->value;

	/* Check for card awarded */
	if (o_ptr->code & P4_GET_CARD) card += o_ptr->value;

	/* Check for cards awarded */
	if (o_ptr->code & P4_GET_2_CARD) card += o_ptr->value * 2;
	if (o_ptr->code & P4_GET_3_CARD) card += o_ptr->value * 3;

	/* Check for prestige awarded */
	if (o_ptr->code & P4_GET_PRESTIGE) prestige += o_ptr->value;

	/* Assume trade will earn 4 cards */
	if (o_ptr->code & P4_TRADE_ACTION) card += 4;

	/* Assume trade without bonus will earn fewer cards */
	if (o_ptr->code & P4_TRADE_NO_BONUS) card--;

	/* Check for consuming two goods */
	if (o_ptr->code & P4_CONSUME_TWO) goods = 2;

	/* Check for consuming three goods */
	if (o_ptr->code & P4_CONSUME_3_DIFF) goods = 3;

	/* Check for consuming all goods */
	if (o_ptr->code & P4_CONSUME_ALL) goods = 4;

	/* Check for double VP action */
	if (player_chose(&real_game, player_us, ACT_CONSUME_X2) ||
	    player_chose(&real_game, player_us, ACT_CONSUME_TRADE | ACT_PRESTIGE))
	{
		/* Multiplier is two */
		vp_mult = 2;
	}

	/* Check for triple VP action */
	if (player_chose(&real_game, player_us, ACT_PRESTIGE | ACT_CONSUME_X2))
	{
		/* Multiplier is three */
		vp_mult = 3;
	}

	/* Compute score */
	score = (prestige * 150 + vp * vp_mult * 100 + card * 52) / goods;

	/* Use specific consume powers first */
	if (!(o_ptr->code & P4_CONSUME_ANY)) score += 10;

	/* Use multi-use powers later */
	if (o_ptr->times > 1) score -= 2 * o_ptr->times;

	/* Return score */
	return score;
}

typedef struct pow_loc
{
	/* Card index */
	int c_idx;

	/* Power index */
	int o_idx;

} pow_loc;

/*
 * Compare two consume powers for sorting.
 */
static int cmp_consume(const void *l1, const void *l2)
{
	pow_loc *l_ptr1 = (pow_loc *)l1;
	pow_loc *l_ptr2 = (pow_loc *)l2;
	power *o_ptr1, *o_ptr2, bonus;

	/* Check first power */
	if (l_ptr1->c_idx < 0)
	{
		/* Use bonus power */
		bonus.phase = PHASE_CONSUME;
		bonus.code = P4_DISCARD_HAND | P4_GET_VP;
		bonus.value = 1;
		bonus.times = 2;

		/* Use fake power */
		o_ptr1 = &bonus;
	}
	else
	{
		/* Get power */
		o_ptr1 = &real_game.deck[l_ptr1->c_idx].d_ptr->powers[l_ptr1->o_idx];
	}

	/* Check second power */
	if (l_ptr2->c_idx < 0)
	{
		/* Use bonus power */
		bonus.phase = PHASE_CONSUME;
		bonus.code = P4_DISCARD_HAND | P4_GET_VP;
		bonus.value = 1;
		bonus.times = 2;

		/* Use fake power */
		o_ptr2 = &bonus;
	}
	else
	{
		/* Get power */
		o_ptr2 = &real_game.deck[l_ptr2->c_idx].d_ptr->powers[l_ptr2->o_idx];
	}

	/* Compare consume powers */
	return score_consume(o_ptr2) - score_consume(o_ptr1);
}


/*
 * Return a "score" for sorting produce powers.
 */
static int score_produce(power *o_ptr)
{
	int score = 0;

	/* List non-discard powers first */
	if (!(o_ptr->code & P5_DISCARD)) score += 10;

	/* Score not this slightly above */
	if (o_ptr->code & P5_NOT_THIS) score += 1;

	/* Score specific powers before generic */
	if (o_ptr->code & P5_WINDFALL_NOVELTY) score += 8;
	if (o_ptr->code & P5_WINDFALL_RARE) score += 6;
	if (o_ptr->code & P5_WINDFALL_GENE) score += 4;
	if (o_ptr->code & P5_WINDFALL_ALIEN) score += 2;

	/* List draw powers last */
	if (o_ptr->code & P5_DRAW_EACH_NOVELTY) score = -2;
	if (o_ptr->code & P5_DRAW_EACH_RARE) score = -4;
	if (o_ptr->code & P5_DRAW_EACH_GENE) score = -6;
	if (o_ptr->code & P5_DRAW_EACH_ALIEN) score = -8;
	if (o_ptr->code & P5_DRAW_DIFFERENT) score = -10;

	/* Return score */
	return score;
}

/*
 * Compare two produce powers for sorting.
 */
static int cmp_produce(const void *l1, const void *l2)
{
	pow_loc *l_ptr1 = (pow_loc *)l1;
	pow_loc *l_ptr2 = (pow_loc *)l2;
	power *o_ptr1;
	power *o_ptr2;
	power bonus;

	/* Check first power */
	if (l_ptr1->c_idx < 0)
	{
		/* Use bonus power */
		bonus.code = P5_WINDFALL_ANY;
		o_ptr1 = &bonus;
	}
	else
	{
		/* Get power */
		o_ptr1 = &real_game.deck[l_ptr1->c_idx].d_ptr->powers[l_ptr1->o_idx];
	}

	/* Check second power */
	if (l_ptr2->c_idx < 0)
	{
		/* Use bonus power */
		bonus.code = P5_WINDFALL_ANY;
		o_ptr2 = &bonus;
	}
	else
	{
		/* Get power */
		o_ptr2 = &real_game.deck[l_ptr2->c_idx].d_ptr->powers[l_ptr2->o_idx];
	}

	/* Compare produce powers */
	return score_produce(o_ptr2) - score_produce(o_ptr1);
}

/*
 * Player spots have been rotated.
 */
static void gui_notify_rotation(game *g, int who)
{
	/* Remember our new player index */
	player_us--;

	/* Handle wraparound */
	if (player_us < 0) player_us = real_game.num_players - 1;
}

/*
 * Auto save during the game.
 */
static void auto_save(game *g, int who)
{
	/* Check for autosave disabled */
	if (!opt.auto_save) return;

	/* Save to file */
	if (save_game(g, "autosave.rftg", who) < 0)
	{
		/* Error */
	}
}

/*
 * Load an auto save file.
 */
static int load_auto_save(game *g)
{
	int i;

	/* Loop over players */
	for (i = 0; i < MAX_PLAYER; i++)
	{
		/* Set choice log pointer */
		g->p[i].choice_log = orig_log[i];
	}

	/* Try to load savefile into game */
	if (load_game(g, "autosave.rftg") < 0)
	{
		/* Give up */
		return FALSE;
	}

	/* Loop over players */
	for (i = 0; i < g->num_players; ++i)
	{
		/* Remember log size */
		orig_log_size[i] = g->p[i].choice_size;
	}

	/* Force current game over */
	g->game_over = 1;

	opt.num_players = g->num_players;
	opt.advanced = g->advanced;
	opt.expanded = g->expanded;
	opt.disable_goal = g->goal_disabled;
	opt.disable_takeover = g->takeover_disabled;

	/* Game successfully loaded */
	return TRUE;
}

/*
 * Should be called when a choice is done, in order to update undo information.
 */
static void choice_done(game *g)
{
	int i;

	/* Loop over all players */
	for (i = 0; i < g->num_players; ++i)
	{
		/* Skip human player */
		if (i != player_us)
		{
			/* Reset size of log */
			g->p[i].choice_size = g->p[i].choice_unread_pos;
		}

		/* Remember new log size */
		orig_log_size[((i-player_us) + g->num_players) % g->num_players] =
			g->p[i].choice_size;
	}

	/* Stop game replaying */
	game_replaying = FALSE;

	/* Add one to undo position */
	++num_undo;

	/* Clear redo possibility */
	max_undo = num_undo;
}
/*
 * Reset status information for a player.
 */

static int status_data[10000], status_data_ptr;
static void reset_status_data(void) { status_data_ptr = 0; }
static void add_data(int v) { status_data[status_data_ptr++] = v; }
int *get_status_data(void) { return status_data; }
static int *selection_result_ptr;
static int *selection_result_len_ptr;
int *selection_result(int len) {
	*selection_result_len_ptr += len;
	return selection_result_ptr;
}


void get_vp(game *g, int who)
{
	player *p_ptr = &g->p[who];
	card *c_ptr;
	int x, kind, worlds = 0, devs = 0, dev6 = 0;

	/* Remember old kind */
	kind = g->oort_kind;

	/* Set oort kind to best scoring kind */
	g->oort_kind = g->best_oort_kind;

	/* Loop over active cards */
	for (x = p_ptr->head[WHERE_ACTIVE] ; x != -1; x = g->deck[x].next)
	{
		c_ptr = &g->deck[x];
		if (c_ptr->d_ptr->type == TYPE_WORLD)
			worlds += c_ptr->d_ptr->vp;
		else if (c_ptr->d_ptr->type == TYPE_DEVELOPMENT)
			devs += c_ptr->d_ptr->vp;

		if (c_ptr->d_ptr->num_vp_bonus)
			dev6 += get_score_bonus(g, who, x);
	}
        add_data(p_ptr->goal_vp);
	add_data(worlds);
        add_data(devs);
        add_data(dev6);

	/* Reset oort kind */
	g->oort_kind = kind;
}

void reset_status(game *g, int who)
{
	int i;
	int act1 = -1, act2 = -1, vp, end_vp, prestige;
	/* Settle discount */
	discounts discount;
	/* Military strength */
	mil_strength military;
	int goal_display[MAX_GOAL];
	int goal_gray[MAX_GOAL];
	int hand_size = 0;

	for (i = 0; i < g->deck_size; i++)
		if (g->deck[i].where == WHERE_HAND && g->deck[i].owner == who) hand_size++;

	/* Check for actions known */
	if (g->advanced && g->cur_action < ACT_SEARCH && who == player_us &&
	    count_active_flags(g, player_us, FLAG_SELECT_LAST))
	{
		/* Copy first action only */
		act1 = g->p[who].action[0];
	}
	else if (g->cur_action >= ACT_SEARCH ||
	         count_active_flags(g, player_us, FLAG_SELECT_LAST))
	{
		/* Copy actions */
		act1 = g->p[who].action[0];
		act2 = g->p[who].action[1];
	}

	/* Copy VP chips */
	vp = g->p[who].vp;
	end_vp = g->p[who].end_vp;

	/* Copy prestige */
	prestige = g->p[who].prestige;
        for (i = 0; i < g->num_players; i++)
            if (i != who && g->p[i].prestige > prestige) break;
        prestige <<= 3;
        if (i == g->num_players) prestige += 4;
        if (g->p[who].prestige_turn) prestige += 2;
        if (g->p[who].prestige_action_used) prestige += 1;

	/* Count general discount */
	compute_discounts(g, who, &discount);

	/* Count military strength */
	compute_military(g, who, &military);

	/* Loop over goals */
	for (i = 0; i < MAX_GOAL; i++)
	{
		/* Assume goal is not displayed */
		goal_display[i] = 0;

		/* Assume goal is not grayed */
		goal_gray[i] = 0;

		/* Skip inactive goals */
		if (!g->goal_active[i]) continue;

		/* Check for "first" goal */
		if (i <= GOAL_FIRST_4_MILITARY)
		{
			/* Check for unclaimed */
			if (!g->p[who].goal_claimed[i]) continue;
		}
		else
		{
			/* Check for insufficient progress */
			if (g->p[who].goal_progress[i] < goal_minimum(i))
				continue;

			/* Check for less progress than other players */
			if (g->p[who].goal_progress[i] < g->goal_most[i])
				continue;

			/* Unclaimed goals should be gray */
			if (!g->p[who].goal_claimed[i])
				goal_gray[i] = 1;
		}

		/* Goal should be displayed */
		goal_display[i] = 1;

	}
//	printf("Player %d: actions %d/%d, vp %d/%d, prestige %d, goals ",who,act1,act2,vp,end_vp,prestige);
	add_data(act1);
	add_data(act2);
	add_data(vp);
	add_data(end_vp);
	add_data(hand_size);
	add_data(military.base);
	add_data(prestige);
	get_vp(g, who);
//	for (i = 0; i < MAX_GOAL; i++) if (g->goal_active[i]) printf("%d", goal_display[i] ? goal_gray[i] ? 2 : 1 : 0);
//	printf("\n");
}

static void get_state(game *g) {
	card *c_ptr;
	int i, display_deck = 0, display_discard = 0, display_pool;

	/* Get chips in VP pool */
	display_pool = g->vp_pool;

	/* Loop over cards in deck */
	for (i = 0; i < g->deck_size; i++)
	{
		/* Get card pointer */
		c_ptr = &g->deck[i];

		if (c_ptr->where == WHERE_DECK) display_deck++;

		/* Check for card in discard pile */
		if (c_ptr->where == WHERE_DISCARD) display_discard++;

//		printf("%3d %d %d %d %d %d %s\n", i, c_ptr->owner, c_ptr->where, c_ptr->d_ptr->cost, c_ptr->order, c_ptr->d_ptr->index, c_ptr->d_ptr->name);

		/* Skip unowned cards */
		if (!(c_ptr->where == WHERE_ACTIVE) && !(c_ptr->where == WHERE_HAND && c_ptr->owner == player_us)) continue;

		add_data(i);
		add_data(c_ptr->d_ptr->index);
		add_data(c_ptr->where == WHERE_HAND ? -1 : (c_ptr->owner + g->num_players - player_us) % g->num_players);
		add_data(c_ptr->where == WHERE_HAND ? (c_ptr->start_where != WHERE_HAND ||
				    c_ptr->start_owner != c_ptr->owner ? 200000 : 0) + (c_ptr->d_ptr->type == TYPE_DEVELOPMENT ? 100000 : 0) + c_ptr->d_ptr->cost * 10000 + i : c_ptr->order * 10000 + i);
                add_data(c_ptr->num_goods);
	}
//	printf("Deck: %d, Discard: %d, VP Pool: %d\n", display_deck, display_discard, display_pool);
	add_data(-1);
	add_data(display_deck);
	add_data(display_discard);
	add_data(display_pool);
	for (i = ACT_EXPLORE_5_0; i <= ACT_PRODUCE; i++)
	{
		int c;
		/* Skip second explore/consume actions */
		if (i == ACT_EXPLORE_1_1 || i == ACT_CONSUME_X2) continue;

		/* Check for basic game and advanced actions */
		if (!g->advanced &&
		    (i == ACT_DEVELOP2 || i == ACT_SETTLE2))
		{
			/* Skip action */
			continue;
		}

		/* Check for inactive phase */
		if (!g->action_selected[i])
		{
			/* Desaturate */
			c = 0;
		} else {
			c = g->cur_action == i ? 2 : 1;
		}
		add_data(c);

	}

	score_game(g);
	/* Loop over players */
	for (i = 0; i < g->num_players; i++)
	{
		/* Reset status information for player */
		reset_status(g, (i + player_us) % g->num_players);
	}
}

/*
 * Make a choice of the given type.
 */
static void gui_make_choice(game *g, int who, int type, int list[], int *nl,
                           int special[], int *ns, int arg1, int arg2, int arg3)
{
	int i;

	/* Auto save */
	auto_save(g, who);

	reset_status_data();
//	printf("TYPE: %d\n", type);
	add_data(type);
	add_data(nl ? *nl : 0);
	add_data(ns ? *ns : 0);
	add_data(arg1);
	add_data(arg2);
	add_data(arg3);
	if (nl) for (i = 0; i < *nl; i++) add_data(list[i]);
	if (ns) for (i = 0; i < *ns; i++) add_data(special[i]);
	get_state(g);
	selection_result_len_ptr = &g->p[who].choice_size;
	selection_result_ptr = &g->p[who].choice_log[g->p[who].choice_size];
	g->game_over = 1;
        restart_loop = RESTART_REDO_GAME;
}

/*
 * Interface to GUI decision functions.
 */
decisions gui_func =
{
	NULL,
	gui_notify_rotation,
	NULL,
	gui_make_choice,
	NULL,
	NULL,
	NULL,
	NULL,
	message_add_private,
};

/*
 * Apply options to game structure.
 */
static void apply_options(void)
{
	int i;

	/* Sanity check number of players in base game */
	if (opt.expanded < 1 && opt.num_players > 4)
	{
		/* Reset to four players */
		opt.num_players = 4;
	}

	/* Sanity check number of players in first or fourth expansion */
	if ((opt.expanded < 2 || opt.expanded == 4) && opt.num_players > 5)
	{
		/* Reset to five players */
		opt.num_players = 5;
	}

	/* Set name of human player */
	real_game.human_name = opt.player_name;

	/* Set number of players */
	real_game.num_players = opt.num_players;

	/* Set expansion level */
	real_game.expanded = opt.expanded;

	/* Set advanced flag */
	real_game.advanced = opt.advanced;

	/* Set promo flag */
	real_game.promo = 0;

	/* Set goals disabled */
	real_game.goal_disabled = opt.disable_goal;

	/* Set takeover disabled */
	real_game.takeover_disabled = opt.disable_takeover;

	/* Check for custom seed value */
	if (opt.customize_seed)
	{
		/* Set start seed */
		real_game.random_seed = opt.seed;
	}
	else
	{
		/* Set random seed */
		real_game.random_seed = time(NULL) + games_started++;
	}

	/* Sanity check advanced mode */
	if (real_game.num_players > 2)
	{
		/* Clear advanced mode */
		real_game.advanced = 0;
	}

	/* Assume no campaign */
	real_game.camp = NULL;

	/* Check for campaign name set */
	if (opt.campaign_name)
	{
		/* Loop over available campaigns */
		for (i = 0; i < num_campaign; i++)
		{
			/* Check for match */
			if (!strcmp(opt.campaign_name, camp_library[i].name))
			{
				/* Set campaign */
				real_game.camp = &camp_library[i];
			}
		}
	}

	/* Apply campaign options (number of players, etc) */
	apply_campaign(&real_game);
}

/*
 * Reset player structures.
 */
void reset_gui(void)
{
	int i;

	/* Reset our player index */
	player_us = 0;

	/* Loop over all possible players */
	for (i = 0; i < MAX_PLAYER; i++)
	{
		/* Check for name already set for human player */
		if (i == player_us && real_game.human_name &&
		    strlen(real_game.human_name))
		{
			/* Load name */
			real_game.p[i].name = real_game.human_name;
		}
		else
		{
			/* Set name */
			real_game.p[i].name = player_names[i];
		}

		/* Restore choice log */
		real_game.p[i].choice_log = orig_log[i];
		real_game.p[i].choice_pos = 0;
		real_game.p[i].choice_unread_pos = 0;

		/* Log size already set when game is loaded or replayed */
		if (restart_loop != RESTART_LOAD && restart_loop != RESTART_REPLAY)
		{
			/* Set size of player's logs */
			real_game.p[i].choice_size = orig_log_size[i];
		}
	}

	/* Restore player control functions */
	real_game.p[player_us].control = &gui_func;
	real_game.p[player_us].ai = FALSE;

	/* Loop over AI players */
	for (i = 1; i < MAX_PLAYER; i++)
	{
		/* Set control to AI functions */
		real_game.p[i].control = &ai_func;
		real_game.p[i].ai = TRUE;

		/* Call initialization function */
		real_game.p[i].control->init(&real_game, i, 0.0);
	}

}


/*
 * Run games forever.
 */
static void run_game(void)
{
	{
		int pos, choice, saved_choice;
		char buf[1024];
		/* Replay by default */
		game_replaying = TRUE;

		/* Check for new game starting */
		if (restart_loop == RESTART_NEW)
		{
			int i;
			/* Read parameters from options */
			apply_options();

			/* Reset our position and GUI elements */
			reset_gui();

			/* Initialize game */
			init_game(&real_game);

			/* Do not force seed for next game */
			opt.customize_seed = FALSE;

			/* Reset undo positions */
			num_undo = 0;
			max_undo = 0;

			/* Loop over players */
			for (i = 0; i < real_game.num_players; i++)
			{
				/* Clear choice log */
				real_game.p[i].choice_size = 0;
				orig_log_size[i] = 0;
			}

			/* Unset replaying flag */
			game_replaying = FALSE;

		}

		/* Check for restoring game */
		else if (restart_loop == RESTART_RESTORE)
		{
			/* Check for auto save enabled and auto save present */
			if (opt.auto_save && load_auto_save(&real_game))
			{
				/* Restart loaded game */
				restart_loop = RESTART_LOAD;
			}
			else
			{
				/* Just start a new game */
				restart_loop = RESTART_NEW;
			}

			/* Restart main loop */
			run_game();
			return;
		}


		/* Undo previous choice */
		else if (restart_loop == RESTART_UNDO)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Remove one state from undo list */
			if (num_undo > 0) num_undo--;
		}

		/* Undo current round */
		else if (restart_loop == RESTART_UNDO_ROUND)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Reset counts */
			pos = choice = saved_choice = 0;

			/* Count to num_undo choices */
			while (choice < num_undo && pos < real_game.p[0].choice_size)
			{
				/* Check if the current position is a round boundary */
				if (is_round_boundary(real_game.advanced,
				                      real_game.p[0].choice_log + pos))
				{
					/* Save the current choice */
					saved_choice = choice;
				}

				/* Update the position */
				pos = next_choice(real_game.p[0].choice_log, pos);

				/* Add one to choice count */
				++choice;
			}

			/* Set the undo position at the previous round boundary */
			num_undo = saved_choice;
		}

		/* Undo game */
		else if (restart_loop == RESTART_UNDO_GAME)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Start from the beginning */
			num_undo = 0;
		}

		/* Redo current choice */
		else if (restart_loop == RESTART_REDO)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Add one to undo position */
			++num_undo;
		}

		/* Redo current round */
		else if (restart_loop == RESTART_REDO_ROUND)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Reset counts */
			pos = choice = 0;
			saved_choice = -1;

			/* Count to num_undo choices */
			while (choice <= num_undo && pos < real_game.p[0].choice_size)
			{
				/* Update position */
				pos = next_choice(real_game.p[0].choice_log, pos);

				/* Add one to choice count */
				++choice;
			}

			/* Loop until end of log */
			while (pos < real_game.p[0].choice_size)
			{
				/* Check for round boundary */
				if (is_round_boundary(real_game.advanced,
				                      real_game.p[0].choice_log + pos))
				{
					/* Save the current choice */
					saved_choice = choice;
					break;
				}

				/* Update position */
				pos = next_choice(real_game.p[0].choice_log, pos);

				/* Add one to choice count */
				++choice;
			}

			/* Check if choice was found */
			if (saved_choice >= 0)
			{
				/* Set the undo position at the next round boundary */
				num_undo = saved_choice;
			}
			else
			{
				/* Set the undo position at the end of the log */
				num_undo = choice;
			}
		}

		/* Redo to end of current game */
		else if (restart_loop == RESTART_REDO_GAME)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Set undo point (will be reduced later) */
			num_undo = 9999;
		}

		/* Load a new game */
		else if (restart_loop == RESTART_LOAD)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Set undo point (will be reduced later) */
			num_undo = 9999;

		}

		/* Replay a loaded game */
		else if (restart_loop == RESTART_REPLAY)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);

			/* Begin at start */
			num_undo = 0;
		}

		/* Replay to current position (to regenerate log) */
		else if (restart_loop == RESTART_CURRENT)
		{
			/* Reset our position and GUI elements */
			reset_gui();

			/* Start with start of game random seed */
			real_game.random_seed = real_game.start_seed;

			/* Initialize game */
			init_game(&real_game);
		}

		/* Reset counts */
		pos = choice = 0;

		/* Count to num_undo choices */
		while (choice < num_undo && pos < real_game.p[0].choice_size)
		{
			/* Update log position */
			pos = next_choice(real_game.p[0].choice_log, pos);

			/* Add one to choice count */
			++choice;
		}

		/* Set the current undo point (in case the log was too small) */
		num_undo = choice;

		/* Reset the size choice of the human player */
		real_game.p[0].choice_size = pos;

		/* Find total number of replay points */
		while (pos < orig_log_size[0])
		{
			/* Update log position */
			pos = next_choice(real_game.p[0].choice_log, pos);

			/* Add one to choice count */
			++choice;
		}

		/* Set the max number of undo positions in the log */
		max_undo = choice;

		/* Clear restart loop flag */
		restart_loop = 0;

		/* Begin game */
		begin_game(&real_game);

		/* Check for aborted game */
		if (real_game.game_over) return;

		/* Play game rounds until finished */
		while (game_round(&real_game));

		/* Check for restart request */
		if (restart_loop)
		{
			/* Restart loop */
			return;
		}

		/* Declare winner */
		declare_winner(&real_game);

		/* Format seed message */
		sprintf(buf, "(The seed for this game was %u.)\n", real_game.start_seed);

		/* Send message */
		message_add(&real_game, buf);

		/* Auto save */
		auto_save(&real_game, player_us);

		reset_status_data();
		add_data(-1);
		get_state(&real_game);

	}
}




/*
 * Setup windows, callbacks, etc, then let GTK take over.
 */
int main(int argc, char *argv[])
{

	char *fname = NULL;
	char msg[1024];
	int i, err;


	/* Load card designs */
	err = read_cards(NULL);

	/* Check for errors */
	if (err == -1)
	{
		/* Print error and exit */
		display_error("Error: Could not locate cards.txt!\n");
		exit(1);
	}
	else if (err == -2)
	{
		/* Print error and exit */
		display_error("Error: Could not parse cards.txt!\n");
		exit(1);
	}

	/* Load campaigns */
	read_campaign();

	/* By default restore single-player game */
	restart_loop = RESTART_RESTORE;

	/* Parse arguments */
	for (i = 1; i < argc; i++)
	{
		/* Check for number of players */
		if (!strcmp(argv[i], "-p"))
		{
			/* Set number of players */
			opt.num_players = atoi(argv[++i]);

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for expansion level */
		else if (!strcmp(argv[i], "-e"))
		{
			/* Set expansion level */
			opt.expanded = atoi(argv[++i]);

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for player name */
		else if (!strcmp(argv[i], "-n"))
		{
			/* Set player name */
			opt.player_name = argv[++i];

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for advanced game */
		else if (!strcmp(argv[i], "-a"))
		{
			/* Set advanced */
			opt.advanced = 1;

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for random seed */
		else if (!strcmp(argv[i], "-r"))
		{
			/* Set random seed */
			opt.customize_seed = TRUE;

			/* Set start seed */
			opt.seed = (unsigned int) atof(argv[++i]);

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for goals on */
		else if (!strcmp(argv[i], "-g"))
		{
			/* Set goals on */
			opt.disable_goal = FALSE;

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for goals off */
		else if (!strcmp(argv[i], "-nog"))
		{
			/* Set goals off */
			opt.disable_goal = TRUE;

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for takeovers on */
		else if (!strcmp(argv[i], "-t"))
		{
			/* Set takeovers on */
			opt.disable_takeover = FALSE;

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for takeovers off */
		else if (!strcmp(argv[i], "-not"))
		{
			/* Set takeovers off */
			opt.disable_takeover = TRUE;

			/* Start new game */
			restart_loop = RESTART_NEW;
		}

		/* Check for saved game */
		else if (!strcmp(argv[i], "-s"))
		{
			/* Set file name */
			fname = argv[++i];
		}
	}
	opt.auto_save = 1;

	/* Apply options */
	apply_options();

	/* Create choice logs for each player */
	for (i = 0; i < MAX_PLAYER; i++)
	{
		/* Create log */
		real_game.p[i].choice_log = (int *)malloc(sizeof(int) * 4096);

		/* Save original log */
		orig_log[i] = real_game.p[i].choice_log;
		orig_log_size[i] = 0;

		/* Clear choice log size and position */
		real_game.p[i].choice_size = 0;
		real_game.p[i].choice_pos = 0;
	}


	/* Check if loading from file */
	if (fname)
	{
		/* Try to load savefile into load state */
		if (load_game(&real_game, fname) < 0)
		{
			/* Format error */
			sprintf(msg, "Failed to load game from file %s\n", fname);

			/* Show error */
			display_error(msg);
		}
		else
		{
			/* Force current game over */
			real_game.game_over = 1;

			/* Loop over players */
			for (i = 0; i < real_game.num_players; ++i)
			{
				/* Remember log size */
				orig_log_size[i] = real_game.p[i].choice_size;
			}

			/* Switch to loaded state when able */
			restart_loop = RESTART_LOAD;
		}
	}

	/* Run games */
	run_game();

	/* Exit */
	return 0;
}

void continue_game(int loop) {
        if (loop < 0) {
                choice_done(&real_game);
        } else {
                restart_loop = loop;
        }
	run_game();
}

int get_cards_num() {
        return real_game.deck_size;
}

char *get_card_name(int i) {
        return real_game.deck[i].d_ptr->name;
}

int get_card_image(int i) {
	return real_game.deck[i].d_ptr->index;
}

int get_card_num_powers(int i) {
        return real_game.deck[i].d_ptr->num_power;
}

static char buf[1024];
static char *name_consume(power *o_ptr) {
                char *name, buf2[1024];
		/* Check for simple powers */
		if (o_ptr->code == P4_DRAW)
		{
			/* Make string */
			sprintf(buf, "Draw %d", o_ptr->value);
		}
		else if (o_ptr->code == P4_VP)
		{
			/* Make string */
			sprintf(buf, "Take VP");
		}
		else if (o_ptr->code == P4_DRAW_LUCKY)
		{
			/* Make string */
			sprintf(buf, "Draw if lucky");
		}
		else if (o_ptr->code == P4_ANTE_CARD)
		{
			/* Make string */
			sprintf(buf, "Ante card");
		}
		else if (o_ptr->code & P4_CONSUME_3_DIFF)
		{
			/* Make string */
			sprintf(buf, "Consume 3 kinds");
		}
		else if (o_ptr->code & P4_CONSUME_N_DIFF)
		{
			/* Make string */
			sprintf(buf, "Consume different kinds");
		}
		else if (o_ptr->code & P4_CONSUME_ALL)
		{
			/* Make string */
			sprintf(buf, "Consume all goods");
		}
		else if (o_ptr->code & P4_TRADE_ACTION)
		{
			/* Make string */
			sprintf(buf, "Trade good");

			/* Check for no bonuses */
			if (o_ptr->code & P4_TRADE_NO_BONUS)
			{
				/* Append qualifier */
				strcat(buf, " (no bonus)");
			}
		}
		else
		{
			/* Get type of good to consume */
			if (o_ptr->code & P4_CONSUME_NOVELTY)
			{
				/* Novelty good */
				name = "Novelty ";
			}
			else if (o_ptr->code & P4_CONSUME_RARE)
			{
				/* Rare good */
				name = "Rare ";
			}
			else if (o_ptr->code & P4_CONSUME_GENE)
			{
				/* Genes good */
				name = "Genes ";
			}
			else if (o_ptr->code & P4_CONSUME_ALIEN)
			{
				/* Alien good */
				name = "Alien ";
			}
			else
			{
				/* Any good */
				name = "";
			}

			/* Start consume string */
			if (o_ptr->code & P4_DISCARD_HAND)
			{
				/* Make string */
				sprintf(buf, "Consume from hand for ");
			}
			else if (o_ptr->code & P4_CONSUME_TWO)
			{
				/* Start string */
				sprintf(buf, "Consume two %sgoods for ", name);
			}
			else if (o_ptr->code & P4_CONSUME_PRESTIGE)
			{
				/* Make string */
				sprintf(buf, "Consume prestige for ");
			}
			else
			{
				/* Start string */
				sprintf(buf, "Consume %sgood for ", name);
			}

			/* Check for cards */
			if (o_ptr->code & P4_GET_CARD)
			{
				/* Create card reward string */
				sprintf(buf2, "%d card%s", o_ptr->value, PLURAL(o_ptr->value));

				/* Add to string */
				strcat(buf, buf2);

				/* Check for other reward as well */
				if (o_ptr->code & (P4_GET_VP | P4_GET_PRESTIGE))
				{
					/* Add "and" */
					strcat(buf, " and ");
				}
			}

			/* Check for extra cards */
			if (o_ptr->code & P4_GET_2_CARD)
			{
				/* Create card reward string */
				strcat(buf, "2 cards");

				/* Check for other reward as well */
				if (o_ptr->code & (P4_GET_VP | P4_GET_PRESTIGE))
				{
					/* Add "and" */
					strcat(buf, " and ");
				}
			}

			/* Check for extra cards */
			if (o_ptr->code & P4_GET_3_CARD)
			{
				/* Create card reward string */
				strcat(buf, "3 cards");

				/* Check for other reward as well */
				if (o_ptr->code & (P4_GET_VP | P4_GET_PRESTIGE))
				{
					/* Add "and" */
					strcat(buf, " and ");
				}
			}

			/* Check for points */
			if (o_ptr->code & P4_GET_VP)
			{
				/* Create VP reward string */
				sprintf(buf2, "%d VP", o_ptr->value);

				/* Add to string */
				strcat(buf, buf2);

				/* Check for other reward as well */
				if (o_ptr->code & P4_GET_PRESTIGE)
				{
					/* Add "and" */
					strcat(buf, " and ");
				}
			}

			/* Check for prestige */
			if (o_ptr->code & P4_GET_PRESTIGE)
			{
				/* Create prestige reward string */
				sprintf(buf2, "%d prestige", o_ptr->value);

				/* Add to string */
				strcat(buf, buf2);
			}

			/* Check for multiple times */
			if (o_ptr->times > 1)
			{
				/* Create times string */
				sprintf(buf2, " (x%d)", o_ptr->times);

				/* Add to string */
				strcat(buf, buf2);
			}
		}
        return buf;
}

static char *name_produce(design *d_ptr, power *o_ptr) {
		/* Clear string describing power */
		strcpy(buf, "");

		/* Check for simple powers */
		if (o_ptr->code & P5_DRAW_EACH_NOVELTY)
		{
			/* Make string */
			sprintf(buf, "Draw per Novelty produced");
		}
		else if (o_ptr->code & P5_DRAW_EACH_RARE)
		{
			/* Make string */
			sprintf(buf, "Draw per Rare produced");
		}
		else if (o_ptr->code & P5_DRAW_EACH_GENE)
		{
			/* Make string */
			sprintf(buf, "Draw per Genes produced");
		}
		else if (o_ptr->code & P5_DRAW_EACH_ALIEN)
		{
			/* Make string */
			sprintf(buf, "Draw per Alien produced");
		}
		else if (o_ptr->code & P5_DRAW_DIFFERENT)
		{
			/* Make string */
			sprintf(buf, "Draw per kind produced");
		}

		/* Check for discard required */
		if (o_ptr->code & P5_DISCARD)
		{
			/* Start string */
			sprintf(buf, "Discard to ");
		}

		/* Regular production powers */
		if (o_ptr->code & P5_PRODUCE)
		{
			/* Add to string */
			strcat(buf, "produce on ");
			strcat(buf, d_ptr->name);
		}
		else if (o_ptr->code & P5_WINDFALL_ANY)
		{
			/* Add to string */
			strcat(buf, "produce on any windfall");
		}
		else if (o_ptr->code & P5_WINDFALL_NOVELTY)
		{
			/* Add to string */
			strcat(buf, "produce on Novelty windfall");
		}
		else if (o_ptr->code & P5_WINDFALL_RARE)
		{
			/* Add to string */
			strcat(buf, "produce on Rare windfall");
		}
		else if ((o_ptr->code & P5_WINDFALL_GENE) &&
		         (o_ptr->code & P5_NOT_THIS))
		{
			/* Add to string */
			strcat(buf, "produce on other Genes windfall");
		}
		else if (o_ptr->code & P5_WINDFALL_GENE)
		{
			/* Add to string */
			strcat(buf, "produce on Genes windfall");
		}
		else if (o_ptr->code & P5_WINDFALL_ALIEN)
		{
			/* Add to string */
			strcat(buf, "produce on Alien windfall");
		}

		/* Capitalize string if needed */
		buf[0] = toupper(buf[0]);
                return buf;
}

char *name_settle(power *o_ptr) {
		/* Check for simple powers */
		if (o_ptr->code & P3_PLACE_TWO)
		{
			/* Make string */
			sprintf(buf, "Place second world");
		}
		else if (o_ptr->code & P3_PLACE_MILITARY)
		{
			/* Make string */
			sprintf(buf, "Place second military world");
		}
		else if (o_ptr->code & P3_PLACE_LEFTOVER)
		{
			/* Make string */
			sprintf(buf, "Place with leftover military");
		}
		else if (o_ptr->code & P3_UPGRADE_WORLD)
		{
			/* Make string */
			sprintf(buf, "Upgrade world");
		}
		else if (o_ptr->code & P3_PLACE_ZERO)
		{
			/* Make string */
			sprintf(buf, "Place non-military world at zero cost");
		}
		else if (o_ptr->code & P3_FLIP_ZERO)
		{
			/* Make string */
			sprintf(buf, "Flip to place non-military world");
		}
                return buf;
}

char *get_card_power_name(int i, int p) {
        design *d_ptr = real_game.deck[i].d_ptr;
        power *o_ptr = &real_game.deck[i].d_ptr->powers[p];
        if (o_ptr->phase == PHASE_CONSUME) return name_consume(o_ptr);
        if (o_ptr->phase == PHASE_PRODUCE) return name_produce(d_ptr, o_ptr);
	if (o_ptr->phase == PHASE_SETTLE) return name_settle(o_ptr);
        return "";
}

int get_card_power_score(int i, int p) {
        power *o_ptr = &real_game.deck[i].d_ptr->powers[p];
        if (o_ptr->phase == PHASE_CONSUME) return score_consume(o_ptr);
        if (o_ptr->phase == PHASE_PRODUCE) return score_produce(o_ptr);
        return 0;
}

char *choose_pay_prompt(int which, int mil_only, int mil_bonus)
{
	game *g = &real_game;
	int who = player_us;
	card *c_ptr;
	power *o_ptr;
	char *cost_card;
	char *p;
	int military, cost, ict_mil, iif_mil;
	discounts discount;
	compute_discounts(g, who, &discount);

	/* Get card we are paying for */
	c_ptr = &real_game.deck[which];

	/* Start at beginning of buffer */
	p = buf;

	/* Create prompt */
	p += sprintf(p, "Choose payment for %s ", c_ptr->d_ptr->name);

	/* Check for development */
	if (c_ptr->d_ptr->type == TYPE_DEVELOPMENT)
	{
		/* Compute cost */
		cost = devel_cost(g, who, which);

		/* Create prompt */
		p += sprintf(p, "(%d card%s)", cost, PLURAL(cost));
	}

	/* Check for world */
	else if (c_ptr->d_ptr->type == TYPE_WORLD)
	{
		/* Check for takeover */
		if (c_ptr->owner != who)
		{
			/* Compute strength difference */
			military =
				strength_against(g, who, which,
				                 g->takeover_power[g->num_takeover - 1], 0) -
				strength_against(g, c_ptr->owner, which, -1, 1);

			/* Check for ahead in strength */
			if (military > 0)
			{
				/* Format text */
				p += sprintf(p, "(currently %d military ahead)", military);
			}

			/* Check for equal strength */
			else if (military == 0)
			{
				/* Format text */
				p += sprintf(p, "(currently equal strength)");
			}

			/* Behind in strength */
			else
			{
				/* Format text */
				p += sprintf(p, "(currently %d military behind)", -military);
			}
		}

		/* Check for military world */
		else if (c_ptr->d_ptr->flags & FLAG_MILITARY)
		{
			/* Compute payment */
			military_world_payment(g, who, which, mil_only, mil_bonus,
			                       &discount,
			                       &military, &cost, &cost_card);

			/* Check for no pay-for-military power */
			if (cost == -1)
			{
				/* Format text */
				p += sprintf(p, "(%d military)", military);
			}
			else
			{
				/* Format text */
				p += sprintf(p, "(%d military or %d card%s)",
				             military, cost, PLURAL(cost));
			}
		}
		else
		{
			/* Compute payment */
			peaceful_world_payment(g, who, which, mil_only,
			                       &discount,
			                       &cost, &ict_mil, &iif_mil);

			/* Format text */
			p += sprintf(p, "(");

			/* Check for cost available */
			if (cost >= 0)
			{
				/* Format text */
				p += sprintf(p, "%d card%s", cost, PLURAL(cost));
			}

			/* Check for ICT or IIF */
			if (ict_mil >= 0 || iif_mil >= 0)
			{
				/* Check for cost */
				if (cost >= 0) p += sprintf(p, " or ");

				/* Check for both ICT and IIF and different military needed */
				if (ict_mil >= 0 && iif_mil >= 0 && ict_mil != iif_mil)
				{
					/* Format text */
					p += sprintf(p, "%d/%d military", ict_mil, iif_mil);
				}

				/* Check for only ICT, or equal military needed */
				else if (ict_mil >= 0)
				{
					/* Format text */
					p += sprintf(p, "%d military", ict_mil);
				}

				/* Check for only IIF */
				else if (iif_mil >= 0)
				{
					/* Format text */
					p += sprintf(p, "%d military", iif_mil);
				}
			}

			/* Format text */
			p += sprintf(p, ")");
		}
	}
	return buf;
}

int can_prestige(void) {
    if (real_game.expanded != 3 || real_game.p[player_us].prestige_action_used) return 0;
    return real_game.p[player_us].prestige > 0 ? 3 : 1;
}
