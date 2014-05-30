#include "main.h"

#include "storage/bufpage.h"
#include "storage/itemptr.h"

#include <ncurses.h>

/***** stuff copy-pasted from btree.h *****/

/* There's room for a 16-bit vacuum cycle ID in BTPageOpaqueData */
typedef uint16 BTCycleId;
#define MAX_BT_CYCLE_ID		0xFF7F

typedef struct BTPageOpaqueData
{
	BlockNumber btpo_prev;		/* left sibling, or P_NONE if leftmost */
	BlockNumber btpo_next;		/* right sibling, or P_NONE if rightmost */
	union
	{
		uint32		level;		/* tree level --- zero for leaf pages */
		TransactionId xact;		/* next transaction ID, if deleted */
	}			btpo;
	uint16		btpo_flags;		/* flag bits, see below */
	BTCycleId	btpo_cycleid;	/* vacuum cycle ID of latest split */
} BTPageOpaqueData;

typedef BTPageOpaqueData *BTPageOpaque;

typedef struct IndexTupleData
{
	ItemPointerData t_tid;		/* reference TID to heap tuple */

	/* ---------------
	 * t_info is laid out in the following fashion:
	 *
	 * 15th (high) bit: has nulls
	 * 14th bit: has var-width attributes
	 * 13th bit: unused
	 * 12-0 bit: size of tuple
	 * ---------------
	 */

	unsigned short t_info;		/* various info about tuple */

} IndexTupleData;				/* MORE DATA FOLLOWS AT END OF STRUCT */

typedef IndexTupleData *IndexTuple;


#define IndexInfoFindDataOffset(t_info) \
( \
	(!((t_info) & INDEX_NULL_MASK)) ? \
	( \
		(Size)MAXALIGN(sizeof(IndexTupleData)) \
	) \
	: \
	( \
		(Size)MAXALIGN(sizeof(IndexTupleData) + sizeof(IndexAttributeBitMapData)) \
	) \
)
#define INDEX_NULL_MASK 0x8000

typedef struct IndexAttributeBitMapData
{
	bits8		bits[(INDEX_MAX_KEYS + 8 - 1) / 8];
}	IndexAttributeBitMapData;

/*****  *****/


static void display_heap_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno);
static void display_btree_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno);
static void display_raw_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno);

void
display_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno)
{
	int			special_size;

	special_size = BLCKSZ - ((PageHeader) (block))->pd_special;

	if (special_size == 0)
		display_heap_block(hdrw, w, block, blkno);
	else if (special_size == sizeof(BTPageOpaqueData) && ((BTPageOpaque) PageGetSpecialPointer(block))->btpo_cycleid <= MAX_BT_CYCLE_ID)
		display_btree_block(hdrw, w, block, blkno);
	else
		display_raw_block(hdrw, w, block, blkno);
}

int itemy, itemx;
int rawdatay, rawdatax;

static void
colorrawbytes(WINDOW *w, int start, int len, attr_t attr, short color)
{
	int y, x;
	int pos;

	for (pos = start; pos < start + len; pos++)
	{
		y = rawdatay + pos / 16;
		x = 6 + (pos % 16) * 2;
		if (pos % 16 >= 8)
			x++;
		mvwchgat(w, y, x, 2, attr, color, NULL);
	}
}


static void
display_heap_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno)
{
	PageHeader phdr = (PageHeader) block;
	OffsetNumber off,
				maxoff;
	XLogRecPtr lsn;
	int			i;

	werase(hdrw);
	werase(w);
	wmove(w, 0, 0);

	mvwprintw(hdrw, 0, 0, "Displaying heap block %u", blkno);

	lsn = PageGetLSN(block);
	wprintw(w, "pd_lsn: %X/%08X\n", (uint32) (lsn >> 32), (uint32) lsn);
	wprintw(w, "pd_checksum: %04X\n", phdr->pd_checksum);
	wprintw(w, "pd_flags: %04X\n", phdr->pd_flags);
	wprintw(w, "pd_lower: %d\n", phdr->pd_lower);
	wprintw(w, "pd_upper: %d\n", phdr->pd_upper);
	wprintw(w, "pd_special: %d\n", phdr->pd_special);
	wprintw(w, "pd_pagesize_version: %04X\n", phdr->pd_pagesize_version);
	wprintw(w, "pd_prune_xid: %u\n", phdr->pd_prune_xid);

	maxoff = PageGetMaxOffsetNumber(block);
	wprintw(w, "\nItems (%d):\n", maxoff);
	getyx(w, itemy, itemx);
	for (off = 1; off <= maxoff; off++)
	{
		ItemId iid = PageGetItemId(block, off);
		char *flags;

		switch (iid->lp_flags)
		{
			case LP_UNUSED:
				flags = "UNUSED";
				break;
			case LP_NORMAL:
				flags = "NORMAL";
				break;
			case LP_REDIRECT:
				flags = "REDIRECT";
				break;
			case LP_DEAD:
				flags = "DEAD";
				break;
			default:
				flags = "???";
				break;
		}

		wprintw(w, "%u: off: %u, flags: %s, len: %u\n",
				off, iid->lp_off, flags, iid->lp_len);
	}

	wprintw(w, "\nRaw:\n");
	getyx(w, rawdatay, rawdatax);
	for (i = 0; i < BLCKSZ; i+=16)
	{
		unsigned char *p = (unsigned char *) &block[i];
		wprintw(w,
				"%04X  %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
				i,
				p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
				p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	}

	/* Color the raw data */

	/* page header */
	colorrawbytes(w, 0, SizeOfPageHeaderData, A_NORMAL, COLOR_PAIR(10));
}

static void
display_btree_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno)
{
	PageHeader phdr = (PageHeader) block;
	OffsetNumber off,
				maxoff;
	XLogRecPtr lsn;
	int			i;
	BTPageOpaque btpageop;

	werase(hdrw);
	werase(w);
	wmove(w, 0, 0);

	mvwprintw(hdrw, 0, 0, "Displaying B-tree block %u", blkno);

	lsn = PageGetLSN(block);
	btpageop = (BTPageOpaque) PageGetSpecialPointer(block);

	wprintw(w, "pd_lsn: %X/%08X\n", (uint32) (lsn >> 32), (uint32) lsn);
	wprintw(w, "pd_checksum: %04X\n", phdr->pd_checksum);
	wprintw(w, "pd_flags: %04X\n", phdr->pd_flags);
	wprintw(w, "pd_lower: %d\n", phdr->pd_lower);
	wprintw(w, "pd_upper: %d\n", phdr->pd_upper);
	wprintw(w, "pd_special: %d\n", phdr->pd_special);
	wprintw(w, "pd_pagesize_version: %04X\n", phdr->pd_pagesize_version);
	wprintw(w, "pd_prune_xid: %u\n", phdr->pd_prune_xid);

	wprintw(w, "\n");
	wprintw(w, "btpo_prev: %u\n", btpageop->btpo_prev);
	wprintw(w, "btpo_next: %u\n", btpageop->btpo_next);
	wprintw(w, "btpo_level: %u\n", btpageop->btpo.level);
	wprintw(w, "btpo_flags: %u\n", btpageop->btpo_flags);
	wprintw(w, "btpo_cycleid: %u\n", btpageop->btpo_cycleid);

	maxoff = PageGetMaxOffsetNumber(block);
	wprintw(w, "\nItems (%d):\n", maxoff);
	getyx(w, itemy, itemx);
	for (off = 1; off <= maxoff; off++)
	{
		ItemId iid = PageGetItemId(block, off);
		char *flags;

		switch (iid->lp_flags)
		{
			case LP_UNUSED:
				flags = "UNUSED";
				break;
			case LP_NORMAL:
				flags = "NORMAL";
				break;
			case LP_REDIRECT:
				flags = "REDIRECT";
				break;
			case LP_DEAD:
				flags = "DEAD";
				break;
			default:
				flags = "???";
				break;
		}

		wprintw(w, "%u: off: %u, flags: %s, len: %u",
				off, iid->lp_off, flags, iid->lp_len);

		if (ItemIdHasStorage(iid))
		{
			IndexTuple itup = (IndexTuple) PageGetItem(block, iid);
			ItemPointerData t_tid = itup->t_tid;
			int32		key;

			wprintw(w, "    (%u, %u)",
					(BlockNumber) ((t_tid.ip_blkid.bi_hi << 16) | (uint16) (t_tid.ip_blkid.bi_lo)),
					t_tid.ip_posid);

			/* key value, assuming it's an int4 */
			key = *(int32 *)(((char *) itup) + IndexInfoFindDataOffset(itup->t_info));
			wprintw(w, "  %d", key);

			wprintw(w, "\n");
		}
	}

	wprintw(w, "\nRaw:\n");
	getyx(w, rawdatay, rawdatax);
	for (i = 0; i < BLCKSZ; i+=16)
	{
		unsigned char *p = (unsigned char *) &block[i];
		wprintw(w,
				"%04X  %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
				i,
				p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
				p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	}

	/* Color the raw data */

	/* page header */
	colorrawbytes(w, 0, SizeOfPageHeaderData, A_NORMAL, COLOR_PAIR(10));
}


static void
display_raw_block(WINDOW *hdrw, WINDOW *w, char *block, BlockNumber blkno)
{
	int			i;

	werase(hdrw);
	werase(w);
 
	wmove(w, 0, 0);

	for (i = 0; i < BLCKSZ; i += 16)
	{
		unsigned char *p = (unsigned char *) &block[i];
		wprintw(w,
				"%04X  %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
				i,
				p[0], p[1], p[2], p[3],  p[4], p[5], p[6], p[7],
				p[8], p[9], p[10], p[11],  p[12], p[13], p[14], p[15]);
	}
}



void
display_relations(WINDOW *hdrw, WINDOW *w, relation_info *rels, int nrels)
{
	int			i;

	werase(hdrw);
	werase(w);

	/* print header */
	mvwprintw(hdrw, 0, 0, "name");
	mvwprintw(hdrw, 0, 30, "size");
	mvwprintw(hdrw, 0, 40, "relkind");

	for (i = 0; i < nrels; i++)
	{
		relation_info *rel = &rels[i];
		mvwprintw(w, i, 0, "%.29s", rel->relname);
		mvwprintw(w, i, 30, "%u", rel->nblocks);
		mvwprintw(w, i, 40, "%c", rel->relkind);
	}
	reporterror("relations fetched");
}


