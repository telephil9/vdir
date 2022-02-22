#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <plumb.h>
#include <bio.h>
#include "icons.h"
#include "theme.h"

extern void alert(const char *title, const char *message, Mousectl *mctl, Keyboardctl *kctl);
void redraw(void);

enum
{
	Toolpadding = 4,
	Padding = 1,
	Scrollwidth = 14,
	Slowscroll = 10,
};

enum
{
	Emouse,
	Eresize,
	Ekeyboard,
};

const char ellipsis[] = "…";

char *home;
char path[256];
Dir* dirs;
long ndirs;
Mousectl *mctl;
Keyboardctl *kctl;
Rectangle toolr;
Rectangle homer;
Rectangle upr;
Rectangle cdr;
Rectangle newdirr;
Rectangle newfiler;
Rectangle viewr;
Rectangle scrollr;
Rectangle scrposr;
Rectangle pathr;
Image *folder;
Image *file;
Image *ihome;
Image *icd;
Image *iup;
Image *inewfile;
Image *inewfolder;
Image *toolbg;
Image *toolfg;
Image *viewbg;
Image *viewfg;
Image *selbg;
Image *selfg;
Image *scrollbg;
Image *scrollfg;
int sizew;
int lineh;
int nlines;
int offset;
int plumbfd;
int scrolling;
int oldbuttons;
int lastn;

void
showerrstr(void)
{
	char errbuf[ERRMAX];

	errstr(errbuf, ERRMAX-1);
	alert("Error", errbuf, mctl, kctl);
}

void
readhome(void)
{
	Biobuf *bp;
	
	bp = Bopen("/env/home", OREAD);
	home = Brdstr(bp, 0, 0);
	Bterm(bp);
}

char*
abspath(char *wd, char *p)
{
	char *s;

	if(p[0]=='/')
		s = cleanname(p);
	else{
		s = smprint("%s/%s", wd, p);
		cleanname(s);
	}
	return s;
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
	int fd, i, m;

	fd = open(path, OREAD);
	if(fd<0){
		showerrstr();
		return;
	}
	if(dirs!=nil)
		free(dirs);
	ndirs = dirreadall(fd, &dirs);
	qsort(dirs, ndirs, sizeof *dirs, (int(*)(void*,void*))dircmp);
	offset = 0;
	close(fd);
	m = 1;
	for(i=0; i < ndirs; i++){
		if(dirs[i].length>m)
			m=dirs[i].length;
	}
	sizew = 1+1+log(m)/log(10);
}

void
up(void)
{
	snprint(path, sizeof path, abspath(path, ".."));
	loaddirs();
}

void
cd(char *dir)
{
	char newpath[256] = {0};

	if(dir == nil)
		snprint(newpath, sizeof path, home);
	else if(dir[0] == '/')
		snprint(newpath, sizeof newpath, dir);
	else
		snprint(newpath, sizeof newpath, "%s/%s", path, dir);
	if(access(newpath, 0)<0)
		alert("Error", "Directory does not exist", mctl, kctl);
	else
		snprint(path, sizeof path, abspath(path, newpath));
	loaddirs();
}

void
mkdir(char *name)
{
	char *p;
	int fd;

	p = smprint("%s/%s", path, name);
	if(access(p, 0)>=0){
		alert("Error", "Directory already exists", mctl, kctl);
		goto cleanup;
	}
	fd = create(p, OREAD, DMDIR|0755);
	if(fd<0){
		showerrstr();
		goto cleanup;
	}
	close(fd);
	loaddirs();
cleanup:
	free(p);
}

void
touch(char *name)
{
	char *p;
	int fd;

	p = smprint("%s/%s", path, name);
	if(access(p, 0)>=0){
		alert("Error", "File already exists", mctl, kctl);
		goto cleanup;
	}
	fd = create(p, OREAD, 0644);
	if(fd<0){
		showerrstr();
		goto cleanup;
	}
	close(fd);
	loaddirs();
cleanup:
	free(p);
}

int
plumbfile(char *path, char *name)
{
	char *f;
	int e;

	f = smprint("%s/%s", path, name);
	e = access(f, 0)==0;
	if(e)
		plumbsendtext(plumbfd, "vdir", nil, path, name);
	else{
		alert("Error", "File does not exist anymore", mctl, kctl);
		loaddirs();
		redraw();
	}
	free(f);
	return e;
}

void
initcolors(void)
{
	Theme *theme;

	theme = loadtheme();
	if(theme == nil){
		toolbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xEFEFEFFF);
		toolfg = display->black;
		viewbg = display->white;
		viewfg = display->black;
		selbg  = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xEFEFEFFF);
		selfg  = display->black;
		scrollbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x999999FF);
		scrollfg = display->white;
	}else{
		toolbg = theme->back;
		toolfg = theme->text;
		viewbg = theme->back;
		viewfg = theme->text;
		selbg  = theme->border;
		selfg  = theme->text;
		scrollbg = theme->border;
		scrollfg = theme->back;
	}
}

Image*
loadicon(Rectangle r, uchar *data, int ndata)
{
	Image *i;
	int n;

	i = allocimage(display, r, RGBA32, 0, DNofill);
	if(i==nil)
		sysfatal("allocimage: %r");
	n = loadimage(i, r, data, ndata);
	if(n<0)
		sysfatal("loadimage: %r");
	return i;
}

void
initimages(void)
{
	Rectangle small = Rect(0, 0, 12, 12);
	Rectangle big   = Rect(0, 0, 16, 16);

	folder = loadicon(small, folderdata, sizeof folderdata);
	file   = loadicon(small, filedata, sizeof filedata);
	ihome  = loadicon(big, homedata, sizeof homedata);
	icd    = loadicon(big, cddata, sizeof cddata);
	iup    = loadicon(big, updata, sizeof updata);
	inewfile = loadicon(big, newfiledata, sizeof newfiledata);
	inewfolder = loadicon(big, newfolderdata, sizeof newfolderdata);
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
drawbutton(Point *p, Image *c, Image *i)
{
	Rectangle r;

	p->x += Toolpadding;
	r = Rect(p->x, p->y, p->x+16, p->y+16);
	draw(screen, r, c, i, ZP);
	p->x += 16+Toolpadding;
	return r;
}

Point
drawtext(Point p, Image *i, char* t, int n)
{
	char *s;
	Rune  rn;

	s = t;
	if(*s && (p.x+stringwidth(font, s)) > n){
		p = string(screen, p, i, ZP, font, ellipsis);
		while (*s && (p.x+stringwidth(font, s)) > n) s++;
	}
	for( ; *s; s++){
		s += chartorune(&rn, s) - 1;
		p = runestringn(screen, p, i, ZP, font, &rn, 1);
	}
	return p;
}

void
drawdir(int n, int selected)
{
	char buf[255], *t;
	Dir d;
	Image *img, *bg, *fg;
	Point p;
	Rectangle r;
	int dy;

	if(offset+n>=ndirs)
		return;
	bg = selected ? selbg : viewbg;
	fg = selected ? selfg : viewfg;
	d = dirs[offset+n];
	p = addpt(viewr.min, Pt(Toolpadding, Toolpadding));
	p.y += n*lineh;
	r = Rpt(p, addpt(p, Pt(Dx(viewr)-2*Toolpadding, lineh)));
	draw(screen, r, bg, nil, ZP);
	t = mdate(d);
	snprint(buf, sizeof buf, "%*lld  %s", sizew, d.length, t);
	free(t);
	img = (d.qid.type&QTDIR) ? folder : file;
	p.y -= Padding;
	dy = (lineh-12)/2;
	draw(screen, Rect(p.x, p.y+dy, p.x+12, p.y+dy+12), fg, img, ZP);
	p.x += 12+4+Padding;
	p.y += Padding;
	p = drawtext(p, fg, d.name, viewr.max.x - stringwidth(font, buf) - 2*Padding - Toolpadding);
	p.x = viewr.max.x - stringwidth(font, buf) - 2*Padding - Toolpadding;
	string(screen, p, fg, ZP, font, buf);
}

void
flash(int n)
{
	int i;

	for(i=0; i<5; i++){
		drawdir(n, i&1);
		sleep(50);
		flushimage(display, 1);
	}
}

void
redraw(void)
{
	Point p;
	int i, h, y;

	draw(screen, screen->r, viewbg, nil, ZP);
	p = addpt(screen->r.min, Pt(0, Toolpadding));
	draw(screen, toolr, toolbg, nil, ZP);
	line(screen, Pt(toolr.min.x, toolr.max.y), toolr.max, 0, 0, 0, toolfg, ZP);
	homer = drawbutton(&p, toolfg, ihome);
	cdr = drawbutton(&p, toolfg, icd);
	upr = drawbutton(&p, toolfg, iup);
	p.x += Toolpadding;
	p.y = toolr.min.y + (Toolpadding+16+Toolpadding-font->height)/2;
	pathr = Rect(p.x, p.y, p.x + stringwidth(font, path), p.y + font->height);
	p = drawtext(p, toolfg, path, screen->r.max.x - 2*(Toolpadding+16+Toolpadding));
	p.x = screen->r.max.x - 2*(Toolpadding+16+Toolpadding);
	p.y = screen->r.min.y + Toolpadding;
	newdirr = drawbutton(&p, toolfg, inewfolder);
	newfiler = drawbutton(&p, toolfg, inewfile);
	draw(screen, scrollr, scrollbg, nil, ZP);
	if(ndirs>0){
		h = ((double)nlines/ndirs)*Dy(scrollr);
		y = ((double)offset/ndirs)*Dy(scrollr);
		scrposr = Rect(scrollr.min.x, scrollr.min.y+y, scrollr.max.x-1, scrollr.min.y+y+h);
	}else
		scrposr = Rect(scrollr.min.x, scrollr.min.y, scrollr.max.x-1, scrollr.max.y);
	draw(screen, scrposr, scrollfg, nil, ZP);
	for(i = 0; i<nlines && offset+i<ndirs; i++){
		drawdir(i, 0);
	}
	flushimage(display, 1);
}

int
scrollclamp(int offset)
{
	if(nlines >= ndirs)
		offset = 0;
	else if(offset < 0)
		offset = 0;
	else if(offset+nlines > ndirs)
		offset = ndirs-nlines;
	return offset;
}

void
scrollup(int off)
{
	int newoff;

	newoff = scrollclamp(offset - off);
	if(newoff == offset)
		return;
	offset = newoff;
	redraw();
}

void
scrolldown(int off)
{
	int newoff;

	newoff = scrollclamp(offset + off);
	if(newoff == offset)
		return;
	offset = newoff;
	redraw();
}

void
evtresize(void)
{
	if(getwindow(display, Refnone)<0)
		sysfatal("cannot reattach: %r");
	lineh = Padding+font->height+Padding;
	toolr = screen->r;
	toolr.max.y = toolr.min.y+16+2*Toolpadding;
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
		threadexitsall(nil);
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
	case 0x20:
		plumbsendtext(plumbfd, "vdir", nil, nil, path);
		break;
	}
}

Point
cept(const char *text)
{
	Point p;

	p = screen->r.min;
	p.x += (Dx(screen->r)-stringwidth(font, text)-4)/2;
	p.y += (Dy(screen->r)-font->height-4)/2;
	return p;
}

int
indexat(Point p)
{
	int n;

	if(!ptinrect(p, viewr))
		return -1;
	n = (p.y-viewr.min.y)/lineh;
	if(offset+n>=ndirs)
		return -1;
	return n;
}

void
evtmouse(Mouse m)
{
	int n, dy;
	Dir d;
	char buf[256] = {0};

	if(oldbuttons == 0 && m.buttons != 0 && ptinrect(m.xy, scrollr))
		scrolling = 1;
	else if(m.buttons == 0)
		scrolling = 0;

	if(m.buttons&1){
		if(scrolling){
			dy = 1+nlines*((double)(m.xy.y - scrollr.min.y)/Dy(scrollr));
			scrollup(dy);
		}
	}else if(m.buttons&2){
		if(scrolling){
			if(nlines<ndirs){
				offset = scrollclamp((m.xy.y - scrollr.min.y) * ndirs/Dy(scrollr));
				redraw();
			}
		}
	}if((m.buttons&4) && oldbuttons == 0){
		if(scrolling){
			dy = 1+nlines*((double)(m.xy.y - scrollr.min.y)/Dy(scrollr));
			scrolldown(dy);
		}else if(ptinrect(m.xy, homer)){
			cd(nil);
			redraw();
		}else if(ptinrect(m.xy, upr)){
			up();
			redraw();
		}else if(ptinrect(m.xy, cdr)){
			m.xy = cept("Go to directory");
			if(enter("Go to directory", buf, sizeof buf, mctl, kctl, nil)>0){
				cd(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, pathr)){
			plumbsendtext(plumbfd, "vdir", nil, nil, path);
		}else if(ptinrect(m.xy, newdirr)){
			m.xy = cept("Create directory");
			if(enter("Create directory", buf, sizeof buf, mctl, kctl, nil)>0){
				mkdir(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, newfiler)){
			m.xy = cept("Create file");
			if(enter("Create file", buf, sizeof buf, mctl, kctl, nil)>0){
				touch(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, viewr)){
			n = indexat(m.xy);
			if(n==-1)
				return;
			d = dirs[offset+n];
			if(d.qid.type & QTDIR){
				cd(d.name);
				redraw();
			}else{
				if(plumbfile(path, d.name))
					flash(n);
			}
		}
	}else if(m.buttons&8)
		scrollup(Slowscroll);
	else if(m.buttons&16)
		scrolldown(Slowscroll);
	else{
		n = indexat(m.xy);
		if(n==-1){
			if(lastn!=-1){
				drawdir(lastn, 0);
				lastn = -1;
				flushimage(display, 1);
			}
		}else if(n!=lastn){
			if(lastn!=-1)
				drawdir(lastn, 0);
			drawdir(n, 1);
			lastn = n;
			flushimage(display, 1);
		}
	}

	oldbuttons = m.buttons;
}

void
threadmain(int argc, char *argv[])
{
	Mouse m;
	Rune k;
	Alt alts[] = {
		{ nil, &m,  CHANRCV },
		{ nil, nil, CHANRCV },	
		{ nil, &k,  CHANRCV },
		{ nil, nil, CHANEND },
	};

	offset = 0;
	scrolling = 0;
	oldbuttons = 0;
	lastn = -1;
	if(getwd(path, sizeof path) == nil)
		sysfatal("getwd: %r");
	if(argc==2 && access(argv[1], 0) >= 0)
		snprint(path, sizeof path, abspath(path, argv[1]));
	plumbfd = plumbopen("send", OWRITE|OCEXEC);
	if(plumbfd<0)
		sysfatal("plumbopen: %r");
	if(initdraw(nil, nil, "vdir")<0)
		sysfatal("initdraw: %r");
	display->locking = 0;
	mctl = initmouse(nil, screen);
	if(mctl==nil)
		sysfatal("initmouse: %r");
	kctl = initkeyboard(nil);
	if(kctl==nil)
		sysfatal("initkeyboard: %r");
	alts[Emouse].c = mctl->c;
	alts[Eresize].c = mctl->resizec;
	alts[Ekeyboard].c = kctl->c;
	readhome();
	loaddirs();
	initcolors();
	initimages();
	evtresize();
	for(;;){
		switch(alt(alts)){
		case Emouse:
			evtmouse(m);
			break;
		case Eresize:
			evtresize();
			break;
		case Ekeyboard:
			evtkey(k);
			break;
		}
	}
}
