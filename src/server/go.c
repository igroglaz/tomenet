/* File: go.c */
/* Purpose: Allow players to play a game of Go, by C. Blue */

/* Details:
 * Note that Go is called Igo in Japan, Weiqi in China and Baduk in Korea.
 * This code expects to find the Go engine executable in a subfolder 'go' and
 * also saves games into it. All engines that use the Go Text Protocol (GTP)
 * should be easily compatible.
 *
 * At the time of creation I decided on Fuego for the engine, which is free,
 * open source, and placed #2 in 9x9 tournament at Kanazawa 2010 Computer Go
 * Olympiad (the #1 program wasn't free). Might switch to GnuGo though.
 *
 * Other options are for example GnuGo or Aya. Although somewhat weaker (might
 * change, all under development), they allow scaling via a --level parameter.
 * GnuGo is open source, while Aya is free but not open source (runs on linux
 * via WINE).
 *
 * Other very strong bots that aren't open source are Zen, MoGo, Erica,
 * Valkyrie and CrazyStone. Unfortunately bot tournament results say pretty
 * little about a bot's strength, because the hardware that must be use is not
 * standardized! So one bot might win while running on a large cluster, while
 * another bot comes in 2nd on a simple Quadcore..
 *                                                     - C. Blue -
 */


/* ------------------------------------------------------------------------- */


#define SERVER
#include "angband.h"
#ifdef ENABLE_GO_GAME

/* # of buffer lines for GTP responses: 9 lines,2 coordinate lines, +1, +1 for msg hack.
   Could probably be reduced now that we have board_line..[] helper vars. */
#define MAX_GTP_LINES (9+2+1+1)

/* Screen absolute x,y coordinates of the Go board */
#define GO_BOARD_X	34
#define GO_BOARD_Y	8

/* Next actions to perform after async reading finishes */
#define NACT_NONE		0 /* nothing */
#define NACT_MOVE_PLAYER	1 /* request next move from player */
#define NACT_MOVE_CPU		2 /* request next move from AI */

/* Maximum rank: At this rank you'll have to play a game as white too! */
#define TOP_RANK	7

/* Time limits for the player (and CPU) in seconds */
#define GO_TIME_PY		25


/* Error handling constants */
static const int NONE = 0, DOWN = 1, UP = 2;

/* Discard all responses we could've read from the engine
   in case we're just in the process of terminating it anyway.
   This avoids slight slowdowns, useful if we're doing a warm
   restart of the Go engine while keeping the TomeNET server running. */
#define DISCARD_RESPONSES_WHEN_TERMINATING

/* Display text output for debugging? */
#define GO_DEBUGPRINT


/* Global control variables */
bool go_engine_up;		/* Go engine is online? */
int go_engine_processing = 0;	/* Go engine is expected to deliver replies to commands? */
bool go_game_up;		/* A game of Go is actually being played right now? */
u32b go_engine_player_id;	/* Player ID of the player who plays a game of Go right now. */

/* Private control variable to determine what to do next
   after go_engine_processing reaches 0 again: */
static int go_engine_next_action = NACT_NONE;

/* Private stuff */
static void writeToPipe(char *data);
static void readFromPipe(char *buf, int *cont);
static int test_for_response(void); /* non-blocking read */
static int wait_for_response(void); /* blocking read */
static int handle_loading(void);
static void go_engine_move_CPU(void);
static void go_engine_move_result(int move_result);
static void go_challenge_cleanup(bool server_shutdown);
static void go_engine_board(void);
static bool go_err(int engine_status, int game_status, char *name);

/* Handle game initialization and colour assignment */
static bool CPU_has_white, game_over = TRUE;
/* Manage player's time limit */
static int player_timelimit_sec, player_timeleft_sec;

/* Pipe-related vars */
static pid_t nPid;
static int pipeto[2];		/* pipe to feed the exec'ed program input */
static int pipefrom[2];		/* pipe to get the exec'ed program output */
static FILE *fw, *fr;
static char pipe_buf[MAX_GTP_LINES][240];
static int pipe_buf_current_line,  pipe_response_complete = 0;
#ifdef DISCARD_RESPONSES_WHEN_TERMINATING
static bool terminating = FALSE;
#endif
static bool received_board_visuals = FALSE;

/* Keep track of game starting time and duration */
static time_t tstart, tend;
static struct tm* tmstart;
static struct tm* tmend;
/* Game record: Keep track of moves and passes (for undo-handling too) */
static int pass_count;
static bool last_move_was_pass = FALSE, CPU_to_move, CPU_now_to_move;

/* Helper vars to update the board visuals between turns */
static char board_line_old[10][10], board_line[10][10];


/* ------------------------------------------------------------------------- */


/* Start engine, create & connect pipes, on server startup usually */
int go_engine_init(void) {
	s_printf("GO_INIT: ---INIT---\n");

	/* Try to create pipes for STDIN and STDOUT */
	if (pipe(pipeto) != 0) {
		s_printf("GO_ERROR: pipe() to.\n");
		return 1;
	}
	if (pipe(pipefrom) != 0) {
		s_printf("GO_ERROR: pipe() from.\n");
		return 2;
	}

	/* Try to create Go AI engine process */
	nPid = fork();
	if (nPid < 0) {
		s_printf("GO_ERROR: fork().\n");
		return 3;
	} else if (nPid == 0) {
//		close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
		/* dup pipe read/write to stdin/stdout */
		dup2(pipeto[0], STDIN_FILENO);
		dup2(pipefrom[1], STDOUT_FILENO);
		dup2(pipefrom[1], STDERR_FILENO);
		/* close unnecessary pipe descriptors for a clean environment */
		close(pipeto[1]);
		close(pipefrom[0]);

/*	"Options:\n"
	"  -config file execute GTP commands from file before\n"
	"               starting main command loop\n"
	"  -help        display this help and exit\n"
	"  -maxgames n  make clear_board fail after n invocations\n"
	"  -nobook      don't automatically load opening book\n"
	"  -nohandicap  don't support handicap commands\n"
	"  -quiet       don't print debug messages\n"
	"  -size        initial (and fixed) board size\n"
	"  -srand       set random seed (-1:none, 0:time(0))\n";*/

		execlp("./go/fuego", "fuego", NULL);
		s_printf("GO_ERROR: exec().\n");
		return 4;
	}

    { /* We're parent */
	int n;

	/* Clear the pipe_buf[] array */
	pipe_buf_current_line = 0;
	for (n = 0; n < MAX_GTP_LINES; n++) strcpy(pipe_buf[n], "");

	/* Prepare pipes (close unused ends) */
	close(pipeto[0]);
	close(pipefrom[1]);

	if ((fw = fdopen(pipeto[1], "w")) == NULL) {
		s_printf("GO_ERROR: fdopen() w.\n");
		return 5;
	}
	if ((fr = fdopen(pipefrom[0], "r")) == NULL) {
		fclose(fw);
		s_printf("GO_ERROR: fdopen() r.\n");
		return 6;
	}


	/* Power up engine */
	s_printf("GO_INIT: ---STARTING UP---\n");
	handle_loading();


	/* Prepare general game configuration */
	s_printf("GO_INIT: ---INIT AI---\n");

	writeToPipe("boardsize 9");
	wait_for_response();
	writeToPipe("clear_board");
	wait_for_response();
	writeToPipe("komi 0");
	wait_for_response();
	writeToPipe("time_settings 0 1 0");//infinite: b>0, s=0
	wait_for_response();

	writeToPipe("uct_param_player reuse_subtree 1");
	wait_for_response();

	/* multi-core: */
	/* only required for > 2 threads? doesn't matter: */
	writeToPipe("uct_param_search lock_free 1");
	wait_for_response();

	writeToPipe("uct_param_search number_threads 2");//we have a 2-core cpu
	wait_for_response();

	/* we have 512 MB.. */
	writeToPipe("uct_max_memory 300000000");
	wait_for_response();

	/* Hm, shouldn't this be default anyway? */
	writeToPipe("go_param_rules ko_rule pos_superko");
	wait_for_response();

#if 0 /* get more clue before using maybe^^ */
 #if 1
	/* more playouts = stronger? or obsolete?: */
	writeToPipe("uct_param_player max_games=32000");
	wait_for_response();
 #endif
 #if 0
	/* not already covered by max_games and max_memory? */
	writeToPipe("uct_param_search max_nodes=3750000");//seems max at 300MB RAM?
	wait_for_response();
 #endif
#endif

	/* slight improvements? (might be obsolete meanwhile): */
	writeToPipe("uct_param_search virtual_loss 1");
	wait_for_response();
	writeToPipe("uct_param_search number_playouts 2");
	wait_for_response();

	/* misc */
	//go_param debug_to_comment 1
	//go_param auto_save (filename)
	//go_sentinel_file (filename)  (for resuming games that were interrupted by disconnection)
	//final_status_list dead|seki|alive   returns list of stones; use to capture everything, to make a clear end?


	/* Init this 'flow control', to begin asynchronous pipe operation */
	go_engine_processing = 0;

	/* We're up and running, ready for games */
	go_engine_up = TRUE;
    } /* Parent thread */

	return 0;
}

/* Terminate engine & kill pipes, on server shutdown usually */
void go_engine_terminate(void) {
#ifdef DISCARD_RESPONSES_WHEN_TERMINATING
	terminating = TRUE;
#endif

	/* try to exit running games gracefully */
	if (go_game_up) go_challenge_cleanup(TRUE);

	/* terminate cleanly */
	writeToPipe("quit");
	wait_for_response();

	/* discard any possible answers we were still waiting for */
	go_engine_processing = 0;

	/* close pipes to it */
	fclose(fr);
	fclose(fw);
	go_engine_up = FALSE;

#ifdef DISCARD_RESPONSES_WHEN_TERMINATING
	terminating = FALSE;
#endif
}

void go_challenge(int Ind) {
	player_type *p_ptr = Players[Ind];

	Send_store_special_clr(Ind, 4, 18);

	/* Only one player at a time */
	if (go_game_up) {
		Send_store_special_str(Ind, 6, 3, TERM_ORANGE, "Arrr.. sorry, I can't find a fitting player around right now");
		Send_store_special_str(Ind, 7, 3, TERM_ORANGE, "..wasn't that guy here just a minute ago?!");
		Send_store_special_str(Ind, 8, 3, TERM_ORANGE, "Well sorry, why don't you come back again a bit later!");
		return;
	}
	if (!go_engine_up) {
		Send_store_special_str(Ind, 6, 3, TERM_ORANGE, "Sorry, I don't have a fitting player around right now, darn..");
		Send_store_special_str(Ind, 7, 5, TERM_ORANGE, "why don't you come back a bit later!");
		return;
	}

	/* Owner talk about how you suck..or not!
	   And making a new proposal accordingly. */
	p_ptr->request_id = RID_GO;
	p_ptr->request_type = RTYPE_CFR;
	switch (p_ptr->go_level) {
	case 0:
		Send_store_special_str(Ind, 6, 3, TERM_ORANGE, "So you think you're any good at this huh?");
		Send_store_special_str(Ind, 7, 3, TERM_ORANGE, "Well then, let's see some money. If you win, you'll get it double!");
		Send_store_special_str(Ind, 9, 3, TERM_ORANGE, "If you lose, and I dun' expect much really, I'll just keep it until");
		Send_store_special_str(Ind, 10, 3, TERM_ORANGE, "you've become a bit stronger and can win it back.. as if, haha!");
		Send_store_special_str(Ind, 12, 3, TERM_ORANGE, "..oh and I'll let you take black even, what d'you say eh?");
		Send_request_cfr(Ind, RID_GO, "Bet 500 Au? ");
		break;
	case 1:
		Send_request_cfr(Ind, RID_GO, "Bet 1000 Au? ");
		break;
	case 2:
		Send_request_cfr(Ind, RID_GO, "Bet 2000 Au? ");
		break;
	case 3:
		Send_request_cfr(Ind, RID_GO, "Bet 5000 Au? ");
		break;
	case 4:
		Send_request_cfr(Ind, RID_GO, "Bet 10000 Au? ");
		break;
	case 5:
		Send_request_cfr(Ind, RID_GO, "Bet 20000 Au? ");
		break;
	case 6:
		Send_request_cfr(Ind, RID_GO, "Bet 50000 Au? ");
		break;
	case TOP_RANK:
		Send_request_cfr(Ind, RID_GO, "Bet 100000 Au? ");
		break;
	}
}

void go_challenge_accept(int Ind) {
	player_type *p_ptr = Players[Ind];
	int wager = 500;

	Send_store_special_clr(Ind, 4, 18);

	/* Prepare to deduct wager */
	switch (p_ptr->go_level) {
	case 0: wager = 500; break;
	case 1: wager = 1000; break;
	case 2: wager = 2000; break;
	case 3: wager = 5000; break;
	case 4: wager = 10000; break;
	case 5: wager = 20000; break;
	case 6: wager = 50000; break;
	case TOP_RANK: wager = 100000; break;
	}
	if (p_ptr->au < wager) {
		Send_store_special_str(Ind, 8, 5, TERM_RED, "You don't have the money!");
		return;
	}

	/* Go engine busy? */
	if (go_game_up) {
		Send_store_special_str(Ind, 6, 3, TERM_ORANGE, "Arrr.. sorry, I can't find a fitting player around right now");
		Send_store_special_str(Ind, 7, 3, TERM_ORANGE, "..wasn't that guy here just a minute ago?!");
		Send_store_special_str(Ind, 8, 3, TERM_ORANGE, "Well sorry, why don't you come back again a bit later!");
		return;
	}
	/* We're in! */
	go_game_up = TRUE;
	go_engine_player_id = p_ptr->id;

	/* Deduct wager */
	p_ptr->au -= wager;
	Send_gold(Ind, p_ptr->au, p_ptr->balance);

	/* Introduce opponent */
	switch (p_ptr->go_level) {
	case 0:
		Send_store_special_str(Ind, 6, 3, TERM_ORANGE, "Alright then, I've got just the man for you, wait here..");
		Send_store_special_str(Ind, 8, 10, TERM_ORANGE, "<you hear some shouting>");
		Send_store_special_str(Ind, 10, 3, TERM_ORANGE, "This is \377yDougan The Quick\377o,");
		Send_store_special_str(Ind, 11, 3, TERM_ORANGE, "let's see if you're any good at all..I doubt it. You ready?");
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		break;
	case 6:
		break;
	case TOP_RANK:
		break;
	}

	/* Wait for player keypress to start */
	p_ptr->request_id = RID_GO_START;
	p_ptr->request_type = RTYPE_KEY;
	Send_request_key(Ind, RID_GO_START, "- Press any key to start the game of Go! -");
}

/* Initialize game and initiate first move */
void go_challenge_start(int Ind) {
	player_type *p_ptr = Players[Ind];
	int n;

	Send_store_special_clr(Ind, 4, 18);
	if (go_err(DOWN, DOWN, "go_challenge_start")) return;

	/* somehow spread from 30k to dan level.. Final stage: Take white!
	   Place opponent and configure game parameters accordingly. */
	/* we have 512 MB.. *//* time per CPU move default is 10s */
	switch (p_ptr->go_level) {
	case 0:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 2, TERM_YELLOW, "Dougan, The Quick");
		writeToPipe("uct_max_memory 10000000");
		writeToPipe("go_param timelimit 3");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case 1:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Rabbik, The Bold");
		writeToPipe("uct_max_memory 100000000");
		writeToPipe("go_param timelimit 5");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case 2:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Lima, The Witty");
		writeToPipe("uct_max_memory 100000000");
		writeToPipe("go_param timelimit 10");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case 3:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Zar'am, The Stoic");
		writeToPipe("uct_max_memory 200000000");
		writeToPipe("go_param timelimit 10");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case 4:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Talithe, The Sharp");
		writeToPipe("uct_max_memory 200000000");
		writeToPipe("go_param timelimit 15");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case 5:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Glynn, The Silent");
		writeToPipe("uct_max_memory 300000000");
		writeToPipe("go_param timelimit 20");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case 6:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Aro, The Wise");
		writeToPipe("uct_max_memory 300000000");
		writeToPipe("go_param timelimit 20");
		player_timelimit_sec = GO_TIME_PY;
		break;
	case TOP_RANK:
		Send_store_special_str(Ind, 4, GO_BOARD_X - 4, TERM_YELLOW, "Aro, The Wise");
		writeToPipe("uct_max_memory 300000000");
		writeToPipe("go_param timelimit 20");
		player_timelimit_sec = GO_TIME_PY;
		break;
	}

	/* think while opponent is thinking? */
	if (p_ptr->go_level <= 0) writeToPipe("uct_param_player ponder 0");
	else writeToPipe("uct_param_player ponder 1");

	/* Set colour and notice game start. */
	CPU_has_white = (p_ptr->go_level != TOP_RANK);
	CPU_now_to_move = !CPU_has_white;
	player_timeleft_sec = player_timelimit_sec;

	/* Draw board */
	Send_store_special_str(Ind, GO_BOARD_Y, GO_BOARD_X, TERM_UMBER, " ABCDEFGHJ ");
	Send_store_special_str(Ind, GO_BOARD_Y + 1, GO_BOARD_X, TERM_UMBER, "9.........9");
	Send_store_special_str(Ind, GO_BOARD_Y + 2, GO_BOARD_X, TERM_UMBER, "8.........8");
	Send_store_special_str(Ind, GO_BOARD_Y + 3, GO_BOARD_X, TERM_UMBER, "7.........7");
	Send_store_special_str(Ind, GO_BOARD_Y + 4, GO_BOARD_X, TERM_UMBER, "6.........6");
	Send_store_special_str(Ind, GO_BOARD_Y + 5, GO_BOARD_X, TERM_UMBER, "5.........5");
	Send_store_special_str(Ind, GO_BOARD_Y + 6, GO_BOARD_X, TERM_UMBER, "4.........4");
	Send_store_special_str(Ind, GO_BOARD_Y + 7, GO_BOARD_X, TERM_UMBER, "3.........3");
	Send_store_special_str(Ind, GO_BOARD_Y + 8, GO_BOARD_X, TERM_UMBER, "2.........2");
	Send_store_special_str(Ind, GO_BOARD_Y + 9, GO_BOARD_X, TERM_UMBER, "1.........1");
	Send_store_special_str(Ind, GO_BOARD_Y + 10, GO_BOARD_X, TERM_UMBER, " ABCDEFGHJ ");

	/* Initialize moves/passes tracker */
	pass_count = 0;
	last_move_was_pass = FALSE;

	/* Initialize game board tracker */
	for (n = 0; n < 9; n++) {
		strcpy(board_line_old[n], ".........");
		strcpy(board_line[n], ".........");
	}

	/* Init misc control elements */
	received_board_visuals = FALSE;
	game_over = FALSE;

	/* 'Start the clock' aka the game */
	s_printf("GO_INIT: ---GAME START---\n");
	tstart = time(NULL);
	tmstart = localtime(&tstart);

	/* Initiate human player input loop */
	Send_store_special_str(Ind, 1, 0, TERM_WHITE, "(Enter coordinates to place a stone, eg 'd5'; 'p' to pass; ESC/'r' to resign.)");
	if (CPU_has_white) {
		p_ptr->request_id = RID_GO_MOVE;
		p_ptr->request_type = RTYPE_STR;
		Send_request_str(Ind, RID_GO_MOVE, "Enter your move: ", "");
	}
	else go_engine_move_CPU();
}

/* Get a response, non-blocking; using go_engine_processing */
void go_engine_process(void) {
//	if (go_err(DOWN, DOWN, "go_engine_process")) {
	if (go_err(DOWN, NONE, "go_engine_process")) {
		go_engine_processing = 0;
		return;
	}

	/* Read all the data that's already waiting for us
	   in the receive buffer (pipe) */
	while (test_for_response() >= 0);
}

/* Actually play the game.
   If CPU starts, py_move is NULL on first call of this function.
   Return values:
   0 - game goes on		1 - invalid move, try again
   2 - py lost on time		3 - py resigned
   4 - cpu lost on time		5 - cpu resigned
   1000 + score - black wins	2000 + score - white wins
   6 - jigo. */
int go_engine_move_human(int Ind, char *py_move) {
	char tmp[80];
	player_type *p_ptr = Players[Ind];

	if (go_err(DOWN, DOWN, "go_engine_move_human")) return -1;

	/* parse player move */
	if (strlen(py_move) == 2) { /* Play a normal move on a board grid */
		/* send move */
		if (CPU_has_white) snprintf(tmp, 14, "play black %s", py_move);
		else snprintf(tmp, 14, "play white %s", py_move);
		tmp[13] = '\0';
		writeToPipe(tmp);
		go_engine_next_action = NACT_MOVE_CPU;
		return 0;
	} else if (!strcmp(py_move, "p")) { /* Pass */
		if (CPU_has_white) writeToPipe("play black pass");
		else writeToPipe("play white pass");
		go_engine_next_action = NACT_MOVE_CPU;
		pass_count++;
		last_move_was_pass = TRUE;
		return 0;
	} else if (!strcmp(py_move, "r") ||
	    !strcmp(py_move, "\e")) { /* Resign */
#if 0 /* there is no resign command */
		if (CPU_has_white) writeToPipe("play black resign");
		else writeToPipe("play white resign");
		go_engine_next_action = NACT_MOVE_CPU;
#endif
		go_engine_move_result(3);
		return 3; //resign
	}

	p_ptr->request_id = RID_GO_MOVE;
	p_ptr->request_type = RTYPE_STR;
	Send_request_str(Ind, RID_GO_MOVE, "Invalid move. Try again: ", "");
	return 1; //invalid move
}

/* Display and countdown 'clocks' */
void go_engine_clocks(void) {
	int Ind;
	char clock[6];

	if (go_err(DOWN, DOWN, "go_engine_clocks")) return;
	/* Hold clocks during preparation phase (ie before game really commences) */
	if (game_over) return;

	/* Player still with us? */
	if (!(Ind = lookup_player_ind(go_engine_player_id))) {
		go_challenge_cleanup(FALSE);
		return;
	}

	/* no timelimit? */
	if (!player_timelimit_sec) return;

	/* countdown */
	if (player_timeleft_sec) player_timeleft_sec--;
	if (!player_timeleft_sec) {
		/* loss on time */
		Send_request_abort(Ind);
		if (CPU_now_to_move) go_engine_move_result(4); /* not possible except for mad lag */
		else go_engine_move_result(2);
	}

	sprintf(clock, "%02d", player_timeleft_sec);
	if (CPU_now_to_move)
		Send_store_special_str(Ind, GO_BOARD_Y, GO_BOARD_X - 8, TERM_L_RED, clock);
	else
		Send_store_special_str(Ind, GO_BOARD_Y + 9, GO_BOARD_X - 8, TERM_L_RED, clock);
}


/* ------------------------------------------------------------------------- */


static void go_engine_move_CPU() {
	int Ind;

	if (go_err(DOWN, DOWN, "go_engine_move_CPU")) return;

	/* Player still with us? */
	if (!(Ind = lookup_player_ind(go_engine_player_id))) {
		go_challenge_cleanup(FALSE);
		return;
	}

	/* Get CPU move */
	if (CPU_has_white) writeToPipe("genmove white");
	else writeToPipe("genmove black");
	go_engine_next_action = NACT_MOVE_PLAYER;
}

/* Reads current pipe_buf[][] and assumes it contains the response
   to a player move. Evaluates result and further proceeding from that. */
static int verify_move_human(void) {
	int Ind;
	player_type *p_ptr;
	char clock[6];

	if (go_err(DOWN, DOWN, "verify_move_human")) return -2;

	/* Player still with us? */
	if (!(Ind = lookup_player_ind(go_engine_player_id))) {
		go_challenge_cleanup(FALSE);
		return -1;
	}
	p_ptr = Players[Ind];

	/* Catch timeout! Oops. */
	if (!strcmp(pipe_buf[MAX_GTP_LINES - 1], "SgTimeRecord: outOfTime")) {
		s_printf("GO_GAME: timeout %s\n", p_ptr->name);
		return 2; //time over (py)
	}
	/* Catch illegal moves */
//	if (strstr("? point outside board"))
	if (pipe_buf[MAX_GTP_LINES - 1][0] == '?') {
		p_ptr->request_id = RID_GO_MOVE;
		p_ptr->request_type = RTYPE_STR;
		Send_request_str(Ind, RID_GO_MOVE, "Invalid move. Try again: ", "");
		return 1; //invalid move
	}
	/* Test for game over by double-passe */
	if (pass_count == 2) {
		printf("Both players passed consecutively, so the game ends.\n");
		/* determine score */
		writeToPipe("final_score");
		if (pipe_buf[MAX_GTP_LINES - 1][2] == 'W') {
			/* paranoia: '= W+0' */
			if (pipe_buf[MAX_GTP_LINES - 1][4] != '0') {
				printf("White wins!\n");
				return 2000 + atoi(&pipe_buf[MAX_GTP_LINES - 1][4]);
			} else {
				printf("The game is a draw!\n");
				return 6;
			}
		} else if (pipe_buf[MAX_GTP_LINES - 1][2] == 'B') {
			/* paranoia: '= B+0' */
			if (pipe_buf[MAX_GTP_LINES - 1][4] != '0') {
				printf("Black wins!\n");
				return 1000 + atoi(&pipe_buf[MAX_GTP_LINES - 1][4]);
			} else {
				printf("The game is a draw!\n");
				return 6;
			}
		}
		printf("The game is a draw!\n");
		return 6; //jigo!
	} else if (!last_move_was_pass) pass_count = 0;
	last_move_was_pass = FALSE;

	/* display board after player's move */
s_printf("SHOWBOARD_HUMAN\n");
	writeToPipe("showboard");

	player_timeleft_sec = player_timelimit_sec;
	Send_store_special_str(Ind, GO_BOARD_Y + 9, GO_BOARD_X - 8, TERM_L_RED, "  ");
	sprintf(clock, "%02d", player_timeleft_sec);
	Send_store_special_str(Ind, GO_BOARD_Y, GO_BOARD_X - 8, TERM_L_RED, clock);
	CPU_now_to_move = TRUE;

	return 0;
}

/* Reads current pipe_buf[][] and assumes it contains the response
   to a CPU move. Evaluates result and further proceeding from that. */
static int verify_move_CPU(void) {
	int Ind;
	char cpu_move[80], clock[6];

	/* Player still with us? */
	if (!(Ind = lookup_player_ind(go_engine_player_id))) {
		go_challenge_cleanup(FALSE);
		return -1;
	}

	if (go_err(DOWN, DOWN, "verify_move_CPU")) return -2;

	/* Catch timeout! Oops. */
	if (!strcmp(pipe_buf[MAX_GTP_LINES - 1], "SgTimeRecord: outOfTime")) {
		printf("Your opponent's time has run out, therefore you have won the match!\n");
		return 4;
	}
	/* CPU resigns */
	else if (!strcmp(pipe_buf[MAX_GTP_LINES - 1], "= resign")) {
		printf("Your opponent has resigned, you have won the match!\n");
		return 5;
	}
	/* CPU passes */
	else if (!strcmp(pipe_buf[MAX_GTP_LINES - 1], "= PASS")) {
		printf("CPU passed.\n");
		Send_store_special_str(Ind, 6, GO_BOARD_X + 3, TERM_YELLOW, "'pass'");
		pass_count++;
	}
	/* CPU plays a normal move */
	else if (strlen(pipe_buf[MAX_GTP_LINES - 1]) == 4 &&
	    pipe_buf[MAX_GTP_LINES - 1][0] == '=' &&
	    pipe_buf[MAX_GTP_LINES - 1][1] == ' ') {
		strncpy(cpu_move, pipe_buf[MAX_GTP_LINES - 1], 4);
		cpu_move[0] = cpu_move[2];
		cpu_move[1] = cpu_move[3];
		cpu_move[2] = 0;
		printf("CPU move -%s-.\n", cpu_move);
		pass_count = 0;
		Send_store_special_str(Ind, 6, GO_BOARD_X + 5, TERM_YELLOW, cpu_move);
	}

	/* Test for game over by double-passe */
	if (pass_count == 2) {
		printf("Both players passed consecutively, so the game ends.\n");
		/* determine score */
		writeToPipe("final_score");
		wait_for_response();
		if (pipe_buf[MAX_GTP_LINES - 1][2] == 'W') {
			/* paranoia: '= W+0' */
			if (pipe_buf[MAX_GTP_LINES - 1][4] != '0') {
				printf("White wins!\n");
				return 2000 + atoi(&pipe_buf[MAX_GTP_LINES - 1][4]);
			} else {
				printf("The game is a draw!\n");
				return 6;
			}
		} else if (pipe_buf[MAX_GTP_LINES - 1][2] == 'B') {
			/* paranoia: '= B+0' */
			if (pipe_buf[MAX_GTP_LINES - 1][4] != '0') {
				printf("Black wins!\n");
				return 1000 + atoi(&pipe_buf[MAX_GTP_LINES - 1][4]);
			} else {
				printf("The game is a draw!\n");
				return 6;
			}
		}
		printf("The game is a draw!\n"); /* includes '? cannot score' */
		return 6;//jigo!
	}

	/* display board after CPU's move */
s_printf("SHOWBOARD_CPU\n");
	writeToPipe("showboard");

	player_timeleft_sec = player_timelimit_sec;
	Send_store_special_str(Ind, GO_BOARD_Y, GO_BOARD_X - 8, TERM_L_RED, "  ");
	sprintf(clock, "%02d", player_timeleft_sec);
	Send_store_special_str(Ind, GO_BOARD_Y + 9, GO_BOARD_X - 8, TERM_L_RED, clock);
	CPU_now_to_move = FALSE;

	/* continue playing */
	return 0;
}

static void go_engine_move_result(int move_result) {
	bool invalid_move = FALSE, lost = FALSE, won = FALSE;
	char result[80];
	int Ind;
	player_type *p_ptr;

	if (go_err(DOWN, DOWN, "go_engine_move_result")) return;

	/* Player still with us? */
	if (!(Ind = lookup_player_ind(go_engine_player_id))) {
		go_challenge_cleanup(FALSE);
		return;
	}
	p_ptr = Players[Ind];

	switch (move_result) {
	case 0: break;
	case 1: invalid_move = TRUE; break;
	case 2:
		if (CPU_has_white) writeToPipe("go_set_info result W+Time");
		else writeToPipe("go_set_info result B+Time");
		strcpy(result, "\377r*** You lost on time! ***");
		lost = TRUE;
		break;
	case 3:
		if (CPU_has_white) writeToPipe("go_set_info result W+Resign");
		else writeToPipe("go_set_info result B+Resign");
		strcpy(result, "\377r*** You resigned! ***");
		lost = TRUE;
		break;
	case 4:
		if (CPU_has_white) writeToPipe("go_set_info result B+Time");
		else writeToPipe("go_set_info result W+Time");
		strcpy(result, "\377G*** Your opponent lost on time! ***");
		won = TRUE;
		break;
	case 5:
		if (CPU_has_white) writeToPipe("go_set_info result B+Resign");
		else writeToPipe("go_set_info result W+Resign");
		strcpy(result, "\377G*** Your opponent resigned! ***");
		won = TRUE;
		break;
	case 6:
		writeToPipe("go_set_info result Jigo");
		strcpy(result, "\377o** The game is a draw! **");
		break;
	default:
		if (move_result >= 2000) { /* White wins */
			if (CPU_has_white) {
				sprintf(result, "\377r*** Your opponent won by %d point%s! ***", move_result - 2000, move_result - 2000 == 1 ? "" : "s");
				lost = TRUE;
			} else {
				sprintf(result, "\377G*** You won by %d point%s! ***", move_result - 2000, move_result - 2000 == 1 ? "" : "s");
				won = TRUE;
			}
		} else { /* Black wins */
			if (CPU_has_white) {
				sprintf(result, "\377G*** You won by %d point%s! ***", move_result - 1000, move_result - 1000 == 1 ? "" : "s");
				won = TRUE;
			} else {
				sprintf(result, "\377r*** Your opponent won by %d point%s! ***", move_result - 1000, move_result - 1000 == 1 ? "" : "s");
				lost = TRUE;
			}
		}
	}

	if (move_result >= 2) {
		Send_store_special_str(Ind, 1, 0, TERM_WHITE, "                                                                              ");
		Send_store_special_str(Ind, 6, GO_BOARD_X + 6 - strlen(result) / 2, TERM_WHITE, result);

		switch (p_ptr->go_level) {
		case 0:
			if (lost) {
				Send_store_special_str(Ind, 9, GO_BOARD_X + 13, TERM_ORANGE, "Heh, bloody beginner are ya?");
				Send_store_special_str(Ind, 10, GO_BOARD_X + 13, TERM_ORANGE, "Ah well, figure it out and");
				Send_store_special_str(Ind, 11, GO_BOARD_X + 13, TERM_ORANGE, "you'll beat him next time..");
			} else if (won) {
				Send_store_special_str(Ind, 9, GO_BOARD_X + 13, TERM_ORANGE, "Hah, I thought you'd make");
				Send_store_special_str(Ind, 10, GO_BOARD_X + 13, TERM_ORANGE, "short work of Dougan, saw");
				Send_store_special_str(Ind, 11, GO_BOARD_X + 13, TERM_ORANGE, "right through ya! I'll make");
				Send_store_special_str(Ind, 12, GO_BOARD_X + 13, TERM_ORANGE, "y'a better offer next time!");
//				p_ptr->go_level++;
			} else {
				Send_store_special_str(Ind, 9, GO_BOARD_X + 13, TERM_ORANGE, "You two are on the same..");
				Send_store_special_str(Ind, 10, GO_BOARD_X + 13, TERM_ORANGE, "level eh? Hahaha! That's");
				Send_store_special_str(Ind, 11, GO_BOARD_X + 13, TERM_ORANGE, "just great!");
			}
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		case TOP_RANK:
			break;
		}

		game_over = TRUE;
		go_challenge_cleanup(FALSE);
		return;
	}

	if (p_ptr->store_num == -1) {
		/* "player resigned by leaving the store" */
		go_challenge_cleanup(FALSE);
		return;
	}

	if (invalid_move) {
		p_ptr->request_id = RID_GO_MOVE;
		p_ptr->request_type = RTYPE_STR;
		Send_request_str(Ind, RID_GO_MOVE, "Invalid move. Try again: ", "");
		return;
	}

	/* Prompt for next move */
	if (CPU_to_move) {
		go_engine_move_CPU();
	} else {
		p_ptr->request_id = RID_GO_MOVE;
		p_ptr->request_type = RTYPE_STR;
		Send_request_str(Ind, RID_GO_MOVE, "Enter your move: ", "");
	}
}

/* Process incoming reply pieces to a previous command (Async read) */
static int test_for_response() {
	int i;
	char tmp[80];//, *tptr = tmp + 79;
	char pipe_line_buf[160];

#ifdef DISCARD_RESPONSES_WHEN_TERMINATING
	/* we might not even want any responses (and them causing slowdown) here,
	   in case we're doing a warm restart while TomeNET server keeps running! */
	if (terminating) return 0;
#endif

	if (go_err(DOWN, NONE, "test_for_response")) return -3;

	/* paranoia: invalid file/pipe? */
	if (!fr) {
		s_printf("GO_ENGINE: ERROR - fr is NULL.\n");
		go_engine_processing = 0;
		return -2;
	}

	strcpy(tmp, "");
	strcpy(pipe_line_buf, "");


	/* Handle program output ------------------------------------------- */
	readFromPipe(pipe_line_buf, &pipe_response_complete);

	/* No data waiting to be read? */
	if (!pipe_line_buf[0]) return -1;

	/* Process data: Build lines from it. Build whole response from several lines. */
	if (strlen(pipe_buf[pipe_buf_current_line]) + strlen(pipe_line_buf) >= 240) {
		printf("GO_ERROR: LINE TOO LONG\n");
		return 1;
	} else strcat(pipe_buf[pipe_buf_current_line], pipe_line_buf);

	/* Trim ending '\n' */
	if (pipe_buf[pipe_buf_current_line][strlen(pipe_buf[pipe_buf_current_line]) - 1] == '\n')
		pipe_buf[pipe_buf_current_line][strlen(pipe_buf[pipe_buf_current_line]) - 1] = 0;


	/* Not even got a while line? Exit now. ---------------------------- */
	if (pipe_line_buf[strlen(pipe_line_buf) - 1] != '\n') return 0;

	/* don't output just a single line feed */
#ifdef GO_DEBUGPRINT
	if (strlen(pipe_buf[pipe_buf_current_line])) printf("<%s>\n", pipe_buf[pipe_buf_current_line]);
#endif

	/* Check for certain important responses */
	if (strlen(pipe_buf[pipe_buf_current_line]) > 2 &&
	    /* Special answers? */
	    (strstr(pipe_buf[pipe_buf_current_line], "outOfTime") ||
	    /* either postitive answer? '= ...' */
	    (pipe_buf[pipe_buf_current_line][0] == '=' && pipe_buf[pipe_buf_current_line][1] == ' ') ||
	    /* or negative answer? '? ...' */
	    (pipe_buf[pipe_buf_current_line][0] == '?' && pipe_buf[pipe_buf_current_line][1] == ' '))) {
		/* Keep it in mind! */
		strncpy(tmp, pipe_buf[pipe_buf_current_line], 79);
		tmp[79] = 0;
	}

	/* Received a board layout line? (from 'showboard') */
	if (strlen(pipe_buf[pipe_buf_current_line]) > 2 &&
	    pipe_buf[pipe_buf_current_line][1] == ' ' &&
	    pipe_buf[pipe_buf_current_line][0] >= '1' && pipe_buf[pipe_buf_current_line][0] <= '9') {
		i = pipe_buf[pipe_buf_current_line][0] - '1';
		//strncpy(board_line[i], pipe_buf[pipe_buf_current_line] + 2, 10); it's SPACED!:
		/* despace */
		board_line[i][0] = pipe_buf[pipe_buf_current_line][2];
		board_line[i][1] = pipe_buf[pipe_buf_current_line][4];
		board_line[i][2] = pipe_buf[pipe_buf_current_line][6];
		board_line[i][3] = pipe_buf[pipe_buf_current_line][8];
		board_line[i][4] = pipe_buf[pipe_buf_current_line][10];
		board_line[i][5] = pipe_buf[pipe_buf_current_line][12];
		board_line[i][6] = pipe_buf[pipe_buf_current_line][14];
		board_line[i][7] = pipe_buf[pipe_buf_current_line][16];
		board_line[i][8] = pipe_buf[pipe_buf_current_line][18];
		board_line[i][9] = 0;
		received_board_visuals = TRUE;
	}

	/* If response is too long, just overwrite the final line repeatedly -
	   it should be all that we need at this point anymore. */
	if (pipe_buf_current_line < MAX_GTP_LINES - 1 -1) {
		pipe_buf_current_line++;
	}
	strcpy(pipe_buf[pipe_buf_current_line], "");

	/* Hack: Deliver certain important responses back outside,
	   using an extra last line, reserved for this purpose: */
	/* also convert to upper-case */
//	while (tptr > tmp) *tptr-- = toupper(*tptr);
	if (tmp[0]) strcpy(pipe_buf[MAX_GTP_LINES - 1], tmp);


	/* Not yet a whole response? Exit here. ---------------------------- */
	if (pipe_response_complete != 2) return 0;

	/* If we read a complete response to a command,
	   reduce amount of pending responses left,
	   and check if we have to react ie initiate a new command */
//	printf("---RESPONSE COMPLETE---\n");
	pipe_response_complete = 0;

	/* If we lost on time, go_game_up might be FALSE */
	if (go_game_up && received_board_visuals) go_engine_board();
	received_board_visuals = FALSE;

	/* one less pending response */
	go_engine_processing--;

	/* do we have to react with a new command?
	   This should only happen if no more responses are expected
	   to come in: So we don't initiate a new command before an
	   'important' command's response has been processed completely. */
	if (go_engine_processing == 0) {
		/* If we lost on time, go_game_up might be FALSE */
		if (go_game_up) switch (go_engine_next_action) {
		case NACT_NONE:		/* nothing to do, last command was just misc stuff */
			break;
		case NACT_MOVE_PLAYER:	/* Last command was to request the player's next move */
			/* remember whose turn it is, so we can continue with erased NACT */
			CPU_to_move = FALSE;
			/* erase NACT, so we can writeToPipe misc stuff again */
			go_engine_next_action = NACT_NONE;
			/* evaluate last move, and initiate next one */
			go_engine_move_result(verify_move_CPU());
			break;
		case NACT_MOVE_CPU:	/* Last command was to request the CPU's next move */
			/* remember whose turn it is, so we can continue with erased NACT */
			CPU_to_move = TRUE;
			/* erase NACT, so we can writeToPipe misc stuff again */
			go_engine_next_action = NACT_NONE;
			/* evaluate last move, and initiate next one */
			go_engine_move_result(verify_move_human());
			break;
		}
		else go_engine_next_action = NACT_NONE;
	}

	/* Initialize response buffer for next response.
	   (important for the last 'tmp' line storing special info.) */
	pipe_buf_current_line = 0;
	for (i = 0; i < MAX_GTP_LINES; i++) strcpy(pipe_buf[i], "");

	return 0;
}

/* Get a response, blocking */
static int wait_for_response() {
	int cont, r = 0, i;
	char lbuf[80], tmp[80];//, *tptr = tmp + 79;

#ifdef DISCARD_RESPONSES_WHEN_TERMINATING
	/* we might not even want any responses (and them causing slowdown) here,
	   in case we're doing a warm restart while TomeNET server keeps running! */
	if (terminating) return 0;
#endif

	/* paranoia: invalid file/pipe? */
	if (!fr) {
		s_printf("GO_ENGINE: ERROR - fr is NULL.\n");
		return 1;
	}

	/* reduce pending responses by one, which we are waiting for, in here */
	go_engine_processing--;

	cont = 0;
	do {
		strcpy(lbuf, "");
		strcpy(pipe_buf[r], "");
		while (1) {
			/* handle program output */
			readFromPipe(lbuf, &cont);
			if (lbuf[0]) {
				if (strlen(pipe_buf[r]) + strlen(lbuf) >= 240) {
					printf("GO_ERROR: LINE TOO LONG\n");
					break;
				} else strcat(pipe_buf[r], lbuf);
				if (pipe_buf[r][strlen(pipe_buf[r]) - 1] == '\n') pipe_buf[r][strlen(pipe_buf[r]) - 1] = 0;

				if (lbuf[strlen(lbuf) - 1] == '\n') {
					/* don't output just a single line feed */
#ifdef GO_DEBUGPRINT
					if (strlen(pipe_buf[r])) printf("<%s>\n", pipe_buf[r]);
#endif
					/* Check for certain important responses */
					if (strlen(pipe_buf[r]) > 2 &&
					    /* Special answers? */
					    (strstr(pipe_buf[r], "outOfTime") ||
					    /* either postitive answer? '= ...' */
					    (pipe_buf[r][0] == '=' && pipe_buf[r][1] == ' ') ||
					    /* or negative answer? '? ...' */
					    (pipe_buf[r][0] == '?' && pipe_buf[r][1] == ' '))) {
						/* Keep it in mind! */
						strncpy(tmp, pipe_buf[r], 79);
						tmp[79] = 0;
					}
					/* Received a board layout line? (from 'showboard') */
					if (strlen(pipe_buf[r]) > 2 &&
					    pipe_buf[r][1] == ' ' &&
					    pipe_buf[r][0] >= '1' && pipe_buf[r][0] <= '9') {
						i = pipe_buf[r][0] - '1';
						//strncpy(board_line[i], pipe_buf[r] + 2, 10); it's SPACED!:
						/* despace */
						board_line[i][0] = pipe_buf[r][2];
						board_line[i][1] = pipe_buf[r][4];
						board_line[i][2] = pipe_buf[r][6];
						board_line[i][3] = pipe_buf[r][8];
						board_line[i][4] = pipe_buf[r][10];
						board_line[i][5] = pipe_buf[r][12];
						board_line[i][6] = pipe_buf[r][14];
						board_line[i][7] = pipe_buf[r][16];
						board_line[i][8] = pipe_buf[r][18];
						board_line[i][9] = 0;
					}
					break;
				}
			}
		}
		if (r < MAX_GTP_LINES - 1 -1) r++;
	} while (cont != 2);
//	printf("---RESPONSE COMPLETE---\n");

	/* Hack: Deliver certain important responses back outside */
#if 0
	/* also convert to upper-case */
	while (tptr > tmp) *tptr-- = toupper(*tptr);
#endif
	strcpy(pipe_buf[MAX_GTP_LINES - 1], tmp);
	return 0;
}

/* Write a line of data into the pipe */
static void writeToPipe(char *data) {
#ifdef GO_DEBUGPRINT
	printf("GO_COMMAND: <%s> ### NACT: %d\n", data, go_engine_next_action);
//	printf("writeToPipe: %s\n", fw == NULL ? "NULL" : "ok");
//fw==NULL -> Send_store_special_str(Ind, 6, 3, TERM_ORANGE, "Arrr.. sorry, I can't find a fitting player around right now");
#endif
//	if (go_err(DOWN, NONE, "writeToPipe")) return;

	/* Before actually writing, clear all pending replies we can get! */
	if (go_engine_processing) go_engine_process();

	/* paranoia: catch overflows */
	if (go_engine_processing > 1000) {
		s_printf("GO_ENGINE: ERROR - over 1000 pending replies.\n");
		return;
	}
	/* increase pending replies by one, ie the one to this command we now send */
	go_engine_processing++;

	/* more paranoia: invalid file/pipe? */
	if (!fw) {
		s_printf("GO_ENGINE: ERROR - fw is NULL.\n");
		return;
	}
	fprintf(fw, "%s\n", data);
	fflush(fw);
}

/* Read a line of data from the pipe if there is any, else skip */
static void readFromPipe(char *buf, int *cont) {
	int ch = 0;
	char c[2];

//	if (go_err(DOWN, NONE, "readFromPipe")) return;

	strcpy(buf, "");
	while (1) {
		ch = fgetc(fr);
		if (!ch) break;
		if (ch == EOF) {
			(*cont) = 0;
			break;
		}

		sprintf(c, "%c", (char)ch);
		if (c[0] == '\n') (*cont)++;
		else (*cont) = 0;

		strcat(buf, c);
		if (c[0] == '\n' || strlen(buf) == 79) break;
	}
}

/* Handle engine startup process.
   Blocking reading is ok, because server is just starting up. */
static int handle_loading() {
	char lbuf[80], reply[240];
	int cont = 0;
	strcpy(reply, "");
	strcpy(lbuf, "");

	do {
		if (lbuf[0] && lbuf[strlen(lbuf) - 1] == '\n') strcpy(reply, "");
		readFromPipe(lbuf, &cont);
		if (lbuf[0]) {
//			printf("<%s>\n", lbuf);
			if (strlen(reply) + strlen(lbuf) >= 240) {
				printf("GO_ERROR: LINE TOO LONG\n");
				return 1;
			}
			strcat(reply, lbuf);
			if (reply[0] && reply[strlen(reply) - 1] == '\n') {
				reply[strlen(reply) - 1] = 0;
#ifdef GO_DEBUGPRINT
				printf("<%s>\n", reply);
#endif
			}
		}
		/* wait till startup has been completed before proceeding */
		if (strstr(reply, "... ok") != NULL) cont = 3;
	} while (cont != 3);
	return 0;
}

/* Cleanup: Save game, reset board.
   Possibly ignore responses since they aren't needed, and just flush
   the replies out of the pipe when the next game is initialized.
   'server_shutdown' must be TRUE exactly if server is shut down,
   it'll just use blocking reading, for reading the replies. */
static void go_challenge_cleanup(bool server_shutdown) {
	char tmp[80];

	if (go_err(DOWN, DOWN, "go_challenge_cleanup")) return;

	/* If we have to stop prematurely, interprete it as loss for the player.
	   Note: The 'result' info is cleared by 'clear_board' below. */
	if (!game_over) {
		if (CPU_has_white) writeToPipe("go_set_info result W+Forfeit");
		else writeToPipe("go_set_info result B+Forfeit");
		if (server_shutdown) wait_for_response();
		game_over = TRUE;
	}

	/* for saving game record to disk */
	tend = time(NULL);
	tmend = localtime(&tend);

	/* Save it as sgf */
	sprintf(tmp, "savesgf go/go-%d%d%d-%d%d.sgf",
	    1900 + tmstart->tm_year, tmstart->tm_mon + 1, tmstart->tm_mday,
	    tmstart->tm_hour, tmstart->tm_min);
	writeToPipe(tmp); //(saves current game (and tree if some global flag was set) to sgf)
	if (server_shutdown) wait_for_response();

	/* Prepare for next game in advance - skip on server shutdown */
	if (!server_shutdown) writeToPipe("clear_board");

	/* Clean up everything */
	go_game_up = FALSE;
	go_engine_next_action = NACT_NONE;
//	go_engine_processing = 0;

}

/* Screen operations only: Update the board visuals for the player */
static void go_engine_board(void) {
	int n, x;
	char nline[30];
	int Ind;
	player_type *p_ptr;

	if (go_err(DOWN, DOWN, "go_engine_board")) return;

	/* Player still with us? */
	if (!(Ind = lookup_player_ind(go_engine_player_id))) {
		go_challenge_cleanup(FALSE);
		return;
	}
	p_ptr = Players[Ind];

	for (n = 0; n < 9; n++) {
		/* line hasn't been changed? */
		if (!strcmp(board_line_old[n], board_line[n])) continue;
		/* line has been changed: Update visuals */
		strcpy(nline, "");
		for (x = 0; x < 9; x++) {
			if (board_line[n][x] == 'X') strcat(nline, "\377Do"); //black
			else if (board_line[n][x] == 'O') strcat(nline, "\377wo"); //white
			else strcat(nline, "\377u."); //free
		}

		/* send it */
		Send_store_special_str(Ind, GO_BOARD_Y + 9 - n, GO_BOARD_X + 1, TERM_UMBER, nline);
		strcpy(board_line_old[n], board_line[n]);
	}
	Net_output1(Ind);
}

static bool go_err(int engine_status, int game_status, char *name) {
	if (engine_status == DOWN) {
		if (!go_engine_up) {
			s_printf("GO_ERR: go engine down in %s().\n", name);
			return TRUE;
		}
	} else if (engine_status == UP) {
		if (go_engine_up) {
			s_printf("GO_ERR: go engine up in %s().\n", name);
			return TRUE;
		}
	}
	if (game_status == DOWN) {
		if (!go_game_up) {
			s_printf("GO_ERR: go game down in %s().\n", name);
			return TRUE;
		}
	} else if (game_status == UP) {
		if (go_game_up) {
			s_printf("GO_ERR: go game up in %s().\n", name);
			return TRUE;
		}
	}
	return FALSE;
}

#endif /* ENABLE_GO_GAME */
