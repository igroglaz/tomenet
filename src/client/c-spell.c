/* $Id$ */
/* Client-side spell stuff */

#include "angband.h"

static void print_spells(int book)
{
	int	i, col, realm, num;
	object_type *o_ptr = &inventory[book];

	/* Hack -- handle ghosts right */
	if (p_ptr->ghost)
	{
		realm = REALM_GHOST;
		num = 0;
	}
	else
	{
		realm = find_realm(o_ptr->tval);
		num = o_ptr->xtra1;
	}


	/* Print column */
	col = 20;

	/* Title the list */
	prt("", 1, col);
	put_str("Name", 1, col + 5);
	put_str("Lv Mana Fail", 1, col + 35);

	/* Dump the spells */
	for (i = 0; i < 9; i++)
	{
		/* Check for end of the book */
		if (spell_info[realm][num][i][0] == '\0')
			break;

		/* Dump the info */
		prt(spell_info[realm][num][i], 2 + i, col);
	}

	/* Clear the bottom line */
	prt("", 2 + i, col);
}

static void print_mimic_spells()
{
	int	i, col, j = 2;
	char buf[90];

	/* Print column */
	col = 20;

	/* Title the list */
	prt("", 1, col);
	put_str("Name", 1, col + 5);

	prt("", j, col);
	put_str("a) Polymorph Self", j++, col);

	/* Dump the spells */
	for (i = 0; i < 32; i++)
	{
		/* Check for end of the book */
	        if (!(p_ptr->innate_spells[0] & (1L << i)))
		  continue;

		/* Dump the info */
		sprintf(buf, "%c) %s", I2A(j - 2), monster_spells4[i]);
		prt(buf, j++, col);
	}
	for (i = 0; i < 32; i++)
	{
		/* Check for end of the book */
	        if (!(p_ptr->innate_spells[1] & (1L << i)))
			continue;

 		/* Dump the info */
		sprintf(buf, "%c) %s", I2A(j - 2), monster_spells5[i]);
		prt(buf, j++, col);
	}
	for (i = 0; i < 32; i++)
	{
		/* Check for end of the book */
	        if (!(p_ptr->innate_spells[2] & (1L << i)))
			continue;

		/* Dump the info */
		sprintf(buf, "%c) %s", I2A(j - 2), monster_spells6[i]);
		prt(buf, j++, col);
	}

	/* Clear the bottom line */
	prt("", j++, col);
}

/*
 * Allow user to choose a spell/prayer from the given book.
 */

/* modified to accept certain capital letters for priest spells. -AD- */ 
 
int get_spell(int *sn, cptr prompt, int book, bool known)
{
	int		i, num = 0;
	bool		flag, redraw, okay;
	char		choice;
	char		out_val[160];
	cptr p;
	
	object_type *o_ptr = &inventory[book];
	int realm = find_realm(o_ptr->tval);
	/* see Receive_inven .. one of the dirtiest hack ever :( */
	int sval = o_ptr->xtra1;

	p = "spell";

	if (p_ptr->ghost)
	{
		p = "power";
		realm = REALM_GHOST;
		book = 0;
		sval = 0;
	}

	/* Assume no usable spells */
	okay = FALSE;

	/* Assume no spells available */
	(*sn) = -2;

	/* Check for "okay" spells */
	for (i = 0; i < 9; i++)
	{
		/* Look for "okay" spells */
		if (spell_info[realm][sval][i][0] &&
				/* Hack -- This presumes the spells are sorted by level */
				!strstr(spell_info[realm][sval][i],"unknown"))
//		if (spell_info[realm][book][i])
		/* quite a hack.. FIXME */
//		if (!spell_info[realm][book][i][0])
//		if (!strstr(spell_info[realm][book][i],"unknown"))
		{
			okay = TRUE;
			num++;
		}
	}

	/* No "okay" spells */
	if (!okay) return (FALSE);


	/* Assume cancelled */
	(*sn) = -1;

	/* Nothing chosen yet */
	flag = FALSE;

	/* No redraw yet */
	redraw = FALSE;

	/* Build a prompt (accept all spells) */
	strnfmt(out_val, 78, "(%^ss %c-%c, *=List, ESC=exit) %^s which %s? ",
		p, I2A(0), I2A(num - 1), prompt, p);

	/* Get a spell from the user */
	while (!flag && get_com(out_val, &choice))
	{
		/* Request redraw */
		if ((choice == ' ') || (choice == '*') || (choice == '?'))
		{
			/* Show the list */
			if (!redraw)
			{
				/* Show list */
				redraw = TRUE;

				/* Save the screen */
				Term_save();

				/* Display a list of spells */
				print_spells(book);
			}

			/* Hide the list */
			else
			{
				/* Hide list */
				redraw = FALSE;

				/* Restore the screen */
				Term_load();

				/* Flush any events */
				Flush_queue();
			}

			/* Ask again */
			continue;
		}
		
		/* hack for CAPITAL prayers (heal other) */
		/* hack once more for possible expansion */
                /* lowercase */
                if (islower(choice))
                {
                        i = A2I(choice);
                        if (i >= num) i = -1;
                }

                /* uppercase... hope this is portable. */
                else if (isupper(choice))
                {
                        i = (choice - 'A') + 64;
                        if (i-64 >= num) i = -1;
                }
                else i = -1;

		/* Totally Illegal */
		if (i < 0)
		{
			bell();
			continue;
		}

		/* Stop the loop */
		flag = TRUE;
	}

	/* Restore the screen */
	if (redraw)
	{
		Term_load();

		/* Flush any events */
		Flush_queue();
	}


	/* Abort if needed */
	if (!flag) return (FALSE);

	/* Save the choice */
	(*sn) = i;

	/* Success */
	return (TRUE);
}


/*
 * Peruse the spells/prayers in a Book
 *
 * Note that *all* spells in the book are listed
 */
void show_browse(int book)
{
	/* Save the screen */
	Term_save();

	/* Display the spells */
	print_spells(book);
	
	/* Clear the top line */
	prt("", 0, 0);

	/* Prompt user */
	put_str("[Press any key to continue]", 0, 23);

	/* Wait for key */
	(void)inkey();

	/* Restore the screen */
	Term_load();

	/* Flush any events */
	Flush_queue();
}

#if 0
/*
 * Study a book to gain a new spell/prayer
 */
void do_study(int book)
{
	int j;
	cptr p = "spell";

	/* Priest -- Learn random spell */
	if (!strcmp(p, "prayer"))
	{
		j = -1;
	}
	/* Mage -- Learn a selected spell */
	else
	{
		/* Ask for a spell, allow cancel */
		if (!get_spell(&j, "study", book, FALSE)) return;
	}

	/* Tell the server */
	/* Note that if we are a priest, the server ignores the spell parameter */
	Send_gain(book, j);
}

/*
 * Cast a spell
 */
void do_cast(int book)
{
	int j;

	/* Ask for a spell, allow cancel */
	if (!get_spell(&j, "cast", book, FALSE)) return;

	/* Tell the server */
	Send_cast(book, j);
}

/*
 * Pray a spell
 */
void do_pray(int book)
{
	int j;

	/* Ask for a spell, allow cancel */
	if (!get_spell(&j, "pray", book, FALSE)) return;

	/* Tell the server */
	Send_pray(book, j);
}
#endif	// 0

static int get_mimic_spell(int *sn)
{
	int		i, num = 1;
	bool		flag, redraw;
	char		choice;
	char		out_val[160];
	int             corresp[200];

	/* Assume no spells available */
	(*sn) = -2;

	/* Init the Polymorph power */
	corresp[0] = 0;

	/* Check for "okay" spells */
	for (i = 0; i < 32; i++)
	{
		/* Look for "okay" spells */
		if (p_ptr->innate_spells[0] & (1L << i)) 
		{
		  corresp[num] = i + 1;
		  num++;
		}
	}
	for (i = 0; i < 32; i++)
	{
		/* Look for "okay" spells */
		if (p_ptr->innate_spells[1] & (1L << i)) 
		{
		  corresp[num] = i + 32 + 1;
		  num++;
		}
	}
	for (i = 0; i < 32; i++)
	{
		/* Look for "okay" spells */
		if (p_ptr->innate_spells[2] & (1L << i)) 
		{
		  corresp[num] = i + 64 + 1;
		  num++;
		}
	}


	/* Assume cancelled */
	(*sn) = -1;

	/* Nothing chosen yet */
	flag = FALSE;

	/* No redraw yet */
	redraw = FALSE;

	/* Build a prompt (accept all spells) */
	strnfmt(out_val, 78, "(Powers %c-%c, *=List, ESC=exit) use which power? ",
		I2A(0), I2A(num - 1));

	/* Get a spell from the user */
	while (!flag && get_com(out_val, &choice))
	{
		/* Request redraw */
		if ((choice == ' ') || (choice == '*') || (choice == '?'))
		{
			/* Show the list */
			if (!redraw)
			{
				/* Show list */
				redraw = TRUE;

				/* Save the screen */
				Term_save();

				/* Display a list of spells */
				print_mimic_spells();
			}

			/* Hide the list */
			else
			{
				/* Hide list */
				redraw = FALSE;

				/* Restore the screen */
				Term_load();

				/* Flush any events */
				Flush_queue();
			}

			/* Ask again */
			continue;
		}
		
	       	/* extract request */
		i = (islower(choice) ? A2I(choice) : -1);
	      	if (i >= num) i = -1;

		/* Totally Illegal */
		if (i < 0)
		{
			bell();
			continue;
		}

		/* Stop the loop */
		flag = TRUE;
	}

	/* Restore the screen */
	if (redraw)
	{
		Term_load();

		/* Flush any events */
		Flush_queue();
	}


	/* Abort if needed */
	if (!flag) return (FALSE);

	/* Save the choice */
	(*sn) = corresp[i];

	/* Success */
	return (TRUE);
}


/*
 * Mimic
 */
void do_mimic()
{
  int spell;

  /* Ask for the spell */
  if(!get_mimic_spell(&spell)) return;

  /* Tell the server */
  Send_mimic(spell);
}


#if 0
/*
 * Use a technic
 */
void do_fight(int book)
{
	int j;

	/* Ask for a spell, allow cancel */
	if (!get_spell(&j, "use", book, FALSE)) return;

	/* Tell the server */
	Send_fight(book, j);
}
#endif	// 0

/*
 * Use a ghost ability
 */
void do_ghost(void)
{
	int j;

	/* Ask for an ability, allow cancel */
	if (!get_spell(&j, "use", 0, FALSE)) return;

	/* Tell the server */
	Send_ghost(j);
}
