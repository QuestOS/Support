/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2004,2005  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <../efi/clanton/clanton.h>
#include <../netboot/timer.h>
#include <shared.h>
#include <term.h>

grub_jmp_buf restart_env;

int silent_grub = 0;

/* Clanton secure/non-secure global variable.
   Secure Boot is enabled by default.  The value is then refreshed at run-time
   by reading a fuse.  */
unsigned short int grub_cln_secure = 1;

/* Clanton build-time debug/release switch.  */
#ifndef GRUB_CLN_DEBUG
#define GRUB_CLN_DEBUG                          0
#endif
unsigned short int grub_cln_debug = GRUB_CLN_DEBUG;

/* Clanton-Secure config file buffer.  */
static char *cln_cfg_file_buffer = NULL;
static int cln_cfg_file_size = 0;

#if defined(PRESET_MENU_STRING) || defined(SUPPORT_DISKLESS)

# if defined(PRESET_MENU_STRING)
static const char *preset_menu = PRESET_MENU_STRING;
# elif defined(SUPPORT_DISKLESS)
/* Execute the command "bootp" automatically.  */
static const char *preset_menu = "bootp\n";
# endif /* SUPPORT_DISKLESS */

static int preset_menu_offset;

static int
open_preset_menu (void)
{
#ifdef GRUB_UTIL
  /* Unless the user explicitly requests to use the preset menu,
     always opening the preset menu fails in the grub shell.  */
  if (! use_preset_menu)
    return 0;
#endif /* GRUB_UTIL */
  
  preset_menu_offset = 0;
  return preset_menu != 0;
}

static int
read_from_preset_menu (char *buf, int maxlen)
{
  int len = grub_strlen (preset_menu + preset_menu_offset);

  if (len > maxlen)
    len = maxlen;

  grub_memmove (buf, preset_menu + preset_menu_offset, len);
  preset_menu_offset += len;

  return len;
}

static void
close_preset_menu (void)
{
  /* Disable the preset menu.  */
  preset_menu = 0;
}

#else /* ! PRESET_MENU_STRING && ! SUPPORT_DISKLESS */

#define open_preset_menu()	0
#define read_from_preset_menu(buf, maxlen)	0
#define close_preset_menu()

#endif /* ! PRESET_MENU_STRING && ! SUPPORT_DISKLESS */

#define PIT_CLOCK_TICK_RATE  1193180U
#define PIT_MAXTICKS 0xffff

/* Copied from netboot/timer.c  */
void
load_timer2 (unsigned int ticks)
{
  /* Set up the timer gate, turn off the speaker */
  outb((inb(PPC_PORTB) & ~PPCB_SPKR) | PPCB_T2GATE, PPC_PORTB);
  outb(TIMER2_SEL|WORD_ACCESS|MODE0|BINARY_COUNT, TIMER_MODE_PORT);
  outb(ticks & 0xFF, TIMER2_PORT);
  outb(ticks >> 8, TIMER2_PORT);
}

/* Rely on i8254 timer2 instead of RTC. */
static void
wait_one_second (void)
{
  int i = 0, count = PIT_CLOCK_TICK_RATE / PIT_MAXTICKS;
  
  for (i = 0; i < count; i ++)
    waiton_timer2 (PIT_MAXTICKS);
  waiton_timer2 (PIT_CLOCK_TICK_RATE % PIT_MAXTICKS);
}

static char *
get_entry (char *list, int num, int nested)
{
  int i;

  for (i = 0; i < num; i++)
    {
      do
	{
	  while (*(list++));
	}
      while (nested && *(list++));
    }

  return list;
}

/* Print an entry in a line of the menu box.  */
static void
print_entry (int y, int highlight, char *entry)
{
  int x;

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_NORMAL);
  
  if (highlight && current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_HIGHLIGHT);

  gotoxy (2, y);
  grub_putchar (' ');
  for (x = 3; x < 75; x++)
    {
      if (*entry && x <= 72)
	{
	  if (x == 72)
	    grub_putchar (DISP_RIGHT);
	  else
	    grub_putchar (*entry++);
	}
      else
	grub_putchar (' ');
    }
  gotoxy (74, y);

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_STANDARD);
}

/* Print entries in the menu box.  */
static void
print_entries (int y, int size, int first, int entryno, char *menu_entries)
{
  int i;
  
  gotoxy (77, y + 1);

  if (first)
    grub_putchar (DISP_UP);
  else
    grub_putchar (' ');

  menu_entries = get_entry (menu_entries, first, 0);

  for (i = 0; i < size; i++)
    {
      print_entry (y + i + 1, entryno == i, menu_entries);

      while (*menu_entries)
	menu_entries++;

      if (*(menu_entries - 1))
	menu_entries++;
    }

  gotoxy (77, y + size);

  if (*menu_entries)
    grub_putchar (DISP_DOWN);
  else
    grub_putchar (' ');

  gotoxy (74, y + entryno + 1);
}

static void
print_entries_raw (int size, int first, char *menu_entries)
{
  int i;

#define LINE_LENGTH 67

  for (i = 0; i < LINE_LENGTH; i++)
    grub_putchar ('-');
  grub_putchar ('\n');

  for (i = first; i < size; i++)
    {
      /* grub's printf can't %02d so ... */
      if (i < 10)
	grub_putchar (' ');
      grub_printf ("%d: %s\n", i, get_entry (menu_entries, i, 0));
    }

  for (i = 0; i < LINE_LENGTH; i++)
    grub_putchar ('-');
  grub_putchar ('\n');

#undef LINE_LENGTH
}


static void
print_border (int y, int size)
{
  int i;

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_NORMAL);
  
  gotoxy (1, y);

  grub_putchar (DISP_UL);
  for (i = 0; i < 73; i++)
    grub_putchar (DISP_HORIZ);
  grub_putchar (DISP_UR);

  i = 1;
  while (1)
    {
      gotoxy (1, y + i);

      if (i > size)
	break;
      
      grub_putchar (DISP_VERT);
      gotoxy (75, y + i);
      grub_putchar (DISP_VERT);

      i++;
    }

  grub_putchar (DISP_LL);
  for (i = 0; i < 73; i++)
    grub_putchar (DISP_HORIZ);
  grub_putchar (DISP_LR);

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_STANDARD);
}

static void
run_menu (char *menu_entries, char *config_entries, int num_entries,
	  char *heap, int entryno)
{
  int c, first_entry = 0;
  char *cur_entry = 0;
  struct term_entry *prev_term = NULL;

  if (grub_verbose)
    cls();

  /*
   *  Main loop for menu UI.
   */

restart:
  /* Dumb terminal always use all entries for display 
     invariant for TERM_DUMB: first_entry == 0  */
  if (! (current_term->flags & TERM_DUMB))
    {
      while (entryno > 11)
	{
	  first_entry++;
	  entryno--;
	}
    }

  /* If the timeout was expired or wasn't set, force to show the menu
     interface. */
  if (grub_timeout < 0)
    show_menu = 1;

  /* If SHOW_MENU is false, don't display the menu until ESC is pressed.  */
  if (! show_menu)
    {
      /* Don't show the "Booting in blah seconds message" if the timeout is 0 */
      int print_message = grub_timeout != 0;

      if (print_message)
	grub_printf("\rPress any key to enter the menu\n\n\n");

      while (1)
	{
	  /* Check if any key is pressed */
	  if (checkkey () != -1)
	    {
	      grub_timeout = -1;
	      show_menu = 1;
	      getkey ();
	      break;
	    }

	  /* See if a modifier key is held down.  */
	  if (keystatus () != 0)
	    {
	      grub_timeout = -1;
	      show_menu = 1;
	      break;
	    }

	  /* If GRUB_TIMEOUT is expired, boot the default entry.  */
	  if (grub_timeout >=0)
	    {
	      if (grub_timeout <= 0)
		{
		  grub_timeout = -1;
		  goto boot_entry;
		}

	      grub_timeout--;
	      
	      /* Print a message.  */
	      if (print_message)
		grub_printf ("\rBooting %s in %d seconds...",
		             get_entry(menu_entries, first_entry + entryno, 0),
		             grub_timeout);

	      wait_one_second ();
	    }
	}
    }

  /* Only display the menu if the user wants to see it. */
  if (show_menu)
    {
      init_page (0);
      setcursor (0);

      if (current_term->flags & TERM_DUMB)
	print_entries_raw (num_entries, first_entry, menu_entries);
      else
	print_border (3, 12);

      grub_printf ("\n\
      Use the %c and %c keys to select which entry is highlighted.\n",
		   DISP_UP, DISP_DOWN);
      
      if (grub_cln_secure && ! grub_cln_debug)
        {
          /* Don't show menu choices that are unavailable in secure mode.  */
          grub_printf ("\
      Press enter to boot the selected OS.");
        }
      else if (! auth && password)
	{
	  printf ("\
      Press enter to boot the selected OS or \'p\' to enter a\n\
      password to unlock the next set of features.");
	}
      else
	{
	  if (config_entries)
	    printf ("\
      Press enter to boot the selected OS, \'e\' to edit the\n\
      commands before booting, \'a\' to modify the kernel arguments\n\
      before booting, or \'c\' for a command-line.");
	  else
	    printf ("\
      Press \'b\' to boot, \'e\' to edit the selected command in the\n\
      boot sequence, \'c\' for a command-line, \'o\' to open a new line\n\
      after (\'O\' for before) the selected line, \'d\' to remove the\n\
      selected line, or escape to go back to the main menu.");
	}

      if (current_term->flags & TERM_DUMB)
	grub_printf ("\n\nThe selected entry is %d ", entryno);
      else
	print_entries (3, 12, first_entry, entryno, menu_entries);
    }

  while (1)
    {
      /* Initialize to NULL just in case...  */
      cur_entry = NULL;

      if (grub_timeout >= 0)
	{
	  if (grub_timeout <= 0)
	    {
	      grub_timeout = -1;
	      break;
	    }

	  if (current_term->flags & TERM_DUMB)
	      grub_printf ("\r    Entry %d will be booted automatically in %d seconds.   ", 
			   entryno, grub_timeout);
	  else
	    {
	      gotoxy (3, 22);
	      grub_printf ("The highlighted entry will be booted automatically in %d seconds.    ",
			   grub_timeout);
	      gotoxy (74, 4 + entryno);
	  }
	  
	  grub_timeout--;

	  wait_one_second ();
	}

      /* Check for a keypress, however if TIMEOUT has been expired
	 (GRUB_TIMEOUT == -1) relax in GETKEY even if no key has been
	 pressed.  
	 This avoids polling (relevant in the grub-shell and later on
	 in grub if interrupt driven I/O is done).  */
      if (checkkey () >= 0 || grub_timeout < 0)
	{
	  /* Key was pressed, show which entry is selected before GETKEY,
	     since we're comming in here also on GRUB_TIMEOUT == -1 and
	     hang in GETKEY */
	  if (current_term->flags & TERM_DUMB)
	    grub_printf ("\r    Highlighted entry is %d: ", entryno);

	  c = ASCII_CHAR (getkey ());

	  if (grub_timeout >= 0)
	    {
	      if (current_term->flags & TERM_DUMB)
		grub_putchar ('\r');
	      else
		gotoxy (3, 22);
	      printf ("                                                                    ");
	      grub_timeout = -1;
	      fallback_entryno = -1;
	      if (! (current_term->flags & TERM_DUMB))
		gotoxy (74, 4 + entryno);
	    }

	  /* We told them above (at least in SUPPORT_SERIAL) to use
	     '^' or 'v' so accept these keys.  */
	  if (c == 16 || c == '^')
	    {
	      if (current_term->flags & TERM_DUMB)
		{
		  if (entryno > 0)
		    entryno--;
		}
	      else
		{
		  if (entryno > 0)
		    {
		      print_entry (4 + entryno, 0,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		      entryno--;
		      print_entry (4 + entryno, 1,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		    }
		  else if (first_entry > 0)
		    {
		      first_entry--;
		      print_entries (3, 12, first_entry, entryno,
				     menu_entries);
		    }
		}
	    }
	  else if ((c == 14 || c == 'v')
		   && first_entry + entryno + 1 < num_entries)
	    {
	      if (current_term->flags & TERM_DUMB)
		entryno++;
	      else
		{
		  if (entryno < 11)
		    {
		      print_entry (4 + entryno, 0,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		      entryno++;
		      print_entry (4 + entryno, 1,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		  }
		else if (num_entries > 12 + first_entry)
		  {
		    first_entry++;
		    print_entries (3, 12, first_entry, entryno, menu_entries);
		  }
		}
	    }
	  else if (c == 7)
	    {
	      /* Page Up */
	      first_entry -= 12;
	      if (first_entry < 0)
		{
		  entryno += first_entry;
		  first_entry = 0;
		  if (entryno < 0)
		    entryno = 0;
		}
	      print_entries (3, 12, first_entry, entryno, menu_entries);
	    }
	  else if (c == 3)
	    {
	      /* Page Down */
	      first_entry += 12;
	      if (first_entry + entryno + 1 >= num_entries)
		{
		  first_entry = num_entries - 12;
		  if (first_entry < 0)
		    first_entry = 0;
		  entryno = num_entries - first_entry - 1;
		}
	      print_entries (3, 12, first_entry, entryno, menu_entries);
	    }

	  if (config_entries)
	    {
	      if ((c == '\n') || (c == '\r') || (c == 6))
		break;
	    }
	  else
	    {
	      if ((c == 'd') || (c == 'o') || (c == 'O'))
		{
		  if (! (current_term->flags & TERM_DUMB))
		    print_entry (4 + entryno, 0,
				 get_entry (menu_entries,
					    first_entry + entryno,
					    0));

		  /* insert after is almost exactly like insert before */
		  if (c == 'o')
		    {
		      /* But `o' differs from `O', since it may causes
			 the menu screen to scroll up.  */
		      if (entryno < 11 || (current_term->flags & TERM_DUMB))
			entryno++;
		      else
			first_entry++;
		      
		      c = 'O';
		    }

		  cur_entry = get_entry (menu_entries,
					 first_entry + entryno,
					 0);

		  if (c == 'O')
		    {
		      grub_memmove (cur_entry + 2, cur_entry,
				    heap - cur_entry);

		      cur_entry[0] = ' ';
		      cur_entry[1] = 0;

		      heap += 2;

		      num_entries++;
		    }
		  else if (num_entries > 0)
		    {
		      char *ptr = get_entry(menu_entries,
					    first_entry + entryno + 1,
					    0);

		      grub_memmove (cur_entry, ptr,
				    heap - ptr);
		      heap -= ptr - cur_entry;

		      num_entries--;

		      if (entryno >= num_entries)
			entryno--;
		      if (first_entry && num_entries < 12 + first_entry)
			first_entry--;
		    }

		  if (current_term->flags & TERM_DUMB)
		    {
		      grub_printf ("\n\n");
		      print_entries_raw (num_entries, first_entry,
					 menu_entries);
		      grub_printf ("\n");
		    }
		  else
		    print_entries (3, 12, first_entry, entryno, menu_entries);
		}

	      cur_entry = menu_entries;
	      if (c == 27)
		return;
	      if (c == 'b')
		break;
	    }

	  if (grub_cln_secure && ! grub_cln_debug)
	    {
              /* Prevent the user from interacting with boot settings.  */
	    }
	  else if (! auth && password)
	    {
	      if (c == 'p')
		{
		  /* Do password check here! */
		  char entered[32];
		  char *pptr = password;

		  if (current_term->flags & TERM_DUMB)
		    grub_printf ("\r                                    ");
		  else
		    gotoxy (1, 21);

		  /* Wipe out the previously entered password */
		  grub_memset (entered, 0, sizeof (entered));
		  get_cmdline (" Password: ", entered, 31, '*', 0);

		  while (! isspace (*pptr) && *pptr)
		    pptr++;

		  /* Make sure that PASSWORD is NUL-terminated.  */
		  *pptr++ = 0;

		  if (! check_password (entered, password, password_type))
		    {
		      char *new_file = config_file;
		      while (isspace (*pptr))
			pptr++;

		      /* If *PPTR is NUL, then allow the user to use
			 privileged instructions, otherwise, load
			 another configuration file.  */
		      if (*pptr != 0)
			{
			  while ((*(new_file++) = *(pptr++)) != 0)
			    ;

			  /* Make sure that the user will not have
			     authority in the next configuration.  */
			  auth = 0;
			  return;
			}
		      else
			{
			  /* Now the user is superhuman.  */
			  auth = 1;
			  goto restart;
			}
		    }
		  else
		    {
		      grub_printf ("Failed!\n      Press any key to continue...");
		      getkey ();
		      goto restart;
		    }
		}
	    }
	  else
	    {
	      if (c == 'e')
		{
		  int new_num_entries = 0, i = 0;
		  char *new_heap;

		  if (config_entries)
		    {
		      new_heap = heap;
		      cur_entry = get_entry (config_entries,
					     first_entry + entryno,
					     1);
		    }
		  else
		    {
		      /* safe area! */
		      new_heap = heap + NEW_HEAPSIZE + 1;
		      cur_entry = get_entry (menu_entries,
					     first_entry + entryno,
					     0);
		    }

		  do
		    {
		      while ((*(new_heap++) = cur_entry[i++]) != 0);
		      new_num_entries++;
		    }
		  while (config_entries && cur_entry[i]);

		  /* this only needs to be done if config_entries is non-NULL,
		     but it doesn't hurt to do it always */
		  *(new_heap++) = 0;

		  if (config_entries)
		    run_menu (heap, NULL, new_num_entries, new_heap, 0);
		  else
		    {
		      cls ();
		      print_cmdline_message (CMDLINE_EDIT_MODE);

		      new_heap = heap + NEW_HEAPSIZE + 1;

		      saved_drive = boot_drive;
		      saved_partition = install_partition;
		      current_drive = GRUB_INVALID_DRIVE;

		      if (! get_cmdline (PACKAGE " edit> ", new_heap,
					 NEW_HEAPSIZE + 1, 0, 1))
			{
			  int j = 0;

			  /* get length of new command */
			  while (new_heap[j++])
			    ;

			  if (j < 2)
			    {
			      j = 2;
			      new_heap[0] = ' ';
			      new_heap[1] = 0;
			    }

			  /* align rest of commands properly */
			  grub_memmove (cur_entry + j, cur_entry + i,
					(unsigned long) heap - ((unsigned long) cur_entry + i));

			  /* copy command to correct area */
			  grub_memmove (cur_entry, new_heap, j);

			  heap += (j - i);
			}
		    }

		  goto restart;
		}
	      if (c == 'c')
		{
		  enter_cmdline (heap, 0, 0);
		  goto restart;
		}
	      if (config_entries && c == 'a')
		{
		  int new_num_entries = 0, i = 0, j;
		  int needs_padding, amount;
		  char *new_heap;
		  char * entries;
		  char * entry_copy;
		  char * append_line;
		  char * start;

		  entry_copy = new_heap = heap;
		  cur_entry = get_entry (config_entries, first_entry + entryno,
					 1);
		  
		  do
		    {
		      while ((*(new_heap++) = cur_entry[i++]) != 0);
		      new_num_entries++;
		    }
		  while (config_entries && cur_entry[i]);

		  /* this only needs to be done if config_entries is non-NULL,
		     but it doesn't hurt to do it always */
		  *(new_heap++) = 0;

		  new_heap = heap + NEW_HEAPSIZE + 1;

		  entries = entry_copy;
		  while (*entries) 
		    {
		      if ((strstr(entries, "kernel") == entries) &&
			  isspace(entries[6])) 
			  break;

		      while (*entries) entries++;
		      entries++;
		    }

		  if (!*entries)
		      goto restart;

		  start = entries + 6;

		  /* skip the white space */
		  while (*start && isspace(*start)) start++;
		  /* skip the kernel name */
		  while (*start && !isspace(*start)) start++;

		  /* skip the white space */
		  needs_padding = (!*start || !isspace(*start));
		  while (*start && isspace(*start)) start++;

		  append_line = new_heap;
		  grub_strcpy(append_line, start);

		  cls();
		  print_cmdline_message (CMDLINE_EDIT_MODE);

		  if (get_cmdline(PACKAGE " append> ", 
				    append_line, NEW_HEAPSIZE + 1, 
				    0, 1))
		      goto restart;

		  /* have new args; append_line points to the
		     new args and start points to the old
		     args */

		  i = grub_strlen(start);
		  j = grub_strlen(append_line);

		  if (i > (j + needs_padding))
		      amount = i;
		  else
		      amount = j + needs_padding;

		  /* align rest of commands properly */
		  memmove (start + j + needs_padding, start + i,
		       ((unsigned long) append_line) - ((unsigned long) start) - (amount));

		  if (needs_padding)
		      *start = ' ';

		  /* copy command to correct area */
		  memmove (start + needs_padding, append_line, j);

		  /* set up this entry to boot */
		  config_entries = NULL;
		  cur_entry = entry_copy;
		  heap = new_heap;

		  break;
		}
#ifdef GRUB_UTIL
	      if (c == 'q')
		{
		  /* The same as ``quit''.  */
		  stop ();
		}
#endif
	    }
	}
    }
  
  /* Attempt to boot an entry.  */
  
 boot_entry:
  
  if (grub_verbose || show_menu)
    {
      cls ();
      setcursor (1);
    }
  /* if our terminal needed initialization, we should shut it down
   * before booting the kernel, but we want to save what it was so
   * we can come back if needed */
  prev_term = current_term;
  if (current_term->shutdown) 
    {
      (*current_term->shutdown)();
      current_term = term_table; /* assumption: console is first */
    }

  if (silent_grub)
    setcursor(0);
  
  while (1)
    {
      if (config_entries)
	verbose_printf ("  Booting \'%s\'\n\n",
		get_entry (menu_entries, first_entry + entryno, 0));
      else
	verbose_printf ("  Booting command-list\n\n");

      if (! cur_entry)
	cur_entry = get_entry (config_entries, first_entry + entryno, 1);

      /* Set CURRENT_ENTRYNO for the command "savedefault".  */
      current_entryno = first_entry + entryno;
      
      if (run_script (cur_entry, heap))
	{
	  if (fallback_entryno >= 0)
	    {
	      cur_entry = NULL;
	      first_entry = 0;
	      entryno = fallback_entries[fallback_entryno];
	      fallback_entryno++;
	      if (fallback_entryno >= MAX_FALLBACK_ENTRIES
		  || fallback_entries[fallback_entryno] < 0)
		fallback_entryno = -1;
	    }
	  else
	    break;
	}
      else
	break;
    }

  /* if we get back here, we should go back to what our term was before */
  current_term = prev_term;
  if (current_term->startup)
      /* if our terminal fails to initialize, fall back to console since
       * it should always work */
      if ((*current_term->startup)() == 0)
          current_term = term_table; /* we know that console is first */
  show_menu = 1;
  goto restart;
}


static int
get_line_from_config (char *cmdline, int maxlen, int read_from_file)
{
  int pos = 0, literal = 0, comment = 0;
  static int cln_index;
  char c = 0;  /* since we're loading it a byte at a time! */
  
  while (1)
    {
      if (read_from_file)
	{
          /* Config file is already buffered.  */
          if (cln_index == cln_cfg_file_size)
            {
              cln_index = 0;
              break;
            }
          c = cln_cfg_file_buffer[cln_index ++];
	}
      else
	{
	  if (! read_from_preset_menu (&c, 1))
	    break;
	}

      /* Skip all carriage returns.  */
      if (c == '\r')
	continue;

      /* Replace tabs with spaces.  */
      if (c == '\t')
	c = ' ';

      /* The previous is a backslash, then...  */
      if (literal)
	{
	  /* If it is a newline, replace it with a space and continue.  */
	  if (c == '\n')
	    {
	      c = ' ';
	      
	      /* Go back to overwrite a backslash.  */
	      if (pos > 0)
		pos--;
	    }
	    
	  literal = 0;
	}
	  
      /* translate characters first! */
      if (c == '\\' && ! literal)
	literal = 1;

      if (comment)
	{
	  if (c == '\n')
	    comment = 0;
	}
      else if (! pos)
	{
	  if (c == '#')
	    comment = 1;
	  else if ((c != ' ') && (c != '\n'))
	    cmdline[pos++] = c;
	}
      else
	{
	  if (c == '\n')
	    break;

	  if (pos < maxlen)
	    cmdline[pos++] = c;
	}
    }

  cmdline[pos] = 0;

  return pos;
}

/* This is the starting function in C.  */
void
cmain (void)
{
  int config_len, menu_len, num_entries;
  char *config_entries, *menu_entries;
  char *kill_buf = (char *) KILL_BUF;

  auto void reset (void);
  void reset (void)
    {
      count_lines = -1;
      config_len = 0;
      menu_len = 0;
      num_entries = 0;
      config_entries = (char *) mbi.drives_addr + mbi.drives_length;
      menu_entries = (char *) MENU_BUF;
      init_config ();
    }
  
  /* Initialize the environment for restarting Stage 2.  */
  grub_setjmp (restart_env);
  
  /* Initialize the kill buffer.  */
  *kill_buf = 0;

  /* Initialise the configuration file buffer.  */
  cln_cfg_file_buffer = NULL;
  cln_cfg_file_size = 0;

  /* Check if Clanton Secure Boot is enabled on this SKU.  */
  grub_cln_detect_secure_sku ();  

  /* Never return.  */
  for (;;)
    {
      int is_opened, is_preset;

      reset ();
      
      /* Here load the configuration file.  */
      
#ifdef GRUB_UTIL
      if (use_config_file)
#endif /* GRUB_UTIL */
	{
	  int i;

	  do
	    {
	      /* STATE 0:  Before any title command.
		 STATE 1:  In a title command.
		 STATE >1: In a entry after a title command.  */
	      int state = 0, prev_config_len = 0, prev_menu_len = 0;
	      char *cmdline;

	      /* Try the preset menu first. This will succeed at most once,
		 because close_preset_menu disables the preset menu.  */
	      is_opened = is_preset = open_preset_menu ();
	      if (! is_opened)
		{
                  /* Buffer configuration file and validate it if in Secure
                     mode.  */
                  grub_cln_load_config_file (&cln_cfg_file_buffer,
                                             &cln_cfg_file_size);
                  if (ERR_NONE == errnum)
                    is_opened = 1;
                  else
                    grub_cln_recovery_shell (grub_cln_loaded_from_spi);
		}

	      if (! is_opened)
		break;

	      /* This is necessary, because the menu must be overrided.  */
	      reset ();
	      
	      cmdline = (char *) CMDLINE_BUF;
	      while (get_line_from_config (cmdline, NEW_HEAPSIZE,
					   ! is_preset))
		{
		  struct builtin *builtin;
		  
		  /* Get the pointer to the builtin structure.  */
		  builtin = find_command (cmdline);
		  errnum = 0;
		  if (! builtin)
		    /* Unknown command. Just skip now.  */
		    continue;
		  
		  if (builtin->flags & BUILTIN_TITLE)
		    {
		      char *ptr;
		      
		      /* the command "title" is specially treated.  */
		      if (state > 1)
			{
			  /* The next title is found.  */
			  num_entries++;
			  config_entries[config_len++] = 0;
			  prev_menu_len = menu_len;
			  prev_config_len = config_len;
			}
		      else
			{
			  /* The first title is found.  */
			  menu_len = prev_menu_len;
			  config_len = prev_config_len;
			}
		      
		      /* Reset the state.  */
		      state = 1;
		      
		      /* Copy title into menu area.  */
		      ptr = skip_to (1, cmdline);
		      while ((menu_entries[menu_len++] = *(ptr++)) != 0)
			;
		    }
		  else if (! state)
		    {
		      /* Run a command found is possible.  */
		      if (builtin->flags & BUILTIN_MENU)
			{
			  char *arg = skip_to (1, cmdline);
			  (builtin->func) (arg, BUILTIN_MENU);
			  errnum = 0;
			}
		      else
			/* Ignored.  */
			continue;
		    }
		  else
		    {
		      char *ptr = cmdline;
		      
		      state++;
		      /* Copy config file data to config area.  */
		      while ((config_entries[config_len++] = *ptr++) != 0)
			;
		    }
		}
	      
	      if (state > 1)
		{
		  /* Finish the last entry.  */
		  num_entries++;
		  config_entries[config_len++] = 0;
		}
	      else
		{
		  menu_len = prev_menu_len;
		  config_len = prev_config_len;
		}
	      
	      menu_entries[menu_len++] = 0;
	      config_entries[config_len++] = 0;
	      grub_memmove (config_entries + config_len, menu_entries,
			    menu_len);
	      menu_entries = config_entries + config_len;

	      /* Make sure that all fallback entries are valid.  */
	      if (fallback_entryno >= 0)
		{
		  for (i = 0; i < MAX_FALLBACK_ENTRIES; i++)
		    {
		      if (fallback_entries[i] < 0)
			break;
		      if (fallback_entries[i] >= num_entries)
			{
			  grub_memmove (fallback_entries + i,
					fallback_entries + i + 1,
					((MAX_FALLBACK_ENTRIES - i - 1)
					 * sizeof (int)));
			  i--;
			}
		    }

		  if (fallback_entries[0] < 0)
		    fallback_entryno = -1;
		}
	      /* Check if the default entry is present. Otherwise reset
		 it to fallback if fallback is valid, or to DEFAULT_ENTRY 
		 if not.  */
	      if (default_entry >= num_entries)
		{
		  if (fallback_entryno >= 0)
		    {
		      default_entry = fallback_entries[0];
		      fallback_entryno++;
		      if (fallback_entryno >= MAX_FALLBACK_ENTRIES
			  || fallback_entries[fallback_entryno] < 0)
			fallback_entryno = -1;
		    }
		  else
		    default_entry = 0;
		}
	      
	      if (is_preset)
		close_preset_menu ();
	      else
		grub_close ();
	    }
	  while (is_preset);
	}

      /* go ahead and make sure the terminal is setup */
      if (current_term->startup)
        (*current_term->startup)();

      if (! num_entries)
	{
	  /* If no acceptable config file, goto command-line, starting
	     heap from where the config entries would have been stored
	     if there were any.  */
	  grub_verbose = 1;
	  enter_cmdline (config_entries, 1, 0);
	}
      else
	{
	  /* Run menu interface.  */
	  run_menu (menu_entries, config_entries, num_entries,
		    menu_entries + menu_len, default_entry);
	}
    }
}
