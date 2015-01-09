#include "main.h"


#include <ncurses.h>

static WINDOW *statuswin;
static WINDOW *hdrwin;
static WINDOW *mainwin;
static WINDOW *blockpad;
static int blockpad_pos = 0;

static relation_info *rels;
static int nrels;

static int selected_rel = 0;
static int blockpad_pos_save = 0;

static int blockpad_lines = 20;


static BlockNumber displayed_block = InvalidBlockNumber;
static char *block;

static void
scroll_blockpad(int n)
{
	blockpad_pos += n;
	if (blockpad_pos < 0)
		blockpad_pos = 0;
	if (blockpad_pos > 550)
		blockpad_pos = 550;
}

static void
move_selection(int n)
{
	mvwchgat(blockpad, selected_rel, 0, 40, A_NORMAL, 0, NULL);

	selected_rel += n;
	if (selected_rel >= nrels)
		selected_rel = nrels - 1;
	if (selected_rel < 0)
		selected_rel = 0;

	if (selected_rel < blockpad_pos)
		scroll_blockpad(selected_rel - blockpad_pos);
	else if (selected_rel - blockpad_pos > blockpad_lines)
		scroll_blockpad(selected_rel - blockpad_pos - blockpad_lines);

	mvwchgat(blockpad, selected_rel, 0, 40, A_REVERSE, 0, NULL);
}

static void
refresh_screen(void)
{
	wrefresh(hdrwin);
	prefresh(blockpad, blockpad_pos, 0, 2, 0, blockpad_lines + 2, 80);
}

int main()
{
	int ch;
	
	initscr();			/* start curses	*/
	raw();				/* disable line buffering */
	keypad(stdscr, TRUE);
	noecho();

	start_color();
	use_default_colors();
	init_pair(10, COLOR_WHITE, COLOR_RED);

	statuswin = subwin(stdscr, 1, 0, 0, 0);
	hdrwin = subwin(stdscr, 1, 0, 1, 0);

	blockpad = newpad(1000, 60);

	errorw = statuswin;

	wclear(stdscr);

	db_connect();
	if (db_is_connected())
	{
		reporterror("connected");
		rels = db_fetch_relations(&nrels);
		if (rels)
			display_relations(hdrwin, blockpad, rels, nrels);
	}

	move_selection(0);
	refresh();

	for (;;)
	{
		refresh_screen();

		ch = getch();

		werase(statuswin);
		wrefresh(statuswin);

		switch (ch)
		{
			case KEY_UP:
				if (displayed_block != InvalidBlockNumber)
					scroll_blockpad(-1);
				else
					move_selection(-1);
				break;
			case KEY_DOWN:
				if (displayed_block != InvalidBlockNumber)
					scroll_blockpad(1);
				else
					move_selection(1);
				break;
			case KEY_NPAGE:
				if (displayed_block != InvalidBlockNumber)
					scroll_blockpad(15);
				else
					move_selection(15);
				break;
			case KEY_PPAGE:
				if (displayed_block != InvalidBlockNumber)
					scroll_blockpad(-15);
				else
					move_selection(-15);
				break;

			case KEY_ENTER:
			case KEY_RIGHT:
				if (displayed_block == InvalidBlockNumber)
				{
					block = db_fetch_block(rels[selected_rel].relname, "main", 0);
					if (block)
					{
						blockpad_pos_save = blockpad_pos;
						blockpad_pos = 0;
						displayed_block = 0;
						display_block(hdrwin, blockpad, block, displayed_block);
					}
				}
				else
				{
					displayed_block++;
					block = db_fetch_block(rels[selected_rel].relname, "main", displayed_block);
					if (block)
					{
						blockpad_pos_save = blockpad_pos;
						blockpad_pos = 0;
						display_block(hdrwin, blockpad, block, displayed_block);
					}
				}
				break;

			case KEY_LEFT:
				if (displayed_block != InvalidBlockNumber)
				{
					displayed_block--;
					if (displayed_block == InvalidBlockNumber)
					{
						blockpad_pos = blockpad_pos_save;
						if (displayed_block == InvalidBlockNumber)
						{
							display_relations(hdrwin, blockpad, rels, nrels);
							mvwchgat(blockpad, selected_rel, 0, 40, A_REVERSE, 0, NULL);
						}
					}
					else
					{
						block = db_fetch_block(rels[selected_rel].relname, "main", displayed_block);
						if (block)
						{
							blockpad_pos_save = blockpad_pos;
							blockpad_pos = 0;
							display_block(hdrwin, blockpad, block, displayed_block);
						}
					}

				}
				break;

			case 'g':				/* goto block */
				if (displayed_block != InvalidBlockNumber)
				{
					char str[11];
					BlockNumber blkno;
					char *endptr;

					werase(hdrwin);
					mvwprintw(hdrwin, 0, 0, "Goto block: ");
					refresh_screen();
					echo();
					getnstr(str, sizeof(str) - 1);
					noecho();

					blkno = strtoul(str, &endptr, 10);
					if (*endptr != '\0')
					{
						werase(hdrwin);
						mvwprintw(hdrwin, 0, 0, "Invalid block number");
					}
					else
					{
						block = db_fetch_block(rels[selected_rel].relname, "main", blkno);
						if (block)
						{
							displayed_block = blkno;
							blockpad_pos_save = blockpad_pos;
							blockpad_pos = 0;
							displayed_block = 0;
							display_block(hdrwin, blockpad, block, displayed_block);
						}
					}
				}
				break;

			case 'q':
				endwin();			/* End curses mode		  */
				exit(0);
				break;
			default:
				reporterror("unknown key: %c", ch);
		}
	}
	endwin();			/* End curses mode		  */

	return 0;
}
