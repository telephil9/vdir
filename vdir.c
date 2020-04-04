#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <plumb.h>
#include <bio.h>
#include "icons.h"

extern void alert(const char *title, const char *message);

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
Rectangle newdirr;
Rectangle newfiler;
Rectangle viewr;
Rectangle scrollr;
Rectangle scrposr;
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
Image *scrollbg;
Image *scrollfg;
int lineh;
int nlines;
int offset;

void
showerrstr(void)
{
	char errbuf[ERRMAX];

	errstr(errbuf, ERRMAX-1);
	alert("Error", errbuf);
}

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
	char newpath[256];
	char *sep;
	int n;

	if(dir == nil)
		n = snprint(newpath, sizeof path, home);
	else if(dir[0] == '/')
		n = snprint(newpath, sizeof newpath, dir);
	else{
		sep = strlen(path)==1 ? "" : "/";
		n = snprint(newpath, sizeof newpath, "%s%s%s", path, sep, dir);
	}
	if(access(newpath, 0)<0){
		alert("Error", "Directory does not exist");
		return;
	}
	strncpy(path, newpath, n);
	loaddirs();
}

void
mkdir(char *name)
{
	char *p;
	int fd;

	p = smprint("%s/%s", path, name);
	if(access(p, 0)>=0){
		alert("Error", "Directory already exists");
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
		alert("Error", "File already exists");
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
drawbutton(Point *p, Image *i)
{
	Rectangle r;

	p->x += Toolpadding;
	r = Rect(p->x, p->y, p->x+16, p->y+16);
	draw(screen, r, i, nil, ZP);
	p->x += 16+Toolpadding;
	return r;
}

void
drawdir(Point p, Dir d)
{
	char buf[255], *t;
	Image *img;

	t = mdate(d);
	snprint(buf, sizeof buf, "%12lld  %s", d.length, t);
	free(t);
	img = (d.qid.type&QTDIR) ? folder : file;
	p.y -= Padding;
	draw(screen, Rect(p.x, p.y+Padding, p.x+12, p.y+Padding+12), img, nil, ZP);
	p.x += 12+4+Padding;
	p.y += Padding;
	string(screen, p, viewfg, ZP, font, d.name);
	p.x = viewr.max.x - stringwidth(font, buf) - 3*Padding - Toolpadding;
	string(screen, p, viewfg, ZP, font, buf);
}

void
redraw(void)
{
	Point p;
	int i, h, y;

	draw(screen, screen->r, display->white, nil, ZP);
	p = addpt(screen->r.min, Pt(0, Toolpadding));
	draw(screen, toolr, toolbg, nil, ZP);
	line(screen, Pt(toolr.min.x, toolr.max.y), toolr.max, 0, 0, 0, toolfg, ZP);
	homer = drawbutton(&p, ihome);
	cdr = drawbutton(&p, icd);
	upr = drawbutton(&p, iup);
	p.x += Toolpadding;
	p.y = toolr.min.y + (Toolpadding+16+Toolpadding-font->height)/2;
	string(screen, p, toolfg, ZP, font, path);
	p.x = screen->r.max.x - 2*(Toolpadding+16+Toolpadding);
	p.y = screen->r.min.y + Toolpadding;
	newdirr = drawbutton(&p, inewfolder);
	newfiler = drawbutton(&p, inewfile);
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
		drawdir(p, dirs[offset+i]);
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

Point
cept(const char *text)
{
	Point p;

	p = screen->r.min;
	p.x += (Dx(screen->r)-stringwidth(font, text)-4)/2;
	p.y += (Dy(screen->r)-font->height-4)/2;
	return p;
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
			m.xy = cept("Go to directory");
			if(eenter("Go to directory", buf, sizeof buf, &m)>0){
				cd(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, newdirr)){
			m.xy = cept("Create directory");
			if(eenter("Create directory", buf, sizeof buf, &m)>0){
				mkdir(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, newfiler)){
			m.xy = cept("Create file");
			if(eenter("Create file", buf, sizeof buf, &m)>0){
				touch(buf);
				redraw();
			}
		}else if(ptinrect(m.xy, viewr)){
			n = (m.xy.y-viewr.min.y)/lineh;
			if(offset+n>=ndirs)
				return;
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
