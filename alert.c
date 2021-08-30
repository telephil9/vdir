#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

enum { Padding = 12, };
char Titlefont[] = "/lib/font/bit/dejavusansbd/unicode.14.font";
char Messagefont[] = "/lib/font/bit/dejavusans/unicode.14.font";

int
max(int a, int b)
{
	return a>b ? a : b;
}

void
alert(const char *title, const char *message, Mousectl *mctl, Keyboardctl *kctl)
{
	Alt alts[3];
	Rectangle r, sc;
	Point o, p;
	Image *b, *save, *bg, *fg;
	Font *tf, *mf;
	int done, h, w, tw, mw;
	Mouse m;
	Rune k;

	alts[0].op = CHANRCV;
	alts[0].c  = mctl->c;
	alts[0].v  = &m;
	alts[1].op = CHANRCV;
	alts[1].c  = kctl->c;
	alts[1].v  = &k;
	alts[2].op = CHANEND;
	alts[2].c  = nil;
	alts[2].v  = nil;
	while(nbrecv(kctl->c, nil)==1)
		;
	bg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xf8d7daff);
	fg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x721c24ff);
	tf = openfont(display, Titlefont);
	if(tf==nil)
		sysfatal("openfont: %r");
	mf = openfont(display, Messagefont);
	if(mf==nil)
		sysfatal("openfont: %r");
	done = 0;
	save = nil;
	h = Padding+tf->height+mf->height+Padding;
	tw = stringwidth(tf, title);
	mw = stringwidth(mf, message);
	w = Padding+max(tw, mw)+Padding;
	b = screen;
	sc = b->clipr;
	replclipr(b, 0, b->r);
	while(!done){
		o = addpt(screen->r.min, Pt((Dx(screen->r)-w)/2, (Dy(screen->r)-h)/2));
		r = Rect(o.x, o.y, o.x+w, o.y+h);
		if(save==nil){
			save = allocimage(display, r, b->chan, 0, DNofill);
			if(save==nil)
				break;
			draw(save, r, b, nil, r.min);
		}
		draw(b, r, bg, nil, ZP);
		border(b, r, 2, fg, ZP);
		p = addpt(o, Pt(Padding, Padding));
		string(b, p, fg, ZP, tf, title);
		p.y += tf->height;
		string(b, p, fg, ZP, mf, message);
		flushimage(display, 1);
		if(b!=screen || !eqrect(screen->clipr, sc)){
			freeimage(save);
			save = nil;
		}
		b = screen;
		sc = b->clipr;
		replclipr(b, 0, b->r);
		switch(alt(alts)){
		default:
			continue;
			break;
		case 1:
			done = (k=='\n' || k==Kesc);
			break;
		case 0:
			done = m.buttons&1 && ptinrect(m.xy, r);
			break;
		}
		if(save){
			draw(b, save->r, save, nil, save->r.min);
			freeimage(save);
			save = nil;
		}
			
	}
	replclipr(b, 0, sc);
	freeimage(bg);
	freeimage(fg);
	freefont(tf);
	freefont(mf);
}
