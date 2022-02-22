#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include "theme.h"

Image*
ereadcol(char *s)
{
	Image *i;
	char *e;
	ulong c;

	c = strtoul(s, &e, 16);
	if(e == nil || e == s)
		return nil;
	c = (c << 8) | 0xff;
	i = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, c);
	if(i == nil)
		sysfatal("allocimage: %r");
	return i;
}

Theme*
loadtheme(void)
{
	Theme *theme;
	Biobuf *bp;
	char *s;

	if(access("/dev/theme", AREAD) < 0)
		return 0;
	bp = Bopen("/dev/theme", OREAD);
	if(bp == nil)
		return 0;
	theme = malloc(sizeof *theme);
	if(theme == nil){
		Bterm(bp);
		return nil;
	}
	for(;;){
		s = Brdstr(bp, '\n', 1);
		if(s == nil)
			break;
		if(strncmp(s, "back", 4) == 0)
			theme->back = ereadcol(s+5);
		else if(strncmp(s, "high", 4) == 0)
			theme->high = ereadcol(s+5);
		else if(strncmp(s, "border", 6) == 0)
			theme->border = ereadcol(s+7);
		else if(strncmp(s, "text", 4) == 0)
			theme->text = ereadcol(s+5);
		else if(strncmp(s, "htext", 5) == 0)
			theme->htext = ereadcol(s+6);
		else if(strncmp(s, "title", 5) == 0)
			theme->title = ereadcol(s+6);
		else if(strncmp(s, "ltitle", 6) == 0)
			theme->ltitle = ereadcol(s+7);
		else if(strncmp(s, "hold", 4) == 0)
			theme->hold = ereadcol(s+5);
		else if(strncmp(s, "lhold", 5) == 0)
			theme->lhold = ereadcol(s+6);
		else if(strncmp(s, "palehold", 8) == 0)
			theme->palehold = ereadcol(s+9);
		else if(strncmp(s, "paletext", 8) == 0)
			theme->paletext = ereadcol(s+9);
		else if(strncmp(s, "size", 4) == 0)
			theme->size = ereadcol(s+5);
		else if(strncmp(s, "menuback", 8) == 0)
			theme->menuback = ereadcol(s+9);
		else if(strncmp(s, "menuhigh", 8) == 0)
			theme->menuhigh = ereadcol(s+9);
		else if(strncmp(s, "menubord", 8) == 0)
			theme->menubord = ereadcol(s+9);
		else if(strncmp(s, "menutext", 8) == 0)
			theme->menutext = ereadcol(s+9);
		else if(strncmp(s, "menuhtext", 5) == 0)
			theme->menuhtext = ereadcol(s+6);
		free(s);
	}
	Bterm(bp);
	return theme;
}

