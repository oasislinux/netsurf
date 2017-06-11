/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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
 * Interface to platform-specific graphical user interface window
 * operations.
 */

#ifndef NETSURF_WINDOW_H
#define NETSURF_WINDOW_H

typedef enum gui_save_type {
	GUI_SAVE_SOURCE,
	GUI_SAVE_DRAW,
	GUI_SAVE_PDF,
	GUI_SAVE_TEXT,
	GUI_SAVE_COMPLETE,
	GUI_SAVE_OBJECT_ORIG,
	GUI_SAVE_OBJECT_NATIVE,
	GUI_SAVE_LINK_URI,
	GUI_SAVE_LINK_URL,
	GUI_SAVE_LINK_TEXT,
	GUI_SAVE_HOTLIST_EXPORT_HTML,
	GUI_SAVE_HISTORY_EXPORT_HTML,
	GUI_SAVE_TEXT_SELECTION,
	GUI_SAVE_CLIPBOARD_CONTENTS
} gui_save_type;

typedef enum {
	GDRAGGING_NONE,
	GDRAGGING_SCROLLBAR,
	GDRAGGING_SELECTION,
	GDRAGGING_OTHER
} gui_drag_type;

/**
 * Window creation control flags.
 */
typedef enum {
	GW_CREATE_NONE = 0, /**< New window */
	GW_CREATE_CLONE = (1 << 0), /**< Clone existing window */
	GW_CREATE_TAB = (1 << 1) /**< Create tab in same window as existing */
} gui_window_create_flags;

struct browser_window;
struct form_control;
struct rect;
struct hlcache_handle;
struct nsurl;

enum gui_pointer_shape;

/**
 * Graphical user interface window function table.
 *
 * function table implementing window operations
 */
struct gui_window_table {

	/* Mandatory entries */

	/**
	 * Create and open a gui window for a browsing context.
	 *
	 * The implementing front end must create a context suitable
	 *  for it to display a window referred to as the "gui window".
	 *
	 * The frontend will be expected to request the core redraw
	 *  areas of the gui window which have become invalidated
	 *  either from toolkit expose events or as a result of a
	 *  invalidate() call.
	 *
	 * Most core operations used by the frontend concerning browser
	 *  windows require passing the browser window context therefor
	 *  the gui window must include a reference to the browser
	 *  window passed here.
	 *
	 * If GW_CREATE_CLONE flag is set existing is non-NULL.
	 *
	 * \param bw The core browsing context associated with the gui window
	 * \param existing An existing gui_window, may be NULL.
	 * \param flags flags to control the gui window creation.
	 * \return gui window, or NULL on error.
	 */
	struct gui_window *(*create)(struct browser_window *bw,
			struct gui_window *existing,
			gui_window_create_flags flags);


	/**
	 * Destroy previously created gui window
	 *
	 * \param gw The gui window to destroy.
	 */
	void (*destroy)(struct gui_window *gw);


	/**
	 * Invalidate an area of a window.
	 *
	 * The specified area of the window should now be considered
	 *  out of date. If the area is NULL the entire window must be
	 *  invalidated. It is expected that the windowing system will
	 *  then subsequently cause redraw/expose operations as
	 *  necessary.
	 *
	 * \note the frontend should not attempt to actually start the
	 *  redraw operations as a result of this callback because the
	 *  core redraw functions may already be threaded.
	 *
	 * \param gw The gui window to invalidate.
	 * \param rect area to redraw or NULL for the entire window area
	 * \return NSERROR_OK on success or appropriate error code
	 */
	nserror (*invalidate)(struct gui_window *gw, const struct rect *rect);


	/**
	 * Get the scroll position of a browser window.
	 *
	 * \param gw The gui window to obtain the scroll position from.
	 * \param sx receives x ordinate of point at top-left of window
	 * \param sy receives y ordinate of point at top-left of window
	 * \return true iff successful
	 */
	bool (*get_scroll)(struct gui_window *gw, int *sx, int *sy);


	/**
	 * Set the scroll position of a browser window.
	 *
	 * scrolls the viewport to ensure the specified rectangle of
	 *   the content is shown.
	 * If the rectangle is of zero size i.e. x0 == x1 and y0 == y1
	 *   the contents will be scrolled so the specified point in the
	 *   content is at the top of the viewport.
	 * If the size of the rectangle is non zero the frontend may
	 *   add padding or centre the defined area or it may simply
	 *   align as in the zero size rectangle
	 *
	 * \param gw The gui window to scroll.
	 * \param rect The rectangle to ensure is shown.
	 * \return NSERROR_OK on success or appropriate error code.
	 */
	nserror (*set_scroll)(struct gui_window *gw, const struct rect *rect);


	/**
	 * Find the current dimensions of a browser window's content area.
	 *
	 * This is used to determine the actual available drawing size
	 * in pixels. This allows contents that can be dynamically
	 * reformatted, such as HTML, to better use the available
	 * space.
	 *
	 * \param gw The gui window to measure content area of.
	 * \param width receives width of window
	 * \param height receives height of window
	 * \param scaled whether to return scaled values
	 * \return NSERROR_OK on success and width and height updated
	 *          else error code.
	 */
	nserror (*get_dimensions)(struct gui_window *gw, int *width, int *height, bool scaled);


	/**
	 * Update the extent of the inside of a browser window to that of the
	 * current content.
	 *
	 * @todo this is used to update scroll bars does it need
	 * renaming? some frontends (windows) do not even implement it.
	 *
	 * \param gw The gui window to update the extent of.
	 */
	void (*update_extent)(struct gui_window *gw);


	/* Optional entries */

	/**
	 * Set the title of a window.
	 *
	 * \param gw The gui window to set title of.
	 * \param title new window title
	 */
	void (*set_title)(struct gui_window *gw, const char *title);

	/**
	 * Set the navigation url.
	 *
	 * \param gw window to update.
	 * \param url The url to use as icon.
	 */
	nserror (*set_url)(struct gui_window *gw, struct nsurl *url);

	/**
	 * Set a favicon for a gui window.
	 *
	 * \param gw window to update.
	 * \param icon handle to object to use as icon.
	 */
	void (*set_icon)(struct gui_window *gw, struct hlcache_handle *icon);

	/**
	 * Set the status bar message of a browser window.
	 *
	 * \param g gui_window to update
	 * \param text new status text
	 */
	void (*set_status)(struct gui_window *g, const char *text);

	/**
	 * Change mouse pointer shape
	 *
	 * \param g The gui window to change pointer shape in.
	 * \param shape The new shape to change to.
	 */
	void (*set_pointer)(struct gui_window *g, enum gui_pointer_shape shape);

	/**
	 * Place the caret in a browser window.
	 *
	 * \param  g	   window with caret
	 * \param  x	   coordinates of caret
	 * \param  y	   coordinates of caret
	 * \param  height  height of caret
	 * \param  clip	   clip rectangle, or NULL if none
	 */
	void (*place_caret)(struct gui_window *g, int x, int y, int height, const struct rect *clip);

	/**
	 * Remove the caret, if present.
	 *
	 * \param g window with caret
	 */
	void (*remove_caret)(struct gui_window *g);

	/**
	 * start the navigation throbber.
	 *
	 * \param g window in which to start throbber.
	 */
	void (*start_throbber)(struct gui_window *g);

	/**
	 * stop the navigation throbber.
	 *
	 * \param g window with throbber to stop
	 */
	void (*stop_throbber)(struct gui_window *g);

	/**
	 * start a drag operation within a window
	 *
	 * \param g window to start drag from.
	 * \param type Type of drag to start
	 * \param rect Confining rectangle of drag operation.
	 * \return true if drag started else false.
	 */
	bool (*drag_start)(struct gui_window *g, gui_drag_type type, const struct rect *rect);

	/**
	 * save link operation
	 *
	 * \param g window to save link from.
	 * \param url The link url.
	 * \param title The title of the link.
	 * \return NSERROR_OK on success else appropriate error code.
	 */
	nserror (*save_link)(struct gui_window *g, struct nsurl *url, const char *title);


	/**
	 * Starts drag scrolling of a browser window
	 *
	 * \param g the window to scroll
	 */
	bool (*scroll_start)(struct gui_window *g);

	/**
	 * Called when the gui_window has new content.
	 *
	 * \param gw The gui window that has new content
	 */
	void (*new_content)(struct gui_window *gw);

	/**
	 * create a form select menu
	 *
	 * \param gw The gui window to open select menu form gadget in.
	 * \param control The form control gadget handle.
	 */
	void (*create_form_select_menu)(struct gui_window *gw, struct form_control *control);

	/**
	 * Called when file chooser gadget is activated
	 *
	 * \param gw The gui window to open file chooser in.
	 * \param hl The content of the object.
	 * \param gadget The form control gadget handle.
	 */
	void (*file_gadget_open)(struct gui_window *gw, struct hlcache_handle *hl, struct form_control *gadget);

	/**
	 * object dragged to window
	 *
	 * \param gw The gui window to save dragged object of.
	 * \param c The content of the object.
	 * \param type the type of save.
	 */
	void (*drag_save_object)(struct gui_window *gw, struct hlcache_handle *c, gui_save_type type);

	/**
	 * drag selection save
	 *
	 * \param gw The gui window to save dragged selection of.
	 * \param selection The selection to save.
	 */
	void (*drag_save_selection)(struct gui_window *gw, const char *selection);

	/**
	 * selection started
	 *
	 * \param gw The gui window to start selection in.
	 */
	void (*start_selection)(struct gui_window *gw);
};

#endif
