/*
 * Copyright 2008-2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_GUI_H
#define AMIGA_GUI_H

#include <stdbool.h>
#include <graphics/rastport.h>
#include <intuition/classusr.h>
#include <dos/dos.h>
#include <devices/inputevent.h>

#include "netsurf/window.h"
#include "netsurf/mouse.h"

#include "amiga/gui_menu.h"
#include "amiga/object.h"
#include "amiga/os3support.h"

#ifdef __amigaos4__
#define HOOKF(ret,func,type,ptr,msgtype) static ret func(struct Hook *hook, type ptr, msgtype msg)
#else
#define HOOKF(ret,func,type,ptr,msgtype) static ASM ret func(REG(a0, struct Hook *hook),REG(a2, type ptr), REG(a1, msgtype msg))
#endif

enum
{
    OID_MAIN = 0,
	OID_VSCROLL,
	OID_HSCROLL,
	OID_LAST, /* for compatibility */
	GID_MAIN,
	GID_TABLAYOUT,
	GID_BROWSER,
	GID_STATUS,
	GID_URL,
	GID_ICON,
	GID_STOP,
	GID_RELOAD,
	GID_HOME,
	GID_BACK,
	GID_FORWARD,
	GID_THROBBER,
	GID_SEARCH_ICON,
	GID_FAVE,
	GID_FAVE_ADD,
	GID_FAVE_RMV,
	GID_CLOSETAB,
	GID_CLOSETAB_BM,
	GID_ADDTAB,
	GID_ADDTAB_BM,
	GID_TABS,
	GID_TABS_FLAG,
	GID_USER,
	GID_PASS,
	GID_LOGIN,
	GID_CANCEL,
	GID_NEXT,
	GID_PREV,
	GID_SEARCHSTRING,
	GID_SHOWALL,
	GID_CASE,
	GID_TOOLBARLAYOUT,
	GID_HOTLIST,
	GID_HOTLISTLAYOUT,
	GID_HOTLISTSEPBAR,
	GID_HSCROLL,
	GID_HSCROLLLAYOUT,
	GID_VSCROLL,
	GID_VSCROLLLAYOUT,
	GID_LAST
};

struct find_window;
struct ami_history_local_window;
struct ami_menu_data;

#define AMI_GUI_TOOLBAR_MAX 20

struct ami_win_event_table {
	/* callback to handle events when using a shared msgport
	 *
	 * @param pointer to our window structure (must start with ami_generic_window)
	 * @return TRUE if window was destroyed during event processing
	 */
	BOOL (*event)(void *w);

	/* callback for explicit window closure
	 * some windows are implicitly closed by the browser and should set this to NULL
	*/
	void (*close)(void *w);
};

struct ami_generic_window {
	struct nsObject *node;
	const struct ami_win_event_table *tbl;
};

struct gui_window_2 {
	struct ami_generic_window w;
	struct Window *win;
	Object *restrict objects[GID_LAST];
	struct gui_window *gw; /* currently-displayed gui_window */
	bool redraw_required;
	int throbber_frame;
	struct List tab_list;
	ULONG tabs;
	ULONG next_tab;
	struct Node *last_new_tab;
	struct Hook scrollerhook;
	struct form_control *control;
	browser_mouse_state mouse_state;
	browser_mouse_state key_state;
	ULONG throbber_update_count;
	struct find_window *searchwin;
	ULONG oldh;
	ULONG oldv;
	int temp;
	bool redraw_scroll;
	bool new_content;
	struct ami_menu_data *menu_data[AMI_MENU_AREXX_MAX + 1]; /* only for GadTools menus */
	ULONG hotlist_items;
	Object *restrict hotlist_toolbar_lab[AMI_GUI_TOOLBAR_MAX];
	struct List hotlist_toolbar_list;
	struct List *web_search_list;
	Object *search_bm;
	char *restrict svbuffer;
	char *restrict status;
	char *restrict wintitle;
	char *restrict helphints[GID_LAST];
	browser_mouse_state prev_mouse_state;
	struct timeval lastclick;
	struct AppIcon *appicon; /* iconify appicon */
	struct DiskObject *dobj; /* iconify appicon */
	struct Hook favicon_hook;
	struct Hook throbber_hook;
	struct Hook *ctxmenu_hook;
	Object *restrict history_ctxmenu[2];
	Object *restrict clicktab_ctxmenu;
	gui_drag_type drag_op;
	struct IBox *ptr_lock;
	struct AppWindow *appwin;
	struct MinList *shared_pens;
	gui_pointer_shape mouse_pointer;
	struct Menu *imenu; /* Intuition menu */
	bool closed; /* Window has been closed (via menu) */
};

struct gui_window
{
	struct gui_window_2 *shared;
	int tab;
	struct Node *tab_node;
	int c_x; /* Caret X posn */
	int c_y; /* Caret Y posn */
	int c_w; /* Caret width */
	int c_h; /* Caret height */
	int c_h_temp;
	int scrollx;
	int scrolly;
	struct ami_history_local_window *hw;
	struct List dllist;
	struct hlcache_handle *favicon;
	bool throbbing;
	char *tabtitle;
	APTR deferred_rects_pool;
	struct MinList *deferred_rects;
	struct browser_window *bw;
	float scale;
};

extern struct MinList *window_list; /**\todo stop arexx.c poking about in here */
extern struct Screen *scrn;
extern struct MsgPort *sport;
extern struct gui_window *cur_gw;

/* The return value for these functions must be deallocated using FreeVec() */
STRPTR ami_locale_langs(int *codeset);
char *ami_gui_get_cache_favicon_name(struct nsurl *url, bool only_if_avail);

/* Functions lacking documentation */
void ami_get_msg(void);
void ami_try_quit(void);
void ami_quit_netsurf(void);
void ami_schedule_redraw(struct gui_window_2 *gwin, bool full_redraw);
int ami_key_to_nskey(ULONG keycode, struct InputEvent *ie);
bool ami_text_box_at_point(struct gui_window_2 *gwin, ULONG *restrict x, ULONG *restrict y);
bool ami_mouse_to_ns_coords(struct gui_window_2 *gwin, int *restrict x, int *restrict y,
	int mouse_x, int mouse_y);
BOOL ami_gadget_hit(Object *obj, int x, int y);
void ami_gui_history(struct gui_window_2 *gwin, bool back);
void ami_gui_hotlist_update_all(void);
void ami_gui_tabs_toggle_all(void);
bool ami_locate_resource(char *fullpath, const char *file);
void ami_gui_update_hotlist_button(struct gui_window_2 *gwin);
nserror ami_gui_new_blank_tab(struct gui_window_2 *gwin);
int ami_gui_count_windows(int window, int *tabs);
void ami_gui_set_scale(struct gui_window *gw, float scale);


/**
 * Close a window and all tabs attached to it.
 *
 * @param w gui_window_2 to act upon.
 */
void ami_gui_close_window(void *w);

/**
 * Close all tabs in a window except the active one.
 *
 * @param gwin gui_window_2 to act upon.
 */
void ami_gui_close_inactive_tabs(struct gui_window_2 *gwin);

/**
 * Compatibility function to get space.gadget render area.
 *
 * @param obj A space.gadget object.
 * @param bbox A pointer to a struct IBox *.
 * @return error status.
 */
nserror ami_gui_get_space_box(Object *obj, struct IBox **bbox);

/**
 * Free any data obtained via ami_gui_get_space_box().
 *
 * @param bbox A pointer to a struct IBox.
 */
void ami_gui_free_space_box(struct IBox *bbox);

/**
 * Get the application.library ID NetSurf is registered as.
 *
 * @return App ID.
 */
uint32 ami_gui_get_app_id(void);

/**
 * Get the string for NetSurf's screen titlebar.
 *
 * @return String to use as the screen's titlebar text.
 */
STRPTR ami_gui_get_screen_title(void);

/**
 * Switch to the most-recently-opened tab
 */
void ami_gui_switch_to_new_tab(struct gui_window_2 *gwin);

/**
 * Add a window to the NetSurf window list (to enable event processing)
 */
nserror ami_gui_win_list_add(void *win, int type, const struct ami_win_event_table *table);

/**
 * Remove a window from the NetSurf window list
 */
void ami_gui_win_list_remove(void *win);

/**
 * Get which qualifier keys are being pressed
 */
int ami_gui_get_quals(Object *win_obj);

/**
 * Check rect is not already queued for redraw
 */
bool ami_gui_window_update_box_deferred_check(struct MinList *deferred_rects,
				const struct rect *restrict new_rect, APTR mempool);

#endif

