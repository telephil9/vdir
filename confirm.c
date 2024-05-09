#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

enum { Padding = 12, };

int
confirm(const char *message, Mousectl *mctl, Keyboardctl *kctl, Image *bg, Image *fg, Image *hi)
{
	Alt alts[3];
	Rectangle r, sc;
	Point o, p;
	Image *b, *save;
	int done, rc, h, w;
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
	done = 0;
	rc = 0;
	save = nil;
	h = Padding+font->height+Padding;
	w = Padding+stringwidth(font, message)+stringwidth(font, " Yes / No")+Padding;
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
		border(b, r, 2, hi, ZP);
		p = addpt(o, Pt(Padding, Padding));
		p = string(b, p, fg, ZP, font, message);
		p = string(b, p, hi, ZP, font, " Y");
		p = string(b, p, fg, ZP, font, "es /");
		p = string(b, p, hi, ZP, font, " N");
		string(b, p, fg, ZP, font, "o");
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
			if(k=='\n' || k==Kesc || k=='n' || k=='N'){
				done = 1;
				rc = 0;
			}else if(k=='y' || k=='Y'){
				done = 1;
				rc = 1;
			}
			break;
		case 0:
			done = m.buttons&1 && ptinrect(m.xy, r);
			rc = 0;
			break;
		}
		if(save){
			draw(b, save->r, save, nil, save->r.min);
			freeimage(save);
			save = nil;
		}
			
	}
	replclipr(b, 0, sc);
	return rc;
}
