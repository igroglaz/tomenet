/* $Id$ */
/* File: birth.c */

/* Purpose: create a player character */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#define CLIENT

#include "angband.h"


/*
 * Choose the character's name
 */
void choose_name(void)
{
	char tmp[23];

	/* Prompt and ask */
	prt("Enter your player's name above (or hit ESCAPE).", 21, 2);

	/* Ask until happy */
	while (1)
	{
		/* Go to the "name" area */
		move_cursor(2, 15);

		/* Save the player name */
		strcpy(tmp, nick);

		/* Get an input, ignore "Escape" */
		if (askfor_aux(tmp, 15, 0)) strcpy(nick, tmp);

		/* All done */
		break;
	}

	/* Pad the name (to clear junk) */
	sprintf(tmp, "%-15.15s", nick);

	/* Re-Draw the name (in light blue) */
	c_put_str(TERM_L_BLUE, tmp, 2, 15);

	/* Erase the prompt, etc */
	clear_from(20);
}


/*
 * Choose the character's name
 */
void enter_password(void)
{
	int c;
	char tmp[23];

	/* Prompt and ask */
	prt("Enter your password above (or hit ESCAPE).", 21, 2);

	/* Default */
	strcpy(tmp, pass);

	/* Ask until happy */
	while (1)
	{
		/* Go to the "name" area */
		move_cursor(3, 15);

		/* Get an input, ignore "Escape" */
		if (askfor_aux(tmp, 15, 1)) strcpy(pass, tmp);

		/* All done */
		break;
	}

	/* Pad the name (to clear junk) 
	sprintf(tmp, "%-15.15s", pass); */

	 /* Re-Draw the name (in light blue) */
	for (c = 0; c < strlen(pass); c++)
		Term_putch(15+c, 3, TERM_L_BLUE, 'x');

	/* Erase the prompt, etc */
	clear_from(20);
}


/*
 * Choose the character's sex				-JWT-
 */
static void choose_sex(void)
{
	char        c='\0';		/* pfft redesign while(1) */
	bool hazard = FALSE;
	bool parity = magik(50);

	put_str("m) Male", 20, parity ? 2 : 17);
	put_str("f) Female", 20, parity ? 17 : 2);
#if 0
	put_str("M) Hell Male", 21, parity ? 2 : 17);
	put_str("F) Hell Female", 21, parity ? 17 : 2);
#endif	// 0

	while (1)
	{
		put_str("Choose a sex (* for random, Q to Quit): ", 19, 2);
		if (!hazard) c = inkey();
		if (c == 'Q') quit(NULL);
		if (c == 'm')
		{
			sex = 1;
			c_put_str(TERM_L_BLUE, "Male", 4, 15);
			break;
		}
		else if (c == 'f')
		{
			sex = 0;
			c_put_str(TERM_L_BLUE, "Female", 4, 15);
			break;
		}
#if 0
		else if (c == 'M')
		{
			sex = 3;
			c_put_str(TERM_L_BLUE, "Hell Male", 4, 15);
			break;
		}
		else if (c == 'F')
		{
			sex = 2;
			c_put_str(TERM_L_BLUE, "Hell Female", 4, 15);
			break;
		}
#endif	// 0
		else if (c == '?')
		{
			/*do_cmd_help("help.hlp");*/
		}
		else if (c == '*')
		{
			switch (rand_int(2))
			{
				case 0:
					c = 'f';
					break;
				case 1:
					c = 'm';
					break;
#if 0
				case 2:
					c = 'F';
					break;
				case 3:
					c = 'M';
					break;
#endif	// 0
			}
			hazard = TRUE;
		}
		else
		{
			bell();
		}
	}

	clear_from(19);
}


/*
 * Allows player to select a race			-JWT-
 */
static void choose_race(void)
{
	player_race *rp_ptr;
	int                 j, l, m, n;

	char                c;

	char		out_val[160];

	l = 2;
//	m = 21;
	m = 23 - (Setup.max_race - 1) / 5;
	n = m - 1;


	for (j = 0; j < Setup.max_race; j++)
	{
		rp_ptr = &race_info[j];
		(void)sprintf(out_val, "%c) %s", I2A(j), rp_ptr->title);
		put_str(out_val, m, l);
		l += 15;
		if (l > 70)
		{
			l = 2;
			m++;
		}
	}

	while (1)
	{
		put_str("Choose a race (* for random, Q to Quit): ", n, 2);
		c = inkey();
		if (c == 'Q') quit(NULL);

		if (c == '*') j = rand_int(Setup.max_race);
		else j = (islower(c) ? A2I(c) : -1);

		if ((j < Setup.max_race) && (j >= 0))
		{
			race = j;
			rp_ptr = &race_info[j];
			c_put_str(TERM_L_BLUE, (char*)rp_ptr->title, 5, 15);
			break;
		}
		else if (c == '?')
		{
			/*do_cmd_help("help.hlp");*/
		}
		else
		{
			bell();
		}
	}

	clear_from(n);
}


/*
 * Gets a character class				-JWT-
 */
static void choose_class(void)
{
	player_class *cp_ptr;
        player_race *rp_ptr = &race_info[race];
	int          j, l, m, n;

	char         c='\0';

	char	 out_val[160];
	bool hazard = FALSE;


	/* Prepare to list */
	l = 2;
//	m = 21;
	m = 23 - (Setup.max_class - 1) / 5;
	n = m - 1;

	/* Display the legal choices */
	for (j = 0; j < Setup.max_class; j++)
	{
		cp_ptr = &class_info[j];

                if (!(rp_ptr->choice & BITS(j)))
                {
                        l += 15;
                        continue;
                }

		sprintf(out_val, "%c) %s", I2A(j), cp_ptr->title);
		put_str(out_val, m, l);
		l += 15;
		if (l > 70)
		{
			l = 2;
			m++;
		}
	}

	/* Get a class */
	while (1)
	{
		put_str("Choose a class (? for Help, * for random, Q to Quit): ", n, 2);
		if (!hazard) c = inkey();
		if (c == 'Q') quit(NULL);

		if (c == '*') hazard = TRUE;
		if (hazard) j = rand_int(Setup.max_class);
		else j = (islower(c) ? A2I(c) : -1);

		if ((j < Setup.max_class) && (j >= 0))
		{
			if (!(rp_ptr->choice & BITS(j))) continue;

			class = j;
			cp_ptr = &class_info[j];
			c_put_str(TERM_L_BLUE, (char*)cp_ptr->title, 6, 15);
			break;
		}
		else if (c == '?')
		{
			/*do_cmd_help("help.hlp");*/
		}
		else
		{
			bell();
		}
	}

	clear_from(n);
}


/*
 * Get the desired stat order.
 */
void choose_stat_order(void)
{
	int i, j, k, avail[6];
	char c='\0';
	char out_val[160], stats[6][4];
	bool hazard = FALSE;

	/* All stats are initially available */
	for (i = 0; i < 6; i++)
	{
		strncpy(stats[i], stat_names[i], 3);
		stats[i][3] = '\0';
		avail[i] = 1;
	}

	/* Find the ordering of all 6 stats */
	for (i = 0; i < 6; i++)
	{
		/* Clear bottom of screen */
		clear_from(20);

		/* Print available stats at bottom */
		for (k = 0; k < 6; k++)
		{
			/* Check for availability */
			if (avail[k])
			{
				sprintf(out_val, "%c) %s", I2A(k), stats[k]);
				put_str(out_val, 21, k * 9);
			}
		}

		/* Hack -- it's obvious */
		if (i > 4) hazard = TRUE;

		/* Get a stat */
		while (1)
		{
//			put_str("Choose your stat order (? for Help, * for random, Q to Quit): ", 20, 2);
			put_str("Choose your stat order (* for random, Q to Quit): ", 20, 2);
			if (hazard)
			{
				j = rand_int(6);
			}
			else
			{
				c = inkey();
				if (c == 'Q') quit(NULL);
				if (c == '*') hazard = TRUE;

				j = (islower(c) ? A2I(c) : -1);
			}

			if ((j < 6) && (j >= 0) && (avail[j]))
			{
				stat_order[i] = j;
//				c_put_str(TERM_L_BLUE, stats[j], 8 + i, 15);
				c_put_str(TERM_L_BLUE, stats[j], 8, 15 + i * 5);
				avail[j] = 0;
				break;
			}
			else if (c == '?')
			{
				/*do_cmd_help("help.hlp");*/
			}
			else
			{
				bell();
			}
		}
	}

	clear_from(20);
}

/*
 * Choose to be a batman or not       -Jir-
 */
#if 0
static void choose_bat(void)
{
	char        c;

	put_str("y) Yes, bat me!", 20, 2);
	put_str("n) Who cares?", 21, 2);

	while (1)
	{
		put_str("You wanna be a fruit bat? (? for Help, Q to Quit): ", 19, 2);
		c = inkey();
		if (c == 'Q') quit(NULL);
		if (c == 'y')
		{
			sex += 512;
			c_put_str(TERM_L_BLUE, "Fruit Bat", 15, 15);
			break;
		}
		else if (c == 'n' || c == '*')
		{
			c_put_str(TERM_L_BLUE, "Normal Form", 15, 15);
			break;
		}
		else if (c == '?')
		{
			/*do_cmd_help("help.hlp");*/
		}
		else
		{
			bell();
		}
	}

	clear_from(20);
}
#else	// 0
/* Quick hack!		- Jir -
 * TODO: remove hard-coded things. */
static void choose_mode(void)
{
	char        c='\0';
	bool hazard = FALSE;

	put_str("n) Normal", 20, 2);
	put_str("f) Fruit bat", 20, 15 + 2);
	put_str("g) no Ghost", 20, 30 + 2);
	put_str("h) Hard", 20, 45 + 2);
	put_str("H) Hellish", 20, 60 + 2);

	while (1)
	{
		put_str("Choose a mode (* for random, Q to Quit): ", 19, 2);
		if (!hazard) c = inkey();
		if (c == 'Q') quit(NULL);
		if (c == 'f')
		{
			sex += 512;
			c_put_str(TERM_L_BLUE, "Fruit Bat", 10, 15);
			break;
		}
		else if (c == 'n')
		{
			c_put_str(TERM_L_BLUE, "Normal", 10, 15);
			break;
		}
		else if (c == 'g')
		{
			sex += MODE_NO_GHOST;
			c_put_str(TERM_L_BLUE, "No Ghost", 10, 15);
			break;
		}
		else if (c == 'h')
		{
			sex += (MODE_HELL);
			c_put_str(TERM_L_BLUE, "Hard", 10, 15);
			break;
		}
		else if (c == 'H')
		{
			sex += (MODE_NO_GHOST + MODE_HELL);
			c_put_str(TERM_L_BLUE, "Hellish", 10, 15);
			break;
		}
		else if (c == '?')
		{
			/*do_cmd_help("help.hlp");*/
		}
		else if (c == '*')
		{
			switch (rand_int(5))
			{
				case 0:
					c = 'n';
					break;
				case 1:
					c = 'f';
					break;
				case 2:
					c = 'g';
					break;
				case 3:
					c = 'h';
					break;
				case 4:
					c = 'H';
					break;
			}
			hazard = TRUE;
		}
		else
		{
			bell();
		}
	}

	clear_from(20);
}
#endif	// 0

#if 0
/*
 * Gets a character class				-JWT-
 */
static void choose_class_mage(void)
{
	int          j;

	char         c;

	/* Get a class */
	while (1)
	{
		put_str("Choose the max number of blows you want(1-4) (? for Help, Q to Quit)", 20, 2);
		put_str("The less blows you have the fastest your mana regenerate :", 21, 2);
		c = inkey();
		if (c == 'Q') quit(NULL);
		j = A2I(c);
		if ((j <= 4) && (j >= 1))
		{
			class_extra = j;
			break;
		}
		else if (c == '?')
		{
			/*do_cmd_help("help.hlp");*/
		}
		else
		{
			bell();
		}
	}

	clear_from(20);
}
#endif

/*
 * Get the name/pass for this character.
 */
void get_char_name(void)
{
	/* Clear screen */
	Term_clear();

	/* Title everything */
	put_str("Name        :", 2, 1);
	put_str("Password    :", 3, 1);

	/* Dump the default name */
	c_put_str(TERM_L_BLUE, nick, 2, 15);


	/* Display some helpful information XXX XXX XXX */

	/* Choose a name */
	choose_name();

	/* Enter password */
	enter_password();

	/* Message */
	put_str("Connecting to server....", 21, 1);

	/* Make sure the message is shown */
	Term_fresh();

	/* Note player birth in the message recall */
	c_message_add(" ");
	c_message_add("  ");
	c_message_add("====================");
	c_message_add("  ");
	c_message_add(" ");
}

/*
 * Get the other info for this character.
 */
void get_char_info(void)
{
	/* Title everything */
	put_str("Sex         :", 4, 1);
	put_str("Race        :", 5, 1);
	put_str("Class       :", 6, 1);
	put_str("Stat order  :", 8, 1);
	put_str("Mode        :",10, 1);

	/* Clear bottom of screen */
	clear_from(20);

	/* Display some helpful information XXX XXX XXX */

	/* Choose a sex */
	choose_sex();

	/* Choose a race */
	choose_race();

	/* Choose a class */
	choose_class();

	class_extra = 0;	
		
	/* Choose stat order */
	choose_stat_order();

	/* Choose form */
	choose_mode();

	/* Clear */
	clear_from(20);

	/* Message */
	put_str("Entering game...  [Hit any key]", 21, 1);

	/* Wait for key */
	inkey();

	/* Clear */
	clear_from(20);
}

static bool enter_server_name(void)
{
	/* Clear screen */
	Term_clear();

	/* Message */
	prt("Enter the server name you want to connect to (ESCAPE to quit): ", 3, 1);

	/* Move cursor */
	move_cursor(5, 1);

	/* Default */
        strcpy(server_name, "127.0.0.1");

	/* Ask for server name */
	return askfor_aux(server_name, 80, 0);
}

/*
 * Have the player choose a server from the list given by the
 * metaserver.
 */
/* TODO:
 * - wrap the words using strtok
 * - allow to scroll the screen in case
 * */

bool get_server_name(void)
{
	int i, j, k, l, bytes, socket, offsets[20], lines = 0;
	char buf[8192], *ptr, c, out_val[160];

	/* Message */
	prt("Connecting to metaserver for server list....", 1, 1);

	/* Make sure message is shown */
	Term_fresh();

	/* Connect to metaserver */
	socket = CreateClientSocket(META_ADDRESS, 8801);

	/* Check for failure */
	if (socket == -1)
	{
		/* Hack -- Connect to metaserver #2 */
		socket = CreateClientSocket(META_ADDRESS_2, 8801);

		/* Check for failure */
		if (socket == -1)
		{
			return enter_server_name();
		}
	}

	/* Read */
	bytes = SocketRead(socket, buf, 8192);

	/* Close the socket */
	SocketClose(socket);

	/* Check for error while reading */
	if (bytes <= 0)
	{
		return enter_server_name();
	}

	/* Start at the beginning */
	ptr = buf;
	i = 0;
	Term_clear();

	/* Print each server */
	while (ptr - buf < bytes)
	{
		/* Check for no entry */
		if (strlen(ptr) <= 1)
		{
			/* Increment */
			ptr++;

			/* Next */
			continue;
		}

		/* Save offset */
		offsets[i] = ptr - buf;

		/* Format entry */
		sprintf(out_val, "%c) %s", I2A(i), ptr);

		j = strlen(out_val);

		/* Strip off offending characters */
//		out_val[strlen(out_val) - 1] = '\0';
		out_val[j - 1] = '\0';
		out_val[j]='\0';

		k=0;
		while(j){
			l=strlen(&out_val[k]);
			if(j > 75){
				l=75;
				while(out_val[k+l]!=' ') l--;
				out_val[l]='\0';
			}
			prt(out_val+k, lines++, (k ? 4 : 1));
			k+=(l+1);
			j=strlen(&out_val[k]);
		}
#if 0

		prt(out_val, lines++, 1);

		k = 79;

		while (j > k)
		{
			/* Print this entry */
			prt(out_val + k, lines++, 3);

			k += 77;
		}
#endif

		/* Go to next metaserver entry */
		ptr += strlen(ptr) + 1;

		/* One more entry */
		i++;

		/* We can't handle more than 20 entries -- BAD */
//		if (i > 20) break;
		if (lines > 20) break;
	}

	/* Message */
	prt(longVersion , lines + 1, 1);

	/* Prompt */
	prt("-- Choose a server to connect to (Q for manual entry): ", lines + 2, 1);

	/* Ask until happy */
	while (1)
	{
		/* Get a key */
		c = inkey();

		/* Check for quit */
		if (c == 'Q')
		{
			return enter_server_name();
		}

		/* Index */
		j = (islower(c) ? A2I(c) : -1);

		/* Check for legality */
		if (j >= 0 && j < i)
			break;
	}

	/* Extract server name */
	sscanf(buf + offsets[j], "%s", server_name);

	/* Success */
	return TRUE;
}

/* Human */
static char *human_syllable1[] =
{
	"Ab", "Ac", "Ad", "Af", "Agr", "Ast", "As", "Al", "Adw", "Adr", "Ar",
	"B", "Br", "C", "Cr", "Ch", "Cad", "D", "Dr", "Dw", "Ed", "Eth", "Et",
	"Er", "El", "Eow", "F", "Fr", "G", "Gr", "Gw", "Gal", "Gl", "H", "Ha",
	"Ib", "Jer", "K", "Ka", "Ked", "L", "Loth", "Lar", "Leg", "M", "Mir",
	"N", "Nyd", "Ol", "Oc", "On", "P", "Pr", "R", "Rh", "S", "Sev", "T",
	"Tr", "Th", "V", "Y", "Z", "W", "Wic",
};

static char *human_syllable2[] =
{
	"a", "ae", "au", "ao", "are", "ale", "ali", "ay", "ardo", "e", "ei",
	"ea", "eri", "era", "ela", "eli", "enda", "erra", "i", "ia", "ie",
	"ire", "ira", "ila", "ili", "ira", "igo", "o", "oa", "oi", "oe",
	"ore", "u", "y",
};

static char *human_syllable3[] =
{
	"a", "and", "b", "bwyn", "baen", "bard", "c", "ctred", "cred", "ch",
	"can", "d", "dan", "don", "der", "dric", "dfrid", "dus", "f", "g",
	"gord", "gan", "l", "li", "lgrin", "lin", "lith", "lath", "loth",
	"ld", "ldric", "ldan", "m", "mas", "mos", "mar", "mond", "n",
	"nydd", "nidd", "nnon", "nwan", "nyth", "nad", "nn", "nnor", "nd",
	"p", "r", "ron", "rd", "s", "sh", "seth", "sean", "t", "th", "tha",
	"tlan", "trem", "tram", "v", "vudd", "w", "wan", "win", "wyn", "wyr",
	"wyr", "wyth",
};

/*
 * Random Name Generator
 * based on a Javascript by Michael Hensley
 * "http://geocities.com/timessquare/castle/6274/"
 */
//static void create_random_name(int race, char *name)
void create_random_name(int race, char *name)
{
	char *syl1, *syl2, *syl3;

	int idx;


	/* Paranoia */
	if (!name) return;

	idx = rand_int(sizeof(human_syllable1) / sizeof(char *));
	syl1 = human_syllable1[idx];
	idx = rand_int(sizeof(human_syllable2) / sizeof(char *));
	syl2 = human_syllable2[idx];
	idx = rand_int(sizeof(human_syllable3) / sizeof(char *));
	syl3 = human_syllable3[idx];

	/* Concatenate selected syllables */
	strnfmt(name, 32, "%s%s%s", syl1, syl2, syl3);
}

