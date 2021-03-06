#ifndef MAIN_H
#define MAIN_H

#include <ncurses.h>

#define FRONTEND 1
#include "postgres_fe.h"
#include "storage/block.h"

#include <stdarg.h>

#define reporterror(...)							\
	do {											\
		werase(errorw);								\
		mvwprintw(errorw, 0,0, __VA_ARGS__);		\
		wrefresh(errorw);							\
	} while(0);

typedef struct
{
	char		relname[NAMEDATALEN*2+1];
	BlockNumber	nblocks;
	char		relkind;
} relation_info;

WINDOW *errorw;


extern void db_connect(void);
extern bool db_is_connected(void);
extern relation_info *db_fetch_relations(int *nrels);
extern char *db_fetch_block(char *relname, char *forkname, BlockNumber blkno);


extern void display_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno);
extern void display_relations(WINDOW *hdrw, WINDOW *w, relation_info *rels, int nrels);

#endif
