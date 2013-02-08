/*

Copyright 1985, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* Modified by Stephen so keyboard rate is set using XKB extensions */

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xmu/Error.h>
#include <X11/XKBlib.h>

#define ON 1
#define OFF 0

#define SERVER_DEFAULT (-1)
#define DONT_CHANGE -2

#define ALL -1

#define XKBDDELAY_DEFAULT 660
#define XKBDRATE_DEFAULT (1000/40)

static char *progName;

static int error_status = 0;

static int is_number(char *arg, int maximum);
static void set_mouse(Display *dpy, int acc_num, int acc_denom, int threshold);
static void set_repeat(Display *dpy, int key, int auto_repeat_mode);
static void usage(char *fmt, ...) _X_NORETURN;
static int local_xerror(Display *dpy, XErrorEvent *rep);

static void xkbset_repeatrate(Display *dpy, int delay, int rate);

int
main(int argc, char *argv[])
{
    register char *arg;
    register int i;
    int acc_num, acc_denom, threshold;

    int key, auto_repeat_mode;

    char *disp = NULL;
    Display *dpy;

    int miscpresent = 0;
    int xkbpresent = 0;

    int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;

    progName = argv[0];
    if (argc < 2) {
	usage(NULL, NULL);	       /* replace with window interface */
    }

    dpy = XOpenDisplay(disp);    /*  Open display and check for success */
    if (dpy == NULL) {
	fprintf(stderr, "%s:  unable to open display \"%s\"\n",
		argv[0], XDisplayName(disp));
	exit(EXIT_FAILURE);
    }
    XSetErrorHandler(local_xerror);
    for (i = 1; i < argc;) {
	arg = argv[i++];
/*  Set pointer (mouse) settings:  Acceleration and Threshold. */
	if (strcmp(arg, "m") == 0 || strcmp(arg, "mouse") == 0) {
	    acc_num = SERVER_DEFAULT;		/* restore server defaults */
	    acc_denom = SERVER_DEFAULT;
	    threshold = SERVER_DEFAULT;
	    if (i >= argc) {
		set_mouse(dpy, acc_num, acc_denom, threshold);
		break;
	    }
	    arg = argv[i];
	    if (strcmp(arg, "default") == 0) {
		i++;
	    } else if (*arg >= '0' && *arg <= '9') {
		acc_denom = 1;
		sscanf(arg, "%d/%d", &acc_num, &acc_denom);
		i++;
		if (i >= argc) {
		    set_mouse(dpy, acc_num, acc_denom, threshold);
		    break;
		}
		arg = argv[i];
		if (*arg >= '0' && *arg <= '9') {
		    threshold = atoi(arg); /* Set threshold as user specified. */
		    i++;
		}
	    }
	    set_mouse(dpy, acc_num, acc_denom, threshold);
	} else if (strcmp(arg, "-r") == 0) {		/* Turn off one or
							   all autorepeats */
	    auto_repeat_mode = OFF;
	    key = ALL;		       			/* None specified */
	    arg = argv[i];
	    if (i < argc)
		if (is_number(arg, 255)) {
		    key = atoi(arg);
		    i++;
		}
	    set_repeat(dpy, key, auto_repeat_mode);
	} else if (strcmp(arg, "r") == 0) {		/* Turn on one or
							   all autorepeats */
	    auto_repeat_mode = ON;
	    key = ALL;		       			/* None specified */
	    arg = argv[i];
	    if (i < argc) {
		if (strcmp(arg, "on") == 0) {
		    i++;
		} else if (strcmp(arg, "off") == 0) {	/*  ...except in
							    this case */
		    auto_repeat_mode = OFF;
		    i++;
		}
		else if (strcmp(arg, "rate") == 0) {	/*  ...or this one. */
		    int delay = 0, rate = 0;

		    if (XkbQueryExtension(dpy, &xkbopcode, &xkbevent,
					  &xkberror, &xkbmajor, &xkbminor)) {
			delay = XKBDDELAY_DEFAULT;
			rate = XKBDRATE_DEFAULT;
			xkbpresent = 1;
		    }
		    if (!xkbpresent && !miscpresent)
			fprintf(stderr,
				"server does not have extension for \"r rate\" option\n");
		    i++;
		    arg = argv[i];
		    if (i < argc) {
			if (is_number(arg, 10000) && atoi(arg) > 0) {
			    delay = atoi(arg);
			    i++;
			    arg = argv[i];
			    if (i < argc) {
				if (is_number(arg, 255) && atoi(arg) > 0) {
				    rate = atoi(arg);
				    i++;
				}
			    }
			}
		    }
		    if (xkbpresent) {
			xkbset_repeatrate(dpy, delay, 1000 / rate);
		    }
		}
		else if (is_number(arg, 255)) {
		    key = atoi(arg);
		    i++;
		}
	    }
	    set_repeat(dpy, key, auto_repeat_mode);
	} else
	    usage("unknown option %s", arg);
    }

    XCloseDisplay(dpy);

    exit(error_status);		       /*  Done.  We can go home now.  */
}

static int
is_number(char *arg, int maximum)
{
    register char *p;

    if (arg[0] == '-' && arg[1] == '1' && arg[2] == '\0')
	return (1);
    for (p = arg; isdigit(*p); p++) ;
    if (*p || atoi(arg) > maximum)
	return (0);
    return (1);
}

static void
set_mouse(Display *dpy, int acc_num, int acc_denom, int threshold)
{
    int do_accel = True, do_threshold = True;

    if (acc_num == DONT_CHANGE)	       /* what an incredible crock... */
	do_accel = False;
    if (threshold == DONT_CHANGE)
	do_threshold = False;
    if (acc_num < 0)		       /* shouldn't happen */
	acc_num = SERVER_DEFAULT;
    if (acc_denom <= 0)		       /* prevent divide by zero */
	acc_denom = SERVER_DEFAULT;
    if (threshold < 0)
	threshold = SERVER_DEFAULT;
    XChangePointerControl(dpy, do_accel, do_threshold, acc_num,
			  acc_denom, threshold);
    return;
}

static void
set_repeat(Display *dpy, int key, int auto_repeat_mode)
{
    XKeyboardControl values;

    values.auto_repeat_mode = auto_repeat_mode;
    if (key != ALL) {
	values.key = key;
	XChangeKeyboardControl(dpy, KBKey | KBAutoRepeatMode, &values);
    } else {
	XChangeKeyboardControl(dpy, KBAutoRepeatMode, &values);
    }
    return;
}

static void
xkbset_repeatrate(Display *dpy, int delay, int interval)
{
    XkbDescPtr xkb = XkbAllocKeyboard();

    if (!xkb)
	return;
    XkbGetControls(dpy, XkbRepeatKeysMask, xkb);
    xkb->ctrls->repeat_delay = delay;
    xkb->ctrls->repeat_interval = interval;
    XkbSetControls(dpy, XkbRepeatKeysMask, xkb);
    XkbFreeKeyboard(xkb, 0, True);
}

/*  This is the usage function */

static void
usage(char *fmt, ...)
{
    va_list ap;

    if (fmt) {
	fprintf(stderr, "%s:  ", progName);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n\n");
    }

    fprintf(stderr, "usage:  %s option ...\n", progName);
    fprintf(stderr, "    To set mouse acceleration and threshold:\n");
    fprintf(stderr, "\t m [acc_mult[/acc_div] [thr]]    m default\n");
    fprintf(stderr, "    To set pixel colors:\n");
    fprintf(stderr, "\t-r [keycode]        r off\n");
    fprintf(stderr, "\t r [keycode]        r on\n");
    fprintf(stderr, "\t r rate [delay [rate]]\n");
    exit(EXIT_SUCCESS);
}

static int
local_xerror(Display *dpy, XErrorEvent *rep)
{
    if (rep->request_code == X_SetFontPath && rep->error_code == BadValue) {
	fprintf(stderr,
		"%s:  bad font path element (#%ld), possible causes are:\n",
		progName, rep->resourceid);
	fprintf(stderr,
		"    Directory does not exist or has wrong permissions\n");
	fprintf(stderr, "    Directory missing fonts.dir\n");
	fprintf(stderr, "    Incorrect font server address or syntax\n");
    } else if (rep->request_code == X_StoreColors) {
	switch (rep->error_code) {
	  case BadAccess:
	    fprintf(stderr,
		    "%s:  pixel not allocated read/write\n", progName);
	    break;
	  case BadValue:
	    fprintf(stderr,
		    "%s:  cannot store in pixel 0x%lx, invalid pixel number\n",
		    progName, rep->resourceid);
	    break;
	  default:
	    XmuPrintDefaultErrorMessage(dpy, rep, stderr);
	}
    } else
	XmuPrintDefaultErrorMessage(dpy, rep, stderr);

    error_status = -1;

    return (0);
}
