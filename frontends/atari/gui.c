/*
 * Copyright 2010 <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Provides all the mandatory functions prefixed with gui_ for atari
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/corestrings.h"
#include "utils/nsoption.h"
#include "netsurf/browser_window.h"
#include "netsurf/layout.h"
#include "netsurf/window.h"
#include "netsurf/clipboard.h"
#include "netsurf/fetch.h"
#include "netsurf/misc.h"
#include "netsurf/netsurf.h"
#include "netsurf/content.h"
#include "netsurf/cookie_db.h"
#include "netsurf/url_db.h"
#include "netsurf/plotters.h"
#include "content/backing_store.h"

#include "atari/gemtk/gemtk.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/findfile.h"
#include "atari/schedule.h"
#include "atari/rootwin.h"
#include "atari/statusbar.h"
#include "atari/toolbar.h"
#include "atari/hotlist.h"
#include "atari/cookies.h"
#include "atari/history.h"
#include "atari/encoding.h"
#include "atari/res/netsurf.rsh"
#include "atari/plot/plot.h"
#include "atari/clipboard.h"
#include "atari/osspec.h"
#include "atari/search.h"
#include "atari/deskmenu.h"
#include "atari/download.h"
#include "atari/file.h"
#include "atari/filetype.h"
#include "atari/bitmap.h"
#include "atari/font.h"
#include "cflib.h"

static bool atari_quit = false;

struct gui_window *input_window = NULL;
struct gui_window *window_list = NULL;
void *h_gem_rsrc;
long next_poll;
bool rendering = false;
GRECT desk_area;

/* Comandline / Options: */
int option_window_width;
int option_window_height;
int option_window_x;
int option_window_y;

/* Defaults to option_homepage_url, commandline options overwrites that value */
const char *option_homepage_url;

/* path to choices file: */
char options[PATH_MAX];

EVMULT_IN aes_event_in = {
    .emi_flags = MU_MESAG | MU_TIMER | MU_KEYBD | MU_BUTTON | MU_M1,
    .emi_bclicks = 258,
    .emi_bmask = 3,
    .emi_bstate = 0,
    .emi_m1leave = MO_ENTER,
    .emi_m1 = {0,0,0,0},
    .emi_m2leave = 0,
    .emi_m2 = {0,0,0,0},
    .emi_tlow = 0,
    .emi_thigh = 0
};
EVMULT_OUT aes_event_out;
short aes_msg_out[8];

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy);
static nserror gui_window_set_url(struct gui_window *w, nsurl *url);

/**
 * Core atari event processing.
 */
static void atari_poll(void)
{

    struct gui_window *tmp;
    short mx, my, dummy;

    aes_event_in.emi_tlow = schedule_run();

    if(rendering){
	aes_event_in.emi_tlow = nsoption_int(atari_gui_poll_timeout);
    }

    if(aes_event_in.emi_tlow < 0) {
	aes_event_in.emi_tlow = 10000;
	printf("long poll!\n");
    }

    if(input_window && input_window->root->redraw_slots.areas_used > 0) {
	window_process_redraws(input_window->root);
    }


    graf_mkstate(&mx, &my, &dummy, &dummy);
    aes_event_in.emi_m1.g_x = mx;
    aes_event_in.emi_m1.g_y = my;
    evnt_multi_fast(&aes_event_in, aes_msg_out, &aes_event_out);
    if(gemtk_wm_dispatch_event(&aes_event_in, &aes_event_out, aes_msg_out) == 0) {
	if( (aes_event_out.emo_events & MU_MESAG) != 0 ) {
	    NSLOG(netsurf, INFO, "WM: %d\n", aes_msg_out[0]);
	    switch(aes_msg_out[0]) {

	    case MN_SELECTED:
		NSLOG(netsurf, INFO, "Menu Item: %d\n", aes_msg_out[4]);
		deskmenu_dispatch_item(aes_msg_out[3], aes_msg_out[4]);
		break;

	    case AP_TERM:
		atari_quit = true;
		break;
	    default:
		break;
	    }
	}
	if((aes_event_out.emo_events & MU_KEYBD) != 0) {
	    uint16_t nkc = gem_to_norm( (short)aes_event_out.emo_kmeta,
					(short)aes_event_out.emo_kreturn);
	    deskmenu_dispatch_keypress(aes_event_out.emo_kreturn,
				       aes_event_out.emo_kmeta, nkc);
	}
    }

    tmp = window_list;
    while(tmp){
	if(tmp->root->redraw_slots.areas_used > 0){
	    window_process_redraws(tmp->root);
	}
	tmp = tmp->next;
    }

    /** @todo implement generic treeview redraw function. */
    /** @todo rename hl to atari_hotlist or create getter for it... */

    atari_treeview_flush_redraws();
}

/**
 * Create and open a gui window for a browsing context.
 *
 * \param bw		bw to create gui_window for
 * \param existing	an existing gui_window, may be NULL
 * \param flags		flags for gui window creation
 * \return gui window, or NULL on error
 *
 * If GW_CREATE_CLONE flag is set existing is non-NULL.
 *
 * The created gui_window must include a reference to the browser
 * window passed in the bw param.
 */
static struct gui_window *
gui_window_create(struct browser_window *bw,
		  struct gui_window *existing,
		  gui_window_create_flags flags)
{
    struct gui_window *gw=NULL;
    NSLOG(netsurf, INFO, "gw: %p, BW: %p, existing %p, flags: %d\n", gw, bw,
          existing, (int)flags);

    gw = calloc(1, sizeof(struct gui_window));
    if (gw == NULL)
	return NULL;

    NSLOG(netsurf, INFO, "new window: %p, bw: %p\n", gw, bw);
    window_create(gw, bw, existing, WIDGET_STATUSBAR|WIDGET_TOOLBAR|WIDGET_RESIZE\
		  |WIDGET_SCROLL);
    if (gw->root->win) {
	GRECT pos = {
	    option_window_x, option_window_y,
	    option_window_width, option_window_height
	};
	gui_window_set_url(gw, corestring_nsurl_about_blank);
	gui_window_set_pointer(gw, BROWSER_POINTER_DEFAULT);
	gui_set_input_gui_window(gw);
	window_open(gw->root, gw, pos);
    }

    /* add the window to the window list: */
    if( window_list == NULL ) {
	window_list = gw;
	gw->next = NULL;
	gw->prev = NULL;
    } else {
	struct gui_window * tmp = window_list;
	while( tmp->next != NULL ) {
	    tmp = tmp->next;
	}
	tmp->next = gw;
	gw->prev = tmp;
	gw->next = NULL;
    }

    /* Loose focus: */
    window_set_focus(gw->root, WIDGET_NONE, NULL );

    /* trigger on-focus event (select all text): */
    window_set_focus(gw->root, URL_WIDGET, NULL);

    /* delete selection: */
    toolbar_key_input(gw->root->toolbar, NK_DEL);

    return( gw );

}

/**
 * Destroy previously created gui window
 *
 * \param gw The gui window to destroy.
 */
void gui_window_destroy(struct gui_window *gw)
{
    if (gw == NULL)
	return;

    NSLOG(netsurf, INFO, "%s\n", __FUNCTION__);

    if (input_window == gw) {
	gui_set_input_gui_window(NULL);
    }

    nsatari_search_session_destroy(gw->search);
    free(gw->browser);
    free(gw->status);
    free(gw->title);
    free(gw->url);

    /* unlink the window: */
    if(gw->prev != NULL ) {
	gw->prev->next = gw->next;
    } else {
	window_list = gw->next;
    }
    if( gw->next != NULL ) {
	gw->next->prev = gw->prev;
    }

    window_unref_gui_window(gw->root, gw);

    free(gw);
    gw = NULL;

    if(input_window == NULL) {
	gw = window_list;
	while( gw != NULL ) {
	    if(gw->root) {
		gui_set_input_gui_window(gw);
		break;
	    }
	    gw = gw->next;
	}
    }
}

/**
 * Find the current dimensions of a atari browser window content area.
 *
 * \param gw The gui window to measure content area of.
 * \param width receives width of window
 * \param height receives height of window
 * \return NSERROR_OK on sucess and width and height updated
 *          else error code.
 */
static nserror
gui_window_get_dimensions(struct gui_window *gw, int *width, int *height)
{
    GRECT rect;
    window_get_grect(gw->root, BROWSER_AREA_CONTENT, &rect);
    *width = rect.g_w;
    *height = rect.g_h;

    return NSERROR_OK;
}

/**
 * Set the title of a window.
 *
 * \param  gw	  window to update
 * \param  title  new window title
 */
static void gui_window_set_title(struct gui_window *gw, const char *title)
{
    if (gw == NULL)
	return;

    if (gw->root) {

	int l;
	char * conv;
	l = strlen(title)+1;
	if (utf8_to_local_encoding(title, l-1, &conv) == NSERROR_OK ) {
	    l = MIN((uint32_t)atari_sysinfo.aes_max_win_title_len, strlen(conv));
	    if(gw->title == NULL)
		gw->title = malloc(l);
	    else
		gw->title = realloc(gw->title, l);

	    strncpy(gw->title, conv, l);
	    free( conv );
	} else {
	    l = MIN((size_t)atari_sysinfo.aes_max_win_title_len, strlen(title));
	    if(gw->title == NULL)
		gw->title = malloc(l);
	    else
		gw->title = realloc(gw->title, l);
	    strncpy(gw->title, title, l);
	}
	gw->title[l] = 0;
	if(input_window == gw)
	    window_set_title(gw->root, gw->title);
    }
}

/* exported interface documented in atari/gui.h */
void atari_window_set_status(struct gui_window *w, const char *text)
{
    int l;
    if (w == NULL || text == NULL)
	return;

    assert(w->root);

    l = strlen(text)+1;
    if(w->status == NULL)
	w->status = malloc(l);
    else
	w->status = realloc(w->status, l);

    strncpy(w->status, text, l);
    w->status[l] = 0;

    if(input_window == w)
	window_set_stauts(w->root, (char*)text);
}


/**
 * Invalidates an area of an atari browser window
 *
 * \param gw gui_window
 * \param rect area to redraw or NULL for the entire window area
 * \return NSERROR_OK on success or appropriate error code
 */
static nserror
atari_window_invalidate_area(struct gui_window *gw,
			     const struct rect *rect)
{
    GRECT area;

    if (gw == NULL) {
	return NSERROR_BAD_PARAMETER;
    }

    window_get_grect(gw->root, BROWSER_AREA_CONTENT, &area);

    if (rect != NULL) {
	    struct gemtk_wm_scroll_info_s *slid;

	    slid = gemtk_wm_get_scroll_info(gw->root->win);

	    area.g_x += rect->x0 - (slid->x_pos * slid->x_unit_px);
	    area.g_y += rect->y0 - (slid->y_pos * slid->y_unit_px);
	    area.g_w = rect->x1 - rect->x0;
	    area.g_h = rect->y1 - rect->y0;
    }

    //dbg_grect("update box", &area);
    window_schedule_redraw_grect(gw->root, &area);

    return NSERROR_OK;
}

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
    if (w == NULL)
	return false;

    window_get_scroll(w->root, sx, sy);

    return( true );
}

/**
 * Set the scroll position of a atari browser window.
 *
 * Scrolls the viewport to ensure the specified rectangle of the
 *   content is shown. The atari implementation scrolls the contents so
 *   the specified point in the content is at the top of the viewport.
 *
 * \param gw gui window to scroll
 * \param rect The rectangle to ensure is shown.
 * \return NSERROR_OK on success or apropriate error code.
 */
static nserror
gui_window_set_scroll(struct gui_window *gw, const struct rect *rect)
{
    if ((gw == NULL) ||
	(gw->browser->bw == NULL) ||
	(!browser_window_has_content(gw->browser->bw))) {
	return NSERROR_BAD_PARAMETER;
    }

    NSLOG(netsurf, INFO, "scroll (gui_window: %p) %d, %d\n", gw, rect->x0,
          rect->y0);
    window_scroll_by(gw->root, rect->x0, rect->y0);

    return NSERROR_OK;
}

/**
 * Update the extent of the inside of a browser window to that of the
 * current content.
 *
 * It seems this method is called when content size got adjusted, so
 * that we can adjust scroll info. We also have to call it when tab
 * change occurs.
 *
 * \param gw gui_window to update the extent of
 */
static void gui_window_update_extent(struct gui_window *gw)
{

    if(browser_window_has_content(gw->browser->bw)) {
	/** @todo store content size. */
	if(window_get_active_gui_window(gw->root) == gw) {
	    int width, height;
	    GRECT area;
	    browser_window_get_extents(gw->browser->bw, false, &width, &height);
	    window_set_content_size(gw->root, width, height);
	    window_update_back_forward(gw->root);
	    window_get_grect(gw->root, BROWSER_AREA_CONTENT, &area);
	    window_schedule_redraw_grect(gw->root, &area);
	}
    }
}


/**
 * set the pointer shape
 */
void gui_window_set_pointer(struct gui_window *gw, gui_pointer_shape shape)
{
    if (gw == NULL)
	return;

    switch (shape) {
    case GUI_POINTER_POINT: /* link */
	gw->cursor = &gem_cursors.hand;
	break;

    case GUI_POINTER_MENU:
	gw->cursor = &gem_cursors.menu;
	break;

    case GUI_POINTER_CARET: /* input */
	gw->cursor = &gem_cursors.ibeam;
	break;

    case GUI_POINTER_CROSS:
	gw->cursor = &gem_cursors.cross;
	break;

    case GUI_POINTER_MOVE:
	gw->cursor = &gem_cursors.sizeall;
	break;

    case GUI_POINTER_RIGHT:
    case GUI_POINTER_LEFT:
	gw->cursor = &gem_cursors.sizewe;
	break;

    case GUI_POINTER_UP:
    case GUI_POINTER_DOWN:
	gw->cursor = &gem_cursors.sizens;
	break;

    case GUI_POINTER_RU:
    case GUI_POINTER_LD:
	gw->cursor = &gem_cursors.sizenesw;
	break;

    case GUI_POINTER_RD:
    case GUI_POINTER_LU:
	gw->cursor = &gem_cursors.sizenwse;
	break;

    case GUI_POINTER_WAIT:
	gw->cursor = &gem_cursors.wait;
	break;

    case GUI_POINTER_PROGRESS:
	gw->cursor = &gem_cursors.appstarting;
	break;

    case GUI_POINTER_NO_DROP:
	gw->cursor = &gem_cursors.nodrop;
	break;

    case GUI_POINTER_NOT_ALLOWED:
	gw->cursor = &gem_cursors.deny;
	break;

    case GUI_POINTER_HELP:
	gw->cursor = &gem_cursors.help;
	break;

    default:
	gw->cursor = &gem_cursors.arrow;
	break;
    }

    if (input_window == gw) {
	gem_set_cursor(gw->cursor);
    }
}


static nserror gui_window_set_url(struct gui_window *w, nsurl *url)
{
    int l;

    if (w == NULL)
	return NSERROR_OK;

    l = strlen(nsurl_access(url))+1;

    if (w->url == NULL) {
	w->url = malloc(l);
    } else {
	w->url = realloc(w->url, l);
    }
    strncpy(w->url, nsurl_access(url), l);
    w->url[l] = 0;
    if(input_window == w->root->active_gui_window) {
        toolbar_set_url(w->root->toolbar, nsurl_access(url));
    }

    return NSERROR_OK;
}

char * gui_window_get_url(struct gui_window *gw)
{
    if (gw == NULL) {
	return(NULL);
    }
    return(gw->url);
}

char * gui_window_get_title(struct gui_window *gw)
{
    if (gw == NULL) {
	return(NULL);
    }
    return(gw->title);
}

static void throbber_advance( void * data )
{

    struct gui_window * gw = (struct gui_window *)data;

    if (gw->root == NULL)
	return;
    if (gw->root->toolbar == NULL)
	return;

    if (gw->root->toolbar->throbber.running == false)
	return;

    toolbar_throbber_progress(gw->root->toolbar);
    atari_schedule(1000, throbber_advance, gw );
}

static void gui_window_start_throbber(struct gui_window *w)
{
    if (w == NULL)
	return;

    toolbar_set_throbber_state(w->root->toolbar, true);
    atari_schedule(1000, throbber_advance, w );
    rendering = true;
}

static void gui_window_stop_throbber(struct gui_window *w)
{
    if (w == NULL)
	return;
    if (w->root->toolbar->throbber.running == false)
	return;

    atari_schedule(-1, throbber_advance, w);

    toolbar_set_throbber_state(w->root->toolbar, false);

    rendering = false;
}

/**
 * Place caret in window
 */
static void
gui_window_place_caret(struct gui_window *w, int x, int y, int height,
		       const struct rect *clip)
{
    window_place_caret(w->root, 1, x, y, height, NULL);
    w->root->caret.state |= CARET_STATE_ENABLED;
    return;
}


/**
 * clear window caret
 */
static void
gui_window_remove_caret(struct gui_window *w)
{
    if (w == NULL)
	return;

    if ((w->root->caret.state & CARET_STATE_ENABLED) != 0) {
	//printf("gw hide caret\n");
	window_place_caret(w->root, 0, -1, -1, -1, NULL);
	w->root->caret.state &= ~CARET_STATE_ENABLED;
    }
    return;
}

/**
 * Set a favicon for a gui window.
 *
 * \param g window to update.
 * \param icon handle to object to use as icon.
 */
static void
gui_window_set_icon(struct gui_window *g, struct hlcache_handle *icon)
{
    struct bitmap *bmp_icon;

    bmp_icon = (icon != NULL) ? content_get_bitmap(icon) : NULL;
    g->icon = bmp_icon;
    if(input_window == g) {
	window_set_icon(g->root, bmp_icon);
    }
}

static void gui_window_new_content(struct gui_window *w)
{
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(w->root->win);
    slid->x_pos = 0;
    slid->y_pos = 0;
    gemtk_wm_update_slider(w->root->win, GEMTK_WM_VH_SLIDER);
    atari_window_invalidate_area(w, NULL);
}


/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
    char *clip;

    *length = 0;
    *buffer = 0;

    clip = scrap_txt_read();

    if(clip == NULL) {
	return;
    } else {

	// clipboard is in atari encoding, convert it to utf8:

	size_t clip_len;
	char *utf8 = NULL;

	clip_len = strlen(clip);
	if (clip_len > 0) {
	    nserror ret;
	    ret = utf8_to_local_encoding(clip, clip_len, &utf8);
	    if (ret == NSERROR_OK && utf8 != NULL) {
		*buffer = utf8;
		*length = strlen(utf8);
	    } else {
		assert(ret == NSERROR_OK && utf8 != NULL);
	    }
	}

	free(clip);
    }
}

/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
			      nsclipboard_styles styles[], int n_styles)
{
    if (length > 0 && buffer != NULL) {

	// convert utf8 input to atari encoding:

	nserror ret;
	char *clip = NULL;

	ret = utf8_to_local_encoding(buffer,length, &clip);
	if (ret == NSERROR_OK) {
	    scrap_txt_write(clip);
	} else {
	    assert(ret == NSERROR_OK);
	}
	free(clip);
    }
}

void gui_set_input_gui_window(struct gui_window *gw)
{
    NSLOG(netsurf, INFO, "Setting input window from: %p to %p\n",
          input_window, gw);
    input_window = gw;
}

struct gui_window * gui_get_input_window(void)
{
    return(input_window);
}

static void gui_quit(void)
{
    NSLOG(netsurf, INFO, "quitting");

    struct gui_window *gw = window_list;
    struct gui_window *tmp = window_list;

    /* Destroy all remaining browser windows: */
    while (gw) {
	tmp = gw->next;
	browser_window_destroy(gw->browser->bw);
	gw = tmp;
    }

    /* destroy the treeview windows: */
    atari_global_history_destroy();
    atari_hotlist_destroy();
    atari_cookie_manager_destroy();

    /* shutdown the toolbar framework: */
    toolbar_exit();

    /* save persistent informations: */
    urldb_save_cookies(nsoption_charp(cookie_file));
    urldb_save(nsoption_charp(url_file));

    deskmenu_destroy();
    gemtk_wm_exit();

    rsrc_free();

    NSLOG(netsurf, INFO, "Shutting down plotter");
    plot_finalise();
    NSLOG(netsurf, INFO, "done");
}

/**
 * Process commandline parameters.
 */
static bool
process_cmdline(int argc, char** argv)
{
    int opt;
    bool set_default_dimensions = true;

    NSLOG(netsurf, INFO, "argc %d, argv %p", argc, argv);

    if ((nsoption_int(window_width) != 0) && (nsoption_int(window_height) != 0)) {

	option_window_width = nsoption_int(window_width);
	option_window_height = nsoption_int(window_height);
	option_window_x = nsoption_int(window_x);
	option_window_y = nsoption_int(window_y);

	if (option_window_width <= desk_area.g_w
	    && option_window_height < desk_area.g_h) {
	    set_default_dimensions = false;
	}
    }

    if (set_default_dimensions) {
	if( sys_type() == SYS_TOS ) {
	    /* on single tasking OS, start as fulled window: */
	    option_window_width = desk_area.g_w;
	    option_window_height = desk_area.g_h;
	    option_window_x = desk_area.g_w/2-(option_window_width/2);
	    option_window_y = (desk_area.g_h/2)-(option_window_height/2);
	} else {
	    option_window_width = 600;
	    option_window_height = 360;
	    option_window_x = 10;
	    option_window_y = 30;
	}
    }

    if (nsoption_charp(homepage_url) != NULL)
	option_homepage_url = nsoption_charp(homepage_url);
    else
	option_homepage_url = NETSURF_HOMEPAGE;

    while((opt = getopt(argc, argv, "w:h:")) != -1) {
	switch (opt) {
	case 'w':
	    option_window_width = atoi(optarg);
	    break;

	case 'h':
	    option_window_height = atoi(optarg);
	    break;

	default:
	    fprintf(stderr,
		    "Usage: %s [w,h,v] url\n",
		    argv[0]);
	    return false;
	}
    }

    if (optind < argc) {
	option_homepage_url = argv[optind];
    }
    return true;
}

static inline void create_cursor(int flags, short mode, void * form,
				 MFORM_EX * m)
{
    m->flags = flags;
    m->number = mode;
    if( flags & MFORM_EX_FLAG_USERFORM ) {
	m->number = mode;
	m->tree = (OBJECT*)form;
    }
}

static nsurl *gui_get_resource_url(const char *path)
{
    char buf[PATH_MAX];
    nsurl *url = NULL;

    atari_find_resource((char*)&buf, path, path);

    netsurf_path_to_nsurl(buf, &url);

    return url;
}

/**
 * Set option defaults for atari frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
    /* Set defaults for absent option strings */
    nsoption_setnull_charp(cookie_file, strdup("cookies"));
    if (nsoption_charp(cookie_file) == NULL) {
	NSLOG(netsurf, INFO, "Failed initialising string options");
	return NSERROR_BAD_PARAMETER;
    }
    return NSERROR_OK;
}

static char *theapp = (char*)"NetSurf";

/**
 * Initialise atari gui.
 */
static void gui_init(int argc, char** argv)
{
    char buf[PATH_MAX];
    OBJECT * cursors;

    atari_find_resource(buf, "netsurf.rsc", "./res/netsurf.rsc");
    NSLOG(netsurf, INFO, "Using RSC file: %s ", (char *)&buf);
    if (rsrc_load(buf)==0) {

	char msg[1024];

	snprintf(msg, 1024, "Unable to open GEM Resource file (%s)!", buf);
	die(msg);
    }

    wind_get_grect(0, WF_WORKXYWH, &desk_area);

    create_cursor(0, POINT_HAND, NULL, &gem_cursors.hand );
    create_cursor(0, TEXT_CRSR,  NULL, &gem_cursors.ibeam );
    create_cursor(0, THIN_CROSS, NULL, &gem_cursors.cross);
    create_cursor(0, BUSY_BEE, NULL, &gem_cursors.wait);
    create_cursor(0, ARROW, NULL, &gem_cursors.arrow);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizeall);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenesw);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenwse);
    cursors = gemtk_obj_get_tree(CURSOR);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_APPSTART,
		  cursors, &gem_cursors.appstarting);
    gem_set_cursor( &gem_cursors.appstarting );
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_SIZEWE,
		  cursors, &gem_cursors.sizewe);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_SIZENS,
		  cursors, &gem_cursors.sizens);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_NODROP,
		  cursors, &gem_cursors.nodrop);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_DENY,
		  cursors, &gem_cursors.deny);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_MENU,
		  cursors, &gem_cursors.menu);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_HELP,
		  cursors, &gem_cursors.help);

    NSLOG(netsurf, INFO, "Enabling core select menu");
    nsoption_set_bool(core_select_menu, true);

    NSLOG(netsurf, INFO, "Loading url.db from: %s", nsoption_charp(url_file));
    if( strlen(nsoption_charp(url_file)) ) {
	urldb_load(nsoption_charp(url_file));
    }

    NSLOG(netsurf, INFO, "Loading cookies from: %s",
          nsoption_charp(cookie_file));
    if( strlen(nsoption_charp(cookie_file)) ) {
	urldb_load_cookies(nsoption_charp(cookie_file));
    }

    if (process_cmdline(argc,argv) != true)
	die("unable to process command line.\n");

    NSLOG(netsurf, INFO, "Initializing NKC...");
    nkc_init();

    NSLOG(netsurf, INFO, "Initializing plotters...");
    struct redraw_context ctx = {
	    .interactive = true,
	    .background_images = true,
	    .plot = &atari_plotters
    };
    plot_init(&ctx, nsoption_charp(atari_font_driver));

    aes_event_in.emi_m1leave = MO_LEAVE;
    aes_event_in.emi_m1.g_w = 1;
    aes_event_in.emi_m1.g_h = 1;
    //next_poll = clock() + (CLOCKS_PER_SEC>>3);

    deskmenu_init();
    menu_register( -1, theapp);
    if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
	menu_register( _AESapid, (char*)"  NetSurf ");
    }
    gemtk_wm_init();

    /* Initialize the specific treeview windows: */
    atari_global_history_init();
    atari_hotlist_init();
    atari_cookie_manager_init();

    /* Initialize the toolbar framework: */
    toolbar_init();
}


/**
 * process miscellaneous window events
 *
 * \param gw The window receiving the event.
 * \param event The event code.
 * \return NSERROR_OK when processed ok
 */
static nserror
gui_window_event(struct gui_window *gw, enum gui_window_event event)
{
	switch (event) {
	case GW_EVENT_UPDATE_EXTENT:
		gui_window_update_extent(gw);
		break;

	case GW_EVENT_REMOVE_CARET:
		gui_window_remove_caret(gw);
		break;

	case GW_EVENT_NEW_CONTENT:
		gui_window_new_content(gw);
		break;

	case GW_EVENT_START_THROBBER:
		gui_window_start_throbber(gw);
		break;

	case GW_EVENT_STOP_THROBBER:
		gui_window_stop_throbber(gw);
		break;

	default:
		break;
	}
	return NSERROR_OK;
}


static struct gui_window_table atari_window_table = {
    .create = gui_window_create,
    .destroy = gui_window_destroy,
    .invalidate = atari_window_invalidate_area,
    .get_scroll = gui_window_get_scroll,
    .set_scroll = gui_window_set_scroll,
    .get_dimensions = gui_window_get_dimensions,
    .event = gui_window_event,

    .set_title = gui_window_set_title,
    .set_url = gui_window_set_url,
    .set_icon = gui_window_set_icon,
    .set_status = atari_window_set_status,
    .set_pointer = gui_window_set_pointer,
    .place_caret = gui_window_place_caret,
};

static struct gui_clipboard_table atari_clipboard_table = {
    .get = gui_get_clipboard,
    .set = gui_set_clipboard,
};

static struct gui_fetch_table atari_fetch_table = {
    .filetype = fetch_filetype,

    .get_resource_url = gui_get_resource_url,
};

static struct gui_misc_table atari_misc_table = {
    .schedule = atari_schedule,

    .quit = gui_quit,
};

/* #define WITH_DBG_LOGFILE 1 */
/**
 * Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int main(int argc, char** argv)
{
    char messages[PATH_MAX];
    char store[PATH_MAX];
    const char *addr;
    char * file_url = NULL;
    struct stat stat_buf;
    nsurl *url;
    nserror ret;

    struct netsurf_table atari_table = {
	.misc = &atari_misc_table,
	.window = &atari_window_table,
	.clipboard = &atari_clipboard_table,
	.download = atari_download_table,
	.fetch = &atari_fetch_table,
	.file = atari_file_table,
	.utf8 = atari_utf8_table,
	.search = atari_search_table,
	.llcache = filesystem_llcache_table,
	.bitmap = atari_bitmap_table,
	.layout = atari_layout_table
    };

    ret = netsurf_register(&atari_table);
    if (ret != NSERROR_OK) {
	die("NetSurf operation table failed registration");
    }

    /** @todo logging file descriptor update belongs in a nslog_init callback */
    setbuf(stderr, NULL);
    setbuf(stdout, NULL);
#ifdef WITH_DBG_LOGFILE
    freopen("stdout.log", "a+", stdout);
    freopen("stderr.log", "a+", stderr);
#endif

    graf_mouse(BUSY_BEE, NULL);

    init_app(NULL);

    init_os_info();

    atari_find_resource((char*)&messages, "messages", "res/messages");
    atari_find_resource((char*)&options, "Choices", "Choices");
    atari_find_resource((char*)&store, "cache", "res/cache");

    /* initialise logging - not fatal if it fails but not much we can
     * do about it
     */
    nslog_init(NULL, &argc, argv);

    /* user options setup */
    ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
    if (ret != NSERROR_OK) {
	die("Options failed to initialise");
    }
    nsoption_read(options, NULL);
    nsoption_commandline(&argc, argv, NULL);

    ret = messages_add_from_file(messages);

    /* common initialisation */
    NSLOG(netsurf, INFO, "Initialising core...");
    ret = netsurf_init(store);
    if (ret != NSERROR_OK) {
	die("NetSurf failed to initialise");
    }

    NSLOG(netsurf, INFO, "Initializing GUI...");
    gui_init(argc, argv);

    graf_mouse( ARROW , NULL);

    NSLOG(netsurf, INFO, "Creating initial browser window...");
    addr = option_homepage_url;
    if (strncmp(addr, "file://", 7) && strncmp(addr, "http://", 7)) {
	if (stat(addr, &stat_buf) == 0) {
	    file_url = local_file_to_url(addr);
	    addr = file_url;
	}
    }

    /* create an initial browser window */
    ret = nsurl_create(addr, &url);
    if (ret == NSERROR_OK) {
	ret = browser_window_create(BW_CREATE_HISTORY,
				    url,
				    NULL,
				    NULL,
				    NULL);
	nsurl_unref(url);
    }
    if (ret != NSERROR_OK) {
	atari_warn_user(messages_get_errorcode(ret), 0);
    } else {
	NSLOG(netsurf, INFO, "Entering Atari event mainloop...");
	while (!atari_quit) {
	    atari_poll();
	}
    }

    netsurf_exit();

    free(file_url);

#ifdef WITH_DBG_LOGFILE
    fclose(stdout);
    fclose(stderr);
#endif
    NSLOG(netsurf, INFO, "exit_gem");

    /* finalise logging */
    nslog_finalise();

    exit_gem();

    return 0;
}
