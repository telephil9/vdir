#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <plumb.h>
#include <bio.h>

enum
{
	Toolpadding = 4,
	Padding = 1,
	Scrollwidth = 14,
	Slowscroll = 10,
};

char *home;
char path[256];
Dir* dirs;
long ndirs;
Rectangle toolr;
Rectangle homer;
Rectangle upr;
Rectangle cdr;
Rectangle viewr;
Rectangle scrollr;
Rectangle scrposr;
Image *folder;
Image *file;
Image *toolbg;
Image *toolfg;
Image *viewbg;
Image *viewfg;
Image *scrollbg;
Image *scrollfg;
int lineh;
int nlines;
int offset;


void
readhome(void)
{
	Biobuf *bp;
	
	bp = Bopen("/env/home", OREAD);
	home = Brdstr(bp, 0, 0);
	Bterm(bp);
}

int
dircmp(Dir *a, Dir *b)
{
	if(a->qid.type==b->qid.type)
		return strcmp(a->name, b->name);
	if(a->qid.type&QTDIR)
		return -1;
	return 1;
}

void
loaddirs(void)
{
	int fd;

	fd = open(path, OREAD);
	if(fd<0)
		sysfatal("open: %r");
	ndirs = dirreadall(fd, &dirs);
	qsort(dirs, ndirs, sizeof *dirs, (int(*)(void*,void*))dircmp);
	offset = 0;
	close(fd);
}

void
up(void)
{
	int i, n;

	n = strlen(path);
	if(n==1)
		return;
	for(i = n-1; path[i]; i--){
		if(path[i]=='/'){
			path[i]=0;
			break;
		}
	}
	if(strlen(path)==0)
		sprint(path, "/");
	loaddirs();
}

void
cd(char *dir)
{
	char *sep;

	if(dir == nil)
		sprint(path, home);
	else if(dir[0] == '/')
		snprint(path, sizeof path, dir);
	else{
		sep = strlen(path)==1 ? "" : "/";
		snprint(path, sizeof path, "%s%s%s", path, sep, dir);
	}
	loaddirs();
}

void
plumbfile(char *path, char *name)
{
	int fd;
	char *f;

	f = smprint("%s/%s", path, name);
	fd = plumbopen("send", OWRITE|OCEXEC);
	if(fd<0)
		return;
	plumbsendtext(fd, "vdir", nil, nil, f);
	close(fd);
	free(f);
}	

void
initcolors(void)
{
	toolbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xEFEFEFFF);
	toolfg = display->black;
	viewbg = display->white;
	viewfg = display->black;
	scrollbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x999999FF);
	scrollfg = display->white;
}

void
initimages(void)
{
	folder = allocimage(display, Rect(0, 0, 12, 12), screen->chan, 0, DWhite);
	line(folder, Pt( 0,  1), Pt( 5,  1), 0, 0, 0, display->black, ZP);
	line(folder, Pt( 5,  1), Pt( 7,  4), 0, 0, 0, display->black, ZP);
	line(folder, Pt( 7,  4), Pt(10,  4), 0, 0, 0, display->black, ZP);
	line(folder, Pt(10,  4), Pt(10, 10), 0, 0, 0, display->black, ZP);
	line(folder, Pt(10, 10), Pt( 0, 10), 0, 0, 0, display->black, ZP);
	line(folder, Pt( 0, 10), Pt( 0,  1), 0, 0, 0, display->black, ZP);	
	file = allocimage(display, Rect(0, 0, 12, 12), screen->chan, 0, DWhite);
	line(file, Pt( 1,  1), Pt( 7,  1), 0, 0, 0, display->black, ZP);
	line(file, Pt( 7,  1), Pt( 7,  4), 0, 0, 0, display->black, ZP);
	line(file, Pt( 7,  4), Pt(10,  4), 0, 0, 0, display->black, ZP);
	line(file, Pt(10,  4), Pt(10, 11), 0, 0, 0, display->black, ZP);
	line(file, Pt(10, 11), Pt( 1, 11), 0, 0, 0, display->black, ZP);
	line(file, Pt( 1, 11), Pt( 1,  1), 0, 0, 0, display->black, ZP);	
	line(file, Pt( 7,  1), Pt(10,  4), 0, 0, 0, display->black, ZP);	
}

char*
mdate(Dir d)
{
	char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
			   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	Tm *tm;

	tm = localtime(d.mtime);
	return smprint("%s %02d %02d:%02d", months[tm->mon], tm->mday, tm->hour, tm->min);
}

Rectangle
drawbutton(Point *p, char *s)
{
	Rectangle r;
	int w;

	string(screen, *p, toolfg, ZP, font, s);
	w = stringwidth(font, s);
	r = Rect(p->x, p->y, p->x+w, p->y+lineh);
	p->x += w+Toolpadding;
	line(screen, Pt(p->x, toolr.min.y), Pt(p->x, toolr.max.y-1), 0, 0, 0, scrollbg, ZP);
	p->x += Toolpadding;
	return r;
}

void
redraw(void)
{
	Point p;
	int i, h, y;
	char buf[255], *t;
	Dir d;
	Image *img;

	draw(screen, screen->r, display->white, nil, ZP);
	p = addpt(screen->r.min, Pt(Toolpadding, Toolpadding));
	draw(screen, toolr, toolbg, nil, ZP);
	line(screen, Pt(toolr.min.x, toolr.max.y), toolr.max, 0, 0, 0, toolfg, ZP);
	homer = drawbutton(&p, "Home");
	upr = drawbutton(&p, "Up");
	cdr = drawbutton(&p, "Cd");
	string(screen, p, toolfg, ZP, font, path);
	draw(screen, scrollr, scrollbg, nil, ZP);
	if(ndirs>0){
		h = ((double)nlines/ndirs)*Dy(scrollr);
		y = ((double)offset/ndirs)*Dy(scrollr);
		scrposr = Rect(scrollr.min.x, scrollr.min.y+y, scrollr.max.x-1, scrollr.min.y+y+h);
	}else
		scrposr = Rect(scrollr.min.x, scrollr.min.y, scrollr.max.x-1, scrollr.max.y);
	draw(screen, scrposr, display->white, nil, ZP);
	p = addpt(viewr.min, Pt(Toolpadding, Toolpadding));
	for(i = 0; i<nlines && offset+i<ndirs; i++){
		d = dirs[offset+i];
		t = mdate(d);
		snprint(buf, sizeof buf, "%-32s %12lld  %s", d.name, d.length, t);
		free(t);
		img = (d.qid.type&QTDIR) ? folder : file;
		p.y -= Padding;
		draw(screen, Rect(p.x, p.y+Padding, p.x+12, p.y+Padding+12), img, nil, ZP);
		p.x += 12+4+Padding;
		p.y += Padding;
		string(screen, p, viewfg, ZP, font, buf);
		p.x = viewr.min.x+Toolpadding;
		p.y += lineh;
	}
}

void
scrollup(int off)
{
	if(offset == 0)
		return;
	offset -= off;
	if(offset < 0)
		offset = 0;
	redraw();
}

void
scrolldown(int off)
{
	if(offset+nlines > ndirs)
		return;
	offset += off;
	redraw();
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone)<0)
		sysfatal("cannot reattach: %r");
	lineh = Padding+font->height+Padding;
	toolr = screen->r;
	toolr.max.y = toolr.min.y+lineh+Toolpadding;
	scrollr = screen->r;
	scrollr.min.y = toolr.max.y+1;
	scrollr.max.x = scrollr.min.x + Scrollwidth;
	scrollr = insetrect(scrollr, 1);
	viewr = screen->r;
	viewr.min.x += Scrollwidth;
	viewr.min.y = toolr.max.y+1;
	nlines = Dy(viewr)/lineh;
	redraw();
}

void
evtkey(Rune k)
{
	switch(k){
	case 'q':
	case Kdel:
		exits(nil);
		break;
	case Kpgup:
		scrollup(nlines);
		break;
	case Kpgdown:
		scrolldown(nlines);
		break;
	case Khome:
		cd(nil);
		redraw();
		break;
	case Kup:
		up();
		redraw();
		break;
	}
}

void
evtmouse(Mouse m)
{
	int n;
	Dir d;
	char buf[256] = {0};

	if(m.buttons&4){
		if(ptinrect(m.xy, homer)){
			cd(nil);
			redraw();
		}else if(ptinrect(m.xy, upr)){
			up();
			redraw();
		}else if(ptinrect(m.xy, cdr)){
			if(eenter("Directory", buf, sizeof buf, &m)>0){
				cd(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, viewr)){
			n = (m.xy.y-viewr.min.y)/lineh;
			d = dirs[offset+n];
			if(d.qid.type && QTDIR){
				cd(d.name);
				redraw();
			}else
				plumbfile(path, d.name);
		}
	}else if(m.buttons&8)
		scrollup(Slowscroll);
	else if(m.buttons&16)
		scrolldown(Slowscroll);
}


void
main(int argc, char *argv[])
{
	Event e;

	offset = 0;
	if(argc==2)
		snprint(path, sizeof path, argv[1]);
	else
		getwd(path, sizeof path);
	readhome();
	loaddirs();
	if(initdraw(nil, nil, "vdir")<0)
		sysfatal("initdraw: %r");
	initcolors();
	initimages();
	einit(Emouse|Ekeyboard);
	eresized(0);
	for(;;){
		switch(event(&e)){
		case Ekeyboard:
			evtkey(e.kbdc);
			break;
		case Emouse:
			evtmouse(e.mouse);
			break;
		}
	}
}
