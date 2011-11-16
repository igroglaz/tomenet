/* $Id$ */
/* Client initialization module */

/*
 * This file should contain non-system-specific code.  If a
 * specific system needs its own "main" function (such as
 * Windows), then it should be placed in the "main-???.c" file.
 */

#include "angband.h"

/* For opendir */
#include <sys/types.h>
#include <dirent.h>

/* For dirname */
#include <libgen.h>

static int Socket;

#ifdef USE_SOUND_2010
/*
 * List of sound modules in the order they should be tried.
 */
const struct module sound_modules[] = {
 #ifdef SOUND_SDL
	{ "sdl", "SDL_mixer sound module", init_sound_sdl },
 #endif /* SOUND_SDL */
	{ "dummy", "Dummy module", NULL },
};
#endif /* USE_SOUND_2010 */

static void init_arrays(void)
{
	/* Macro variables */
	C_MAKE(macro__pat, MACRO_MAX, cptr);
	C_MAKE(macro__act, MACRO_MAX, cptr);
	C_MAKE(macro__cmd, MACRO_MAX, bool);
	C_MAKE(macro__hyb, MACRO_MAX, bool);

	/* Macro action buffer */
	C_MAKE(macro__buf, 1024, char);

	/* Message variables */
	C_MAKE(message__ptr, MESSAGE_MAX, s32b);
	C_MAKE(message__buf, MESSAGE_BUF, char);
	C_MAKE(message__ptr_chat, MESSAGE_MAX, s32b);
	C_MAKE(message__buf_chat, MESSAGE_BUF, char);
	C_MAKE(message__ptr_msgnochat, MESSAGE_MAX, s32b);
	C_MAKE(message__buf_msgnochat, MESSAGE_BUF, char);

	/* Hack -- No messages yet */
	message__tail = MESSAGE_BUF;
	message__tail_chat = MESSAGE_BUF;
	message__tail_msgnochat = MESSAGE_BUF;

	/* Initialize room for the store's stock */
	C_MAKE(store.stock, STORE_INVEN_MAX, object_type);
}

void init_schools(s16b new_size)
{
	/* allocate the extra memory */
	C_MAKE(schools, new_size, school_type);
	max_schools = new_size;
}

void init_spells(s16b new_size)
{
	/* allocate the extra memory */
	C_MAKE(school_spells, new_size, spell_type);
	max_spells = new_size;
}

static bool check_dir(cptr s) {
	DIR *dp = opendir(s);

	if (dp) {
		closedir(dp);
		return TRUE;
	} else {
		return FALSE;
	}
}

static void validate_dir(cptr s) {
	/* Verify or fail */
	if (!check_dir(s))
	{
		quit_fmt("Cannot find required directory:\n%s", s);
	}
}

/*
 * Initialize and verify the file paths, and the score file.
 *
 * Use the ANGBAND_PATH environment var if possible, else use
 * DEFAULT_PATH, and in either case, branch off appropriately.
 *
 * First, we'll look for the ANGBAND_PATH environment variable,
 * and then look for the files in there.  If that doesn't work,
 * we'll try the DEFAULT_PATH constant.  So be sure that one of
 * these two things works...
 *
 * We must ensure that the path ends with "PATH_SEP" if needed,
 * since the "init_file_paths()" function will simply append the
 * relevant "sub-directory names" to the given path.
 *
 * Note that the "path" must be "Angband:" for the Amiga, and it
 * is ignored for "VM/ESA", so I just combined the two.
 */
static void init_stuff(void)
{
#if defined(AMIGA) || defined(VM)

	/* Hack -- prepare "path" */
	strcpy(path, "TomeNET:");

#else /* AMIGA / VM */

	if (argv0) {
		char *app_path = strdup(argv0);
		char *app_dir;

		app_dir = dirname(app_path);

		/* Change current directory to the location of the binary - mikaelh */
		if (chdir(app_dir) == -1) {
			plog_fmt("chdir(\"%s\") failed", app_dir);
		}

		free(app_path);
	}

	if (!strlen(path))
	{
		cptr tail;

		/* Get the environment variable */
		tail = getenv("TOMENET_PATH");

		/* Use the angband_path, or a default */
		strcpy(path, tail ? tail : DEFAULT_PATH);
	}

	/* Hack -- Add a path separator (only if needed) */
	if (!suffix(path, PATH_SEP)) strcat(path, PATH_SEP);

	/* Validate the path */
	validate_dir(path);

#endif /* AMIGA / VM */

	/* Initialize */
	init_file_paths(path);

	/* Hack -- Validate the paths */
	validate_dir(ANGBAND_DIR_SCPT);
	validate_dir(ANGBAND_DIR_TEXT);
	validate_dir(ANGBAND_DIR_USER);
}



/*
 * Open all relevant pref files.
 */
void initialize_main_pref_files(void)
{
	char buf[1024];

	int i;

	/* MEGAHACK -- clean up the arrays
	 * I should have made a mess of something somewhere.. */
#if 0
	for (i = 0; i < OPT_MAX; i++) Client_setup.options[i] = FALSE;
#else
	for (i = 0; i < OPT_MAX; i++) {
		if (option_info[i].o_var)
			Client_setup.options[i] = (*option_info[i].o_var) = option_info[i].o_norm;
	}
#endif
	for (i = 0; i < TV_MAX; i++) Client_setup.u_char[i] = Client_setup.u_attr[i] = 0;
	for (i = 0; i < MAX_F_IDX; i++) Client_setup.f_char[i] = Client_setup.f_attr[i] = 0;
	for (i = 0; i < MAX_K_IDX; i++) Client_setup.k_char[i] = Client_setup.k_attr[i] = 0;
	for (i = 0; i < MAX_R_IDX; i++) Client_setup.r_char[i] = Client_setup.r_attr[i] = 0;


	/* Access the "basic" pref file */
	strcpy(buf, "pref.prf");

	/* Process that file */
	process_pref_file(buf);

	/* Access the "user" pref file */
	sprintf(buf, "user.prf");

	/* Process that file */
	process_pref_file(buf);

	/* Access the "basic" system pref file */
	sprintf(buf, "pref-%s.prf", ANGBAND_SYS);

	/* Process that file */
	process_pref_file(buf);

	/* Access the "visual" system pref file (if any) */
	sprintf(buf, "%s-%s.prf", (use_graphics ? "graf" : "font"), ANGBAND_SYS);

	/* Process that file */
	process_pref_file(buf);

	/* Access the "user" system pref file */
	sprintf(buf, "user-%s.prf", ANGBAND_SYS);

	/* Process that file */
	process_pref_file(buf);


	/* Hack: Convert old window.prf or user.prf files that
	   were made < 4.4.7.1. - C. Blue */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		/* in 4.7.7.1.0.0 there are only 8 flags up to 0x80: */
		if (window_flag[i] >= 0x0100) {
			i = -1;
			break;
		}
	}
	/* Found outdated flags? */
	if (i == -1) {
		for (i = 0; i < ANGBAND_TERM_MAX; i++) {
			u32b new_flags = 0x0;
			/* translate old->new */
			if (window_flag[i] & 0x00001) new_flags |= PW_INVEN;
			if (window_flag[i] & 0x00002) new_flags |= PW_EQUIP;
			if (window_flag[i] & 0x00008) new_flags |= PW_PLAYER;
			if (window_flag[i] & 0x00010) new_flags |= PW_LAGOMETER;
			if (window_flag[i] & 0x00040) new_flags |= PW_MESSAGE;
			if (window_flag[i] & 0x40000) new_flags |= PW_CHAT;
			if (window_flag[i] & 0x80000) new_flags |= PW_MSGNOCHAT;
			window_flag[i] = new_flags;
		}
		/* and.. save them! */
		(void)options_dump("user.prf");
	}
}

void initialize_player_pref_files(void){
	char buf[1024];

	/* Access the "race" pref file */
	if (race < Setup.max_race)
	{
		sprintf(buf, "%s.prf", race_info[race].title);
//		buf[0] = tolower(buf[0]);

		/* Process that file */
		process_pref_file(buf);
	}

	/* Access the "class" pref file */
	if (class < Setup.max_class)
	{
		sprintf(buf, "%s.prf", class_info[class].title);
//		buf[0] = tolower(buf[0]);

		/* Process that file */
		process_pref_file(buf);
	}

	/* Access the "account" pref file */
	sprintf(buf, "%s.prf", nick);
//	buf[0] = tolower(buf[0]);

	/* Process that file */
	process_pref_file(buf);

	/* Access the "character" pref file */
	sprintf(buf, "%s.prf", cname);
//	buf[0] = tolower(buf[0]);

	/* Process that file */
	process_pref_file(buf);
}

/* handle auto-loading of auto-inscription files (*.ins) on logon */
void initialize_player_ins_files(void) {
	char buf[1024];
	int i;

	/* start with empty auto-inscriptions list */
	for (i = 0; i < MAX_AUTO_INSCRIPTIONS; i++) {
		strcpy(auto_inscription_match[i], "");
		strcpy(auto_inscription_tag[i], "");
	}

	/* Access the "race" ins file */
	if (race < Setup.max_race)
	{
		sprintf(buf, "%s.ins", race_info[race].title);
		load_auto_inscriptions(buf);
	}

	/* Access the "class" ins file */
	if (class < Setup.max_class)
	{
		sprintf(buf, "%s.ins", class_info[class].title);
		load_auto_inscriptions(buf);
	}

	/* Access the "account" ins file */
	sprintf(buf, "%s.ins", nick);
	load_auto_inscriptions(buf);

	/* Access the "character" ins file */
	/* hack: only if account and character name aren't the same */
	if (strcmp(nick, cname)) {
		sprintf(buf, "%s.ins", cname);
		load_auto_inscriptions(buf);
	}
}


/*
 * Loop, looking for net input and responding to keypresses.
 */
static void Input_loop(void)
{
	int	netfd, result;

	if (Net_flush() == -1)
		return;

	if ((netfd = Net_fd()) == -1)
	{
		plog("Bad socket filedescriptor");
		return;
	}

	for (;;)
	{
		/* Send out a keepalive packet if need be */
		do_keepalive();
		do_mail();
		do_flicker();
		do_xfers();
		do_ping();

		if (Net_flush() == -1)
		{
			plog("Bad net flush");
			return;
		}

		/* Set the timeout on the network socket */
		/* This polling should probably be replaced with a select
		 * call that combines the net and keyboard input.
		 * This REALLY needs to be replaced since under my Linux
		 * system calling usleep seems to have a 10 ms overhead
		 * attached to it.
		 */
//		SetTimeout(0, 1000);
		SetTimeout(0, next_frame());

		/* Update the screen */
		Term_fresh();

		/* Only take input if we got some */
		if (SocketReadable(netfd))
		{
			if ((result = Net_input()) == -1)
			{
				/*plog("Bad net input");*/
				return;
			}
		}

		flush_count = 0;

		/* See if we have a command waiting */
		request_command();

		/* Process any commands we got */
		while (command_cmd)
		{
			/* Process it */
			process_command();

			/* Clear previous command */
			command_cmd = 0;

			/* Ask for another command */
			request_command();
		}

		/*
		 * Update our internal timer, which is used to figure out when
		 * to send keepalive packets.
		 */
		update_ticks();

		/* Flush input (now!) */
		flush_now();

		/* Redraw windows if necessary */
		if (p_ptr->window)
		{
			window_stuff();
		}

	}
}


/*
 * Display a message on screen.
 */
static void display_message(cptr msg, cptr title)
{
	char buf[80];
	cptr tmp, newline;
	int i, len, prt_len, row = 0;

	/* Save the screen */
	Term_save();

	/* Clear the first line */
	Term_erase(0, row, 255);

	/* Print the "Warning" title */
	snprintf(buf, sizeof(buf), "======== %s ========", title);
	c_put_str(TERM_YELLOW, buf, row++, 0);

	/* Leave the second line empty */
	Term_erase(0, row++, 255);

	tmp = msg;

	/* Simple support for multiple lines separated by '\n' */
	while ((newline = strchr(tmp, '\n'))) {
		/* Length of the line in the input string */
		len = newline - tmp;

		/* Split lines that are over 79 characters long */
		for (i = 0; i < len; i += prt_len) {
			prt_len = MIN(len - i, sizeof(buf) - 1);
			strncpy(buf, tmp + i, prt_len);
			buf[prt_len] = '\0';

			/* Print the line */
			c_prt(TERM_WHITE, buf, row++, 0);
		}

		/* Next line in the input string */
		tmp = newline + 1;
	}

	len = strlen(tmp);

	/* Split long lines */
	for (i = 0; i < len; i += prt_len) {
		prt_len = MIN(len - i, sizeof(buf) - 1);
		strncpy(buf, tmp + i, prt_len);
		buf[prt_len] = '\0';

		/* Print the line */
		c_prt(TERM_WHITE, buf, row++, 0);
	}

	/* One empty line */
	Term_erase(0, row++, 255);

	/* Print the good old "Press any key to continue..." message */
	c_prt(TERM_L_BLUE, "Press any key to continue...", row++, 0);

	/* Wait for the key */
	(void) inkey();

	/* Reload the screen */
	Term_load();
}


/*
 * A hook for "quit()".
 *
 * Close down, then fall back into "quit()".
 */
static void quit_hook(cptr s)
{
	int j;

#ifdef USE_SOUND_2010
	/* let the sound fade out, also helps the user to realize
	   he's been disconnected or something - C. Blue */
#ifdef SOUND_SDL
	mixer_fadeall();
#endif
#endif

	Net_cleanup();

	c_quit = 1;

	/* Display the quit reason */
	if (s && *s) display_message(s, "Quitting");

	if(message_num() && get_check("Save chatlog?")){
		FILE *fp;
		char buf[80];
		int i;
		time_t ct = time(NULL);
		struct tm* ctl = localtime(&ct);

		strcpy(buf, "tomenet-chat_");
		strcat(buf, format("%04d-%02d-%02d_%02d.%02d.%02d",
		    1900 + ctl->tm_year, ctl->tm_mon + 1, ctl->tm_mday,
		    ctl->tm_hour, ctl->tm_min, ctl->tm_sec));
		strcat(buf, ".txt");

		i=message_num();
		get_string("Filename:", buf, 80);
		/* maybe one day we'll get a Mac client */
		FILE_TYPE(FILE_TYPE_TEXT);
		fp=my_fopen(buf, "w");
		if(fp!=(FILE*)NULL){
			dump_messages_aux(fp, i, 1, FALSE);
			fclose(fp);
		}
	}

#ifdef UNIX_SOCKETS
	SocketCloseAll();
#endif

#ifndef WINDOWS
	write_mangrc();
#endif

	/* Nuke each term */
	for (j = ANGBAND_TERM_MAX - 1; j >= 0; j--)
	{
		/* Unused */
		if (!ang_term[j]) continue;

		/* Nuke it */
		term_nuke(ang_term[j]);
	}

	/* plog_hook must not be called anymore because the terminal is gone */
	plog_aux = NULL;
}


static void init_sound() {
#ifdef USE_SOUND_2010
	int i;

	/* One-time popup dialogue, to inform and instruct user of audio capabilities */
	if (sound_hint) plog("*******************************************\nTomeNET supports music and sound effects!\nTo enable those, you need to install a sound pack,\nsee http://www.tomenet.net/ forum and downloads.\n*******************************************\n");

	if (!use_sound) {
		/* Don't initialize sound modules */
		return;
	}

	/* Try the modules in the order specified by sound_modules[] */
	for (i = 0; i < N_ELEMENTS(sound_modules); i++) {
		if (sound_modules[i].init && 0 == sound_modules[i].init(0, NULL)) {
 #ifdef DEBUG_SOUND
			puts(format("USE_SOUND_2010: successfully loaded module %d.", i));
 #endif
			break;
		}
	}
 #ifdef DEBUG_SOUND
	puts("USE_SOUND_2010: done loading modules");
 #endif

	/* initialize mixer, putting configuration read from rc file live */
	set_mixing();

	/* remember indices of sounds that are hardcoded on client-side anyway, for efficiency */
	page_sound_idx = exec_lua(0, "return get_sound_index(\"page\")");
	rain1_sound_idx = exec_lua(0, "return get_sound_index(\"rain_soft\")");
	rain2_sound_idx = exec_lua(0, "return get_sound_index(\"rain_storm\")");
	snow1_sound_idx = exec_lua(0, "return get_sound_index(\"snow_soft\")");
	snow2_sound_idx = exec_lua(0, "return get_sound_index(\"snow_storm\")");
	browse_sound_idx = exec_lua(0, "return get_sound_index(\"browse\")");
	browsebook_sound_idx = exec_lua(0, "return get_sound_index(\"browse_book\")");
#endif
}


/*
 * A default hook function for "plog()" that displays a warning on the screen.
 */
static void plog_hook(cptr s) {
	/* Display a warning */
	if (s) display_message(s, "Warning");
}


/*
 * Initialize everything, contact the server, and start the loop.
 */
void client_init(char *argv1, bool skip)
{
	sockbuf_t ibuf;
	unsigned magic = 12345;
	unsigned char reply_to, status;
	int login_port;
	int bytes, retries;
	char host_name[80];
	u16b version = MY_VERSION;
        s32b temp;

	/* Set the "plog hook" */
	if (!plog_aux) plog_aux = plog_hook;

	/* Setup the file paths */
	init_stuff();

	/* Initialize various arrays */
	init_arrays();

	/* Sound requires Lua */
	init_lua();

	/* Initialize sound */
	init_sound();

	GetLocalHostName(host_name, 80);

	/* Set the "quit hook" */
	if (!quit_aux) quit_aux = quit_hook;

#ifndef UNIX_SOCKETS
	/* Check whether we should query the metaserver */
	if (argv1 == NULL)
	{
		server_port=cfg_game_port;
		/* Query metaserver */
		if (!get_server_name())
			quit("No server specified.");
#ifdef EXPERIMENTAL_META
                cfg_game_port = server_port;
#endif
	}
	else
	{
		/* Set the server's name */
                strcpy(server_name, argv1);
                if (strchr(server_name, ':') &&
		    /* Make sure it's not an IPv6 address. */
		    !strchr(strchr(server_name, ':') + 1, ':'))
                {

                        char *port = strchr(server_name, ':');
                        cfg_game_port = atoi(port + 1);
                        *port = '\0';
                }
	}

	/* Fix "localhost" */
	if (!strcmp(server_name, "localhost"))
#endif
                strcpy(server_name, host_name);


	/* Get character name and pass */
	if (!skip) get_char_name();

	if (server_protocol >= 2)
	{
		/* Use memfrob on the password */
		my_memfrob(pass, strlen(pass));
	}

        /* Capitalize the name */
	nick[0] = toupper(nick[0]);

	/* Create the net socket and make the TCP connection */
	if ((Socket = CreateClientSocket(server_name, cfg_game_port)) == -1)
	{
		quit("That server either isn't up, or you mistyped the hostname.\n");
	}

	/* Create a socket buffer */
	if (Sockbuf_init(&ibuf, Socket, CLIENT_SEND_SIZE,
		SOCKBUF_READ | SOCKBUF_WRITE) == -1)
	{
		quit("No memory for socket buffer\n");
	}

	/* Clear it */
	Sockbuf_clear(&ibuf);

	/* Extended version */
	if (server_protocol >= 2)
	{
		version = 0xFFFFU;
	}

	/* Put the contact info in it */
	Packet_printf(&ibuf, "%u", magic);
	Packet_printf(&ibuf, "%s%hu%c", real_name, GetPortNum(ibuf.sock), 0xFF);
	Packet_printf(&ibuf, "%s%s%hu", nick, host_name, version);

	/* Extended version */
	if (server_protocol >= 2)
	{
		Packet_printf(&ibuf, "%d%d%d%d%d%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_EXTRA, VERSION_BRANCH, VERSION_BUILD + (VERSION_OS) * 1000);
	}

	/* Connect to server */
#ifdef UNIX_SOCKETS
	if ((DgramConnect(Socket, server_name, cfg_game_port)) == -1)
#endif

	/* Send the info */
	if ((bytes = DgramWrite(Socket, ibuf.buf, ibuf.len) == -1))
	{
		quit("Couldn't send contact information\n");
	}

	/* Listen for reply */
	for (retries = 0; retries < 10; retries++)
        {
		/* Set timeout */
		SetTimeout(1, 0);

		/* Wait for info */
		if (!SocketReadable(Socket)) continue;

		/* Read reply */
		if(DgramRead(Socket, ibuf.buf, ibuf.size) <= 0)
		{
			/*printf("DgramReceiveAny failed (errno = %d)\n", errno);*/
			continue;
		}

		/* Extra info from packet */
		Packet_scanf(&ibuf, "%c%c%d%d", &reply_to, &status, &temp, &char_creation_flags);

		/* Hack -- set the login port correctly */
		login_port = (int) temp;

		/* Hack - Receive server version - mikaelh */
		if (char_creation_flags & 0x02)
		{
			Packet_scanf(&ibuf, "%d%d%d%d%d%d", &server_version.major, &server_version.minor,
			    &server_version.patch, &server_version.extra, &server_version.branch, &server_version.build);

			/* Remove the flag */
			char_creation_flags ^= 0x02;
		}
		else
		{
			/* Assume that the server is old */
			server_version.major = VERSION_MAJOR;
			server_version.minor = VERSION_MINOR;
			server_version.patch = VERSION_PATCH;
			server_version.extra = 0;
			server_version.branch = 0;
			server_version.build = 0;
		}

		break;
	}

	/* Check for failure */
	if (retries >= 10)
	{
		Net_cleanup();
		quit("Server didn't respond!\n");
	}

	/* Server returned error code */
	if (status && status != E_NEED_INFO)
	{
		/* The server didn't like us.... */
		switch (status)
		{
			case E_VERSION_OLD:
				quit("Your client is outdated. Please get the latest one from http://www.tomenet.net/");
			case E_VERSION_UNKNOWN:
				quit("Server responds 'Unknown client version'. Server might be outdated or client is invalid. Latest client is at http://www.tomenet.net/");
			case E_GAME_FULL:
				quit("Sorry, the game is full.  Try again later.");
			case E_IN_USE:
				quit("That nickname is already in use.  If it is your nickname, wait 30 seconds and try again.");
			case E_INVAL:
				quit("The server didn't like your nickname, realname, or hostname.");
			case E_TWO_PLAYERS:
				quit("There is already another character from this user/machine on the server.");
			case E_INVITE:
				quit("Sorry, the server is for members only.  Please retry with name 'guest'.");
			case E_BANNED:
				quit("You are temporarily banned from connecting to this server!");
			default:
				quit(format("Connection failed with status %d.", status));
		}
	}

/*	printf("Server sent login port %d\n", login_port);
	printf("Server sent status %u\n", status);  */

	/* Close our current connection */
	/* Dont close the TCP connection DgramClose(Socket); */

	/* Connect to the server on the port it sent */
	if (Net_init(Socket) == -1)
	{
		quit("Network initialization failed!\n");
	}

	/* Verify that we are on the correct port */
	if (Net_verify(real_name, nick, pass) == -1)
	{
		Net_cleanup();
		quit("Network verify failed!\n");
	}

	/* Receive stuff like the MOTD */
	if (Net_setup() == -1)
	{
		Net_cleanup();
		quit("Network setup failed!\n");
	}

	status=Net_login();

	/* Hack -- display the nick */
	prt(format("Name        : %s", cname), 2, 1);

	/* Initialize the pref files */
	initialize_main_pref_files();

	if (status == E_NEED_INFO)
	{
		/* Get sex/race/class */
		/* XXX this function sends PKT_KEEPALIVE */
		get_char_info();
	}

	/* Setup the key mappings */
	keymap_init();

	/* Show the MOTD */
	show_motd(0); /* could be 2 for example to have ppl look at it.. */

	/* Clear the screen again */
	Term_clear();

	/* Start the game */
	if (Net_start(sex, race, class) == -1)
	{
		Net_cleanup();
		quit("Network start failed!\n");
	}

	/* Hack -- flush the key buffer */
	Term_flush();

	/* Turn the lag-o-meter on after we've logged in */
	lagometer_enabled = TRUE;

	/* Main loop */
	Input_loop();

	/* Cleanup network stuff */
	Net_cleanup();

	/* Quit, closing term windows */
	quit(NULL);
}
