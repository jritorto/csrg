#ifndef lint
static char sccsid[] = "@(#)wwredrawwin.c	3.9 05/23/84";
#endif

#include "ww.h"

wwredrawwin1(w, row1, row2, offset)
register struct ww *w;
int row1, row2, offset;
{
	int row;
	register col;
	register char *smap;
	register union ww_char *buf;
	register char *win;
	register union ww_char *ns;
	int nchanged;

	for (row = row1; row < row2; row++) {
		col = w->ww_i.l;
		ns = wwns[row];
		smap = &wwsmap[row][col];
		buf = w->ww_buf[row + offset];
		win = w->ww_win[row];
		nchanged = 0;
		for (; col < w->ww_i.r; col++)
			if (*smap++ == w->ww_index) {
				nchanged++;
				ns[col].c_w =
					buf[col].c_w ^ win[col] << WWC_MSHIFT;
			}
		if (nchanged > 4)
			wwtouched[row] |= WWU_MAJOR|WWU_TOUCHED;
		else if (nchanged > 0)
			wwtouched[row] |= WWU_TOUCHED;
	}
}
