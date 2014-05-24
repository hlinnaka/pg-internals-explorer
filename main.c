#include "main.h"


#include <ncurses.h>

static WINDOW *hdrpad;
static WINDOW *blockpad;
static int blockpad_pos = 0;

static relation_info *rels;
static int nrels;

static int selected_rel = 0;
static int blockpad_pos_save;

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
	prefresh(hdrpad, 0, 0, 2, 0, 2, 80);
	prefresh(blockpad, blockpad_pos, 0, 3, 0, blockpad_lines + 3, 80);
}

int main()
{
	int ch;
	
	initscr();			/* Start curses mode 		*/
	raw();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */
	start_color();
	use_default_colors();
	init_pair(10, COLOR_WHITE, COLOR_RED);

	db_connect();
   
	blockpad = newpad(1000, 80);
	hdrpad = newpad(1, 80);

	refresh_screen();

	if (db_is_connected())
	{
		rels = db_fetch_relations(&nrels);
		display_relations(hdrpad, blockpad, rels, nrels);
	}
	wclear(stdscr);

	for (;;)
	{
		refresh_screen();
		ch = getch();

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
						display_block(hdrpad, blockpad, block, displayed_block);
						displayed_block = 0;
					}
				}
				break;

			case KEY_LEFT:
				if (displayed_block != InvalidBlockNumber)
				{
					displayed_block = InvalidBlockNumber;
					blockpad_pos = blockpad_pos_save;
					display_relations(hdrpad, blockpad, rels, nrels);
					mvwchgat(blockpad, selected_rel, 0, 40, A_REVERSE, 0, NULL);
				}
				break;

			case 'g':				/* goto block */
				if (displayed_block != InvalidBlockNumber)
				{
					char str[11];
					BlockNumber blkno;
					char *endptr;

					werase(hdrpad);
					mvwprintw(hdrpad, 0, 0, "Goto block: ");
					refresh_screen();
					echo();
					getnstr(str, sizeof(str) - 1);
					noecho();

					blkno = strtoul(str, &endptr, 10);
					if (*endptr != '\0')
					{
						werase(hdrpad);
						mvwprintw(hdrpad, 0, 0, "Invalid block number");
					}
					else
					{
						block = db_fetch_block(rels[selected_rel].relname, "main", blkno);
						if (block)
						{
							displayed_block = blkno;
							blockpad_pos_save = blockpad_pos;
							blockpad_pos = 0;
							display_block(hdrpad, blockpad, block, displayed_block);
							displayed_block = 0;
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
