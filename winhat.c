#include <u.h>
#include <stdio.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

#include "winhat.h"


#define COLUMN_COUNT 6
#define COLUMN_WIDTH 12
#define LINE_WIDTH 72

enum {
	W_CURRENT = -1,
	W_NOTCURRENT = -2,
	W_FOCUS_CHANGED = 1,
	W_RECT_CHANGED = 2
};

enum {
	LEFT = 0,
	TOP = 1,
	RIGHT = 2,
	BOTTOM = 3
};


void
threadmain(int argc, char* argv[])
{
	ARGBEGIN{
		default:
			usage();
	} ARGEND;
	
	int winid = 0;
	if (argc != 1 || (argc == 1 && (winid = atoi(argv[0])) == 0))
		usage();

    Mousectl* mctl;
    Keyboardctl* kctl;
    if((mctl = initmouse(nil, screen)) == nil)
        sysfatal("%s: %r", argv0);
    if((kctl = initkeyboard(nil)) == nil)
        sysfatal("%s: %r", argv0);
	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("%s: %r", argv0);

    Rune kbd;
    Mouse mouse;
    int resize[2];
    enum{ MOUSE, RESIZE, KEYBD, NONE };
    Alt alts[4] = {
        {mctl->c, &mouse, CHANRCV},
        {mctl->resizec, &resize, CHANRCV},
        {kctl->c, &kbd, CHANRCV},
        {nil, nil, CHANNOBLK}
    };

	char path[70];
	int len = wctl_path(winid, path);
	if(len <= 0)
		sysfatal("Weird path error?");
	int rect[4] = {0,0,0,0};
	int focus = W_NOTCURRENT;

    draw_own_window(focus);

	int live = 1;
    while(live == 1){
        switch(alt(alts)){
            case RESIZE: 
                draw_own_window(focus);
                break;
            case MOUSE:  
				if(mouse.buttons == 2){
					kill_window(path);
				}else if(mouse.buttons == 4){
					hide_window(path);
					hide_window("/mnt/wsys/wctl");
				}
                break;
            case KEYBD:  
                if(kbd == 'q')
					live = 0;
				break;
            case NONE:  
                monitor_window(path, rect, &focus);
				sleep(100);
                break;
            default:
                sysfatal("This... shouldn't happen. No event.");
                break;
        }
    }
	closekeyboard(kctl);
	closemouse(mctl);
	threadexitsall(nil);
}

void
usage(void)
{
	fprint(2, "usage: %s winid\n", argv0);
	exits("usage");
}

/* You will comply! YOU WILL COMPLY! */
void
monitor_window(const char* path, int rect[4], int* focus)
{
	int status = check_window(path, rect, focus);
    if(status == -2) {
		kill_window("/mnt/wsys/wctl");
		exits(0);
	}
    
    if(status & W_FOCUS_CHANGED) {
		draw_own_window(*focus);
    }
    if(status & W_RECT_CHANGED) {
        if(resize_own_window(rect) == -1) {
            rect[0] = 0; rect[1] = 0;
            rect[2] = 0; rect[3] = 0;
        }else
            focus_window(path);
    }
}

/* Checks if the window's position or focus has changed
   1: updated, int* params changed
   0: unchanged
  -1: reading blocked
  -2: window deleted */
int
check_window(const char* path, int rect[4], int* focus)
{
	int status = wctl_accessibility(path, 4);	
	if(status != 0)
		return status;
	int wctl_fd = open(path, OREAD);
	if(wctl_fd < 0)
		return -1;

	char line[LINE_WIDTH];
	read_line(wctl_fd, line);
	close(wctl_fd);

	for(int i = 0; i < COLUMN_COUNT; i++) {
		int val = analyze_column(line, i);
		if(i < 4 && rect[i] != val) {
			rect[i] = val;
			status |= W_RECT_CHANGED;
		}else if(i == 4 && val != *focus) {
			*focus = val;
			status |= W_FOCUS_CHANGED;
		}
	}
	return status;
}

void
focus_window(const char* path)
{
	int wctl_fd = open(path, OWRITE);
	if(wctl_fd >= 0) {
		fprint(wctl_fd, "current\n");
		close(wctl_fd);
	}
}

void
kill_window(const char* path)
{
	int wctl_fd = open(path, OWRITE);
	if(wctl_fd >= 0) {
		fprint(wctl_fd, "delete\n");
		close(wctl_fd);
	}
}

void
hide_window(const char* path)
{
	int wctl_fd = open(path, OWRITE);
	if(wctl_fd >= 0) {
		fprint(wctl_fd, "hide\n");
		close(wctl_fd);
	}
}

int
resize_own_window(int rect[4])
{
	int status = wctl_accessibility("/mnt/wsys/wctl", 2);
	if(status != 0)
		return status;
	int wctl_fd = open("/mnt/wsys/wctl", OWRITE);
	if(wctl_fd <= 0)
		return -1;
	
	fprint(wctl_fd, "current\n");
	fprint(wctl_fd, "resize -r %d %d %d %d\n",
		rect[LEFT], rect[TOP] - 20, rect[RIGHT], rect[TOP]+20);
	close(wctl_fd);
	return 0;
}

void
draw_own_window(int focus)
{
    if(getwindow(display, Refnone) < 0)
        sysfatal("%s: %r", argv0);

	int bg_color = DGreygreen;
	if (focus == W_NOTCURRENT)
		bg_color = DPalegreygreen;

	Image* bg = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1, bg_color);

	if(bg == nil)
		sysfatal("Couldn't make an Image-- you doing alright, friend?");

	draw(screen, screen->r, bg, nil, ZP);

	freeimage(bg);
	flushimage(display, Refnone);
}

/* Checks wctl status
   0: exists, & readable & writable
  -1: wctl mode doesn't work 
  -2: doesn't exist */
int
wctl_accessibility(const char* path, int mode)
{
	if(access(path, mode) == -1) {
		if(access(path, 0) == -1)
			return -2;
		return -1;
	}
	return 0;
}

int
wctl_path(int winId, char* buff)
{
	return sprintf(buff, "/mnt/wsys/wsys/%d/wctl", winId);
}

int
read_line(int wctl_fd, char* buf)
{
	long n = read(wctl_fd, buf, LINE_WIDTH);
	if(n < LINE_WIDTH)
		return -1;
	return 0;
}

/* Returns int column as int, text column as W_*CURRENT */
int
analyze_column(char* buf, int column) {
	if(column > (COLUMN_COUNT - 1) || column < 0)
		return -1;

	char cbuf[COLUMN_WIDTH];
	int start = column * COLUMN_WIDTH;
	strncpy(cbuf, buf + (start * sizeof(char)), COLUMN_WIDTH);
	
	int result = atoi(cbuf);
	if(result == 0)
		if(strstr(cbuf, "notcurrent") != nil)
			result = W_NOTCURRENT;
		else if(strstr(cbuf, "current") != nil)
			result = W_CURRENT;
	return result;
}
