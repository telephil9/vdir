typedef struct Theme Theme;

struct Theme
{
	Image *back;
	Image *high;
	Image *border;
	Image *text;
	Image *htext;
	Image *title;
	Image *ltitle;
	Image *hold;
	Image *lhold;
	Image *palehold;
	Image *paletext;
	Image *size;
	Image *menubar;
	Image *menuback;
	Image *menuhigh;
	Image *menubord;
	Image *menutext;
	Image *menuhtext;
};

Theme* loadtheme(void);
