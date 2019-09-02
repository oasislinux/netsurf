/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#ifndef NETSURF_GTK_SCAFFOLDING_H
#define NETSURF_GTK_SCAFFOLDING_H 1

#include <stdbool.h>
#include "utils/errors.h"

struct bitmap;
struct hlcache_handle;
struct gui_window;
struct gui_search_web_table;
struct nsurl;



struct gtk_history_window {
	struct nsgtk_scaffolding 	*g;
	GtkWindow		*window;
	GtkScrolledWindow	*scrolled;
	GtkDrawingArea		*drawing_area;
};

struct gtk_search {
	GtkToolbar			*bar;
	GtkEntry			*entry;
	GtkToolButton			*buttons[3]; /* back, forward, */
	GtkCheckButton			*checkAll;	/* close */
	GtkCheckButton			*caseSens;
};


/**
 * create a new scaffolding for a window.
 *
 * \param gw The gui window to create the new scaffold around.
 * \return The newly constructed scaffold or NULL on error.
 */
struct nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *gw);

/**
 * causes all scaffolding windows to be destroyed.
 *
 * \return NSERROR_OK and all scaffolding windows destroyed else
 *         NSERROR_INVALID if download in progress and user continued.
 */
nserror nsgtk_scaffolding_destroy_all(void);

/**
 * Update scaffolding window when throbber state changes
 */
nserror nsgtk_scaffolding_throbber(struct gui_window* gw, bool active);

/**
 * open the toolbar context menu
 */
nserror nsgtk_scaffolding_toolbar_context_menu(struct nsgtk_scaffolding *gs);

/**
 * open the burger menu
 */
nserror nsgtk_scaffolding_burger_menu(struct nsgtk_scaffolding *gs);

/**
 * Obtain the most recently used scaffolding element.
 *
 * This allows tabs to be opened in the most recently used window
 */
struct nsgtk_scaffolding *nsgtk_current_scaffolding(void);

/* acessors for gtk elements within a scaffold */

/**
 * Get the gtk window for a scaffolding.
 */
GtkWindow *nsgtk_scaffolding_window(struct nsgtk_scaffolding *g);

/**
 * Get the gtk notebook from a scaffold.
 */
GtkNotebook *nsgtk_scaffolding_notebook(struct nsgtk_scaffolding *g);

/**
 * Get the gtk url bar from a scaffold.
 */
GtkWidget *nsgtk_scaffolding_urlbar(struct nsgtk_scaffolding *g);

/**
 * Get the gtk toolbar from a scaffold.
 */
GtkToolbar *nsgtk_scaffolding_toolbar(struct nsgtk_scaffolding *g);


struct gtk_search *nsgtk_scaffolding_search(struct nsgtk_scaffolding *g);

GtkMenuBar *nsgtk_scaffolding_menu_bar(struct nsgtk_scaffolding *g);

struct gtk_history_window *nsgtk_scaffolding_history_window(struct nsgtk_scaffolding *g);

struct gui_window *nsgtk_scaffolding_top_level(struct nsgtk_scaffolding *g);

/**
 * reset the scaffold offset value to 0.
 *
 * \todo The value is only ever altered in
 * nsgtk_scaffolding_toolbar_size_allocate and is something to do with
 * the history button either clarify or remove!
 */
void nsgtk_scaffolding_reset_offset(struct nsgtk_scaffolding *g);

/**
 * Iterate through available scaffolding.
 */
struct nsgtk_scaffolding *nsgtk_scaffolding_iterate(struct nsgtk_scaffolding *g);


void nsgtk_scaffolding_toggle_search_bar_visibility(struct nsgtk_scaffolding *g);

/**
 * Set the current active top level gui window.
 */
void nsgtk_scaffolding_set_top_level(struct gui_window *g);

/**
 * update the sensitivity of context sensitive UI elements
 *
 * widgets altered in arrays:
 *   main
 *   right click menu
 *   location
 *   popup
 * current arrays are:
 *   stop
 *   reload
 *   cut
 *   copy
 *   paste
 *   back
 *   forward
 *   nexttab
 *   prevtab
 *   closetab
 */
void nsgtk_scaffolding_set_sensitivity(struct nsgtk_scaffolding *g);

/**
 * Open a context sensitive menu.
 *
 * \param g the scaffolding containing the browser window.
 * \param x The x co-ordinate.
 * \param y The y co-ordinate.
 */
void nsgtk_scaffolding_context_menu(struct nsgtk_scaffolding *g, gdouble x, gdouble y);

/* core acessors */
/**
 * set the title in the window
 *
 * \param gw The gui window to set title on
 * \param title The title to set which may be NULL
 */
void nsgtk_window_set_title(struct gui_window *gw, const char *title);

/**
 * toolbar style changed
 */
void nsgtk_scaffolding_toolbars(struct nsgtk_scaffolding *g);

#endif /* NETSURF_GTK_SCAFFOLDING_H */
