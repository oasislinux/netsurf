/*
 * Copyright 2017 Michael Forney <mforney@mforney.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <libcss/fpmath.h>
#include <pixman.h>
#include <xkbcommon/xkbcommon.h>

#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/utils.h"
#include "netsurf/bitmap.h"
#include "netsurf/browser_window.h"
#include "netsurf/clipboard.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "netsurf/search.h"
#include "netsurf/window.h"
#include "desktop/browser_history.h"
#include "desktop/scrollbar.h"
#include "desktop/search.h"
#include "desktop/searchweb.h"
#include "desktop/textarea.h"
#include "desktop/textinput.h"

#include "tiny/ui.h"
#include "tiny/icons.h"
#include "tiny/platform.h"
#include "tiny/render.h"

#define TOOLBAR_SIZE 24
#define STATUS_HEIGHT 16

#define BROWSER_CLICK (\
	BROWSER_MOUSE_PRESS_1|BROWSER_MOUSE_PRESS_2|\
	BROWSER_MOUSE_CLICK_1|BROWSER_MOUSE_CLICK_2|\
	BROWSER_MOUSE_DRAG_1|BROWSER_MOUSE_DRAG_2\
)

/* colors */
#define WINDOW_BACKGROUND 0xeeeeee
#define WINDOW_BORDER     0x999999
#define URL_BACKGROUND    0xffffea
#define URL_BACKGROUND_HL 0xeeee9e
#define URL_BORDER        0xcc8888
#define CARET_STROKE      0x000000

enum {
	UI_BUTTONS,
	UI_URL,
	UI_CONTENT,
	UI_HSCROLL,
	UI_VSCROLL,
	UI_STATUS,

	UI_NUMELEMENTS,
};

struct element {
	const struct elementimpl *impl;
	struct rect r;
	bool hidden;
};

struct elementimpl {
	void (*redraw)(struct gui_window *g, struct element *e, struct rect *clip, const struct redraw_context *ctx);
	void (*mouse)(struct gui_window *g, struct element *e, browser_mouse_state state, int x, int y);
	void (*key)(struct gui_window *g, struct element *e, uint32_t key);
};

struct gui_window {
	struct platform_window *platform;

	int width, height;

	struct browser_window *bw;
	struct textarea *url;

	struct element ui[UI_NUMELEMENTS];

	bool throbbing;
	const char *status;

	struct {
		int x, y;
		int focus;
		browser_mouse_state state;
	} ptr;

	struct {
		int focus;
	} kbd;

	struct {
		int x, y, h;
	} caret;

	struct {
		int x, y;

		struct scrollbar *h, *v;
		bool hdrag, vdrag;
	} scroll;

	struct {
		int w, h;
	} extent;
};

static int
rectwidth(const struct rect *r) {
	return r->x1 - r->x0;
}

static int
rectheight(const struct rect *r) {
	return r->y1 - r->y0;
}

static int
rectcontains(const struct rect *r, int x, int y) {
	return r->x0 <= x && x < r->x1 && r->y0 <= y && y < r->y1;
}

static bool
trimrect(struct rect *r1, const struct rect *r2)
{
	if (r1->x0 < r2->x0)
		r1->x0 = min(r1->x1, r2->x0);
	if (r1->y0 < r2->y0)
		r1->y0 = min(r1->y1, r2->y0);

	if (r2->x1 < r1->x1)
		r1->x1 = max(r1->x0, r2->x1);
	if (r2->y1 < r1->y1)
		r1->y1 = max(r1->y0, r2->y1);

	return r1->x0 < r1->x1 && r1->y0 < r1->y1;
}

static void
removecaret(struct gui_window *g)
{
	if (!g->caret.h)
		return;
	platform_window_update(g->platform, &(struct rect){g->caret.x, g->caret.y, g->caret.x + 1, g->caret.y + g->caret.h});
	g->caret.h = 0;
}

static void
placecaret(struct gui_window *g, int x0, int y0, int x, int y, int h, const struct rect *clip)
{
	removecaret(g);
	if (y < clip->y0) {
		h -= clip->y0 - y;
		y = clip->y0;
	}
	if (y + h > clip->y1)
		h = clip->y1 - y;
	g->caret.x = x + x0;
	g->caret.y = y + y0;
	g->caret.h = h;
	platform_window_update(g->platform, &(struct rect){
		g->caret.x, g->caret.y,
		g->caret.x + 1, g->caret.y + g->caret.h,
	});
}

static void
navigate(struct gui_window *g)
{
	struct nsurl *url;
	char *text;
	ssize_t len;
	nserror err;

	len = textarea_get_text(g->url, NULL, 0);
	if (len == -1)
		return;
	text = malloc(len);
	if (!text)
		return;
	if (textarea_get_text(g->url, text, len) == -1)
		return;
	if (strchr(text, '.') || text[len - 1] == '/') {
		err = nsurl_create(text, &url);
	} else {
		err = NSERROR_BAD_URL;
	}
	if (err == NSERROR_BAD_URL)
		err = search_web_omni(text, SEARCH_WEB_OMNI_SEARCHONLY, &url);
	if (err != NSERROR_OK)
		return;
	browser_window_navigate(g->bw, url, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
	textarea_set_caret(g->url, -1);
	g->kbd.focus = UI_CONTENT;
}

/**** buttons ****/
static void
buttons_redraw(struct gui_window *g, struct element *e, struct rect *clip, const struct redraw_context *ctx)
{
	plot_style_t style = {
		.fill_colour = WINDOW_BACKGROUND,
		.stroke_colour = 0x999999,
		.stroke_width = 1,
	};
	int buttons[] = {
		ICON_BACK,
		ICON_FORWARD,
		ICON_HOME,
		ICON_RELOAD,
	};
	struct bitmap *b;
	int i, w, h;

	if (g->throbbing)
		buttons[3] = ICON_STOP;
	ctx->plot->clip(clip);
	ctx->plot->rectangle(e->r.x0, e->r.y0, e->r.x1, e->r.y1, &style);
	for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
		b = tiny_icons[buttons[i]];
		w = tiny_bitmap_table->get_width(b);
		h = tiny_bitmap_table->get_height(b);
		ctx->plot->bitmap(e->r.x0 + i * w, e->r.y0, w, h, b, style.fill_colour, 0);
	}
	ctx->plot->line(e->r.x0, e->r.y1 - 1, e->r.x1, e->r.y1 - 1, &style);
}

static void
buttons_mouse(struct gui_window *g, struct element *e, browser_mouse_state state, int x, int y)
{
	const char *homepage;
	nsurl *url;
	nserror err;

	if (!(state & BROWSER_MOUSE_CLICK_1))
		return;

	switch (x / TOOLBAR_SIZE) {
	case 0:
		browser_window_history_back(g->bw, false);
		break;
	case 1:
		browser_window_history_forward(g->bw, false);
		break;
	case 2:
		homepage = nsoption_charp(homepage_url);
		if (!homepage)
			homepage = NETSURF_HOMEPAGE;
		err = nsurl_create(homepage, &url);
		if (err)
			return;
		browser_window_navigate(g->bw, url, NULL, BW_NAVIGATE_NONE, NULL, NULL, NULL);
		break;
	case 3:
		if (g->throbbing)
			browser_window_stop(g->bw);
		else
			browser_window_reload(g->bw, true);
		break;
	}
}

static const struct elementimpl buttonsimpl = {
	.redraw = buttons_redraw,
	.mouse = buttons_mouse,
};

/**** url ****/
static void urlcallback(void *data, struct textarea_msg *msg)
{
	struct gui_window *g = data;
	const struct rect *r = &g->ui[UI_URL].r;
	int x, y, h;

	switch (msg->type) {
	case TEXTAREA_MSG_REDRAW_REQUEST:
		platform_window_update(g->platform, &(struct rect){
			msg->data.redraw.x0 + r->x0, msg->data.redraw.y0 + r->y0,
			msg->data.redraw.x1 + r->x0, msg->data.redraw.y1 + r->y0,
		});
		break;
	case TEXTAREA_MSG_CARET_UPDATE:
		switch (msg->data.caret.type) {
		case TEXTAREA_CARET_SET_POS:
			if (g->kbd.focus == UI_URL) {
				x = msg->data.caret.pos.x;
				y = msg->data.caret.pos.y;
				h = msg->data.caret.pos.height;
				placecaret(g, r->x0, r->y0, x, y, h, msg->data.caret.pos.clip);
			}
			break;
		case TEXTAREA_CARET_HIDE:
			if (g->kbd.focus == UI_URL)
				removecaret(g);
			break;
		}
		break;
	case TEXTAREA_MSG_TEXT_MODIFIED:
		break;
	case TEXTAREA_MSG_SELECTION_REPORT:
		LOG("selection report\n");
		break;
	default:
		break;
	}
}

static void
url_redraw(struct gui_window *g, struct element *e, struct rect *clip, const struct redraw_context *ctx)
{
	textarea_redraw(g->url, e->r.x0, e->r.y0, 0xffffff, 1, clip, ctx);
}

static void
url_mouse(struct gui_window *g, struct element *e, browser_mouse_state state, int x, int y)
{
	textarea_mouse_action(g->url, state, x, y);
}

static void
url_key(struct gui_window *g, struct element *e, uint32_t key)
{
	switch (key) {
	case '\r':
		navigate(g);
		break;
	default:
		textarea_keypress(g->url, key);
	}
}

static const struct elementimpl urlimpl = {
	.redraw = url_redraw,
	.mouse = url_mouse,
	.key = url_key,
};

/**** content ****/
static void
content_redraw(struct gui_window *g, struct element *e, struct rect *clip, const struct redraw_context *ctx)
{
	browser_window_redraw(g->bw, e->r.x0 - g->scroll.x, e->r.y0 - g->scroll.y, clip, ctx);
}

static void
content_mouse(struct gui_window *g, struct element *e, browser_mouse_state state, int x, int y)
{
	x += g->scroll.x;
	y += g->scroll.y;
	//printf("content %s ", isclick ? "click" : "track");
	//printmouse(state, x, y);
	if (state & BROWSER_CLICK) {
		browser_window_mouse_click(g->bw, state, x, y);
	} else {
		browser_window_mouse_track(g->bw, state, x, y);
	}
}

static void
content_key(struct gui_window *g, struct element *e, uint32_t key)
{
	switch (key) {
	case 'f':
		// TODO: implement search
		break;
	}
	browser_window_key_press(g->bw, key);
}

static const struct elementimpl contentimpl = {
	.redraw = content_redraw,
	.mouse = content_mouse,
	.key = content_key,
};

/**** scrollbars ****/
static void
scrollcallback(void *data, struct scrollbar_msg_data *msg)
{
	struct gui_window *g = data;
	int id;

	if (msg->scrollbar == g->scroll.h)
		id = UI_HSCROLL;
	else if (msg->scrollbar == g->scroll.v)
		id = UI_VSCROLL;
	else
		return;

	switch (msg->msg) {
	case SCROLLBAR_MSG_MOVED:
		switch (id) {
		case UI_HSCROLL:
			if (g->scroll.x != msg->scroll_offset)
				g->scroll.x = msg->scroll_offset;
			break;
		case UI_VSCROLL:
			if (g->scroll.y != msg->scroll_offset)
				g->scroll.y = msg->scroll_offset;
			break;
		}
		platform_window_update(g->platform, &g->ui[UI_CONTENT].r);
		platform_window_update(g->platform, &g->ui[id].r);
		break;
	case SCROLLBAR_MSG_SCROLL_START:
		switch (id) {
		case UI_HSCROLL:
			g->scroll.hdrag = true;
			break;
		case UI_VSCROLL:
			g->scroll.vdrag = true;
			break;
		}
		break;
	case SCROLLBAR_MSG_SCROLL_FINISHED:
		switch (id) {
		case UI_HSCROLL:
			g->scroll.hdrag = false;
			break;
		case UI_VSCROLL:
			g->scroll.vdrag = false;
			break;
		}
		break;
	}
}

static struct scrollbar *
findscrollbar(struct gui_window *g, struct element *e)
{
	if (e == &g->ui[UI_HSCROLL])
		return g->scroll.h;
	if (e == &g->ui[UI_VSCROLL])
		return g->scroll.v;

	return NULL;
}

static void
scroll_redraw(struct gui_window *g, struct element *e, struct rect *clip, const struct redraw_context *ctx)
{
	struct scrollbar *s;

	s = findscrollbar(g, e);
	if (!s)
		return;
	ctx->plot->clip(clip);  // TODO: scrollbar should probably do this
	scrollbar_redraw(s, e->r.x0, e->r.y0, clip, 1, ctx);
}

static void
scroll_mouse(struct gui_window *g, struct element *e, browser_mouse_state state, int x, int y)
{
	struct scrollbar *s;
	bool drag;

	switch (e - g->ui) {
	case UI_HSCROLL:
		s = g->scroll.h;
		drag = g->scroll.hdrag;
		break;
	case UI_VSCROLL:
		s = g->scroll.v;
		drag = g->scroll.vdrag;
		break;
	default:
		return;
	}
	if (drag && !(state & BROWSER_MOUSE_DRAG_ON)) {
		scrollbar_mouse_drag_end(s, state, x, y);
	} else {
		scrollbar_mouse_action(s, state, x, y);
	}
}

static const struct elementimpl scrollimpl = {
	.redraw = scroll_redraw,
	.mouse = scroll_mouse,
};

/**** status line ****/
static void
status_redraw(struct gui_window *g, struct element *e, struct rect *clip, const struct redraw_context *ctx)
{
	struct plot_font_style fontstyle = {
		.weight = 400,
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 10 * FONT_SIZE_SCALE,
	};
	plot_style_t style = {
		.fill_colour = WINDOW_BACKGROUND,
		.stroke_colour = WINDOW_BORDER,
		.stroke_width = 1,
	};

	ctx->plot->clip(clip);
	ctx->plot->rectangle(e->r.x0, e->r.y0, e->r.x1, e->r.y1, &style);
	ctx->plot->text(e->r.x0 + 2, e->r.y1 - 4, g->status, strlen(g->status), &fontstyle);
	ctx->plot->line(e->r.x0, e->r.y0, e->r.x1, e->r.y0, &style);
}

static const struct elementimpl statusimpl = {
	.redraw = status_redraw,
};

/**** gui_window implementation ****/
static struct gui_window *
window_create(struct browser_window *bw, struct gui_window *existing, gui_window_create_flags flags)
{
	struct gui_window *g;
	struct textarea_setup setup = {
		.pad_top = 0,
		.pad_right = 2,
		.pad_bottom = 0,
		.pad_left = 2,
		.border_width = 1,
		.border_col = URL_BORDER,
		.selected_bg = URL_BACKGROUND_HL,
		.text = {
			.family = PLOT_FONT_FAMILY_SANS_SERIF,
			.size = 12 * FONT_SIZE_SCALE,
			.weight = 400,
			.background = URL_BACKGROUND,
		},
	};
	nserror err;

	g = calloc(sizeof(*g), 1);
	if (!g)
		goto err0;
	g->url = textarea_create(TEXTAREA_DEFAULT, &setup, urlcallback, g);
	if (!g->url)
		goto err1;
	err = scrollbar_create(true, 0, 0, 0, g, scrollcallback, &g->scroll.h);
	if (err)
		goto err2;
	err = scrollbar_create(false, 0, 0, 0, g, scrollcallback, &g->scroll.v);
	if (err)
		goto err3;
	g->platform = platform_window_create(g);
	if (!g->platform)
		goto err4;

	g->ui[UI_BUTTONS] = (struct element){.impl = &buttonsimpl};
	g->ui[UI_URL] = (struct element){.impl = &urlimpl};
	g->ui[UI_CONTENT] = (struct element){.impl = &contentimpl};
	g->ui[UI_HSCROLL] = (struct element){.impl = &scrollimpl, .hidden = true};
	g->ui[UI_VSCROLL] = (struct element){.impl = &scrollimpl, .hidden = true};
	g->ui[UI_STATUS] = (struct element){.impl = &statusimpl};
	g->bw = bw;

	g->ptr.focus = UI_CONTENT;
	g->kbd.focus = UI_CONTENT;

	return g;

err4:
	scrollbar_destroy(g->scroll.v);
err3:
	scrollbar_destroy(g->scroll.h);
err2:
	textarea_destroy(g->url);
err1:
	free(g);
err0:
	return NULL;
}

static void
window_destroy(struct gui_window *g)
{
	LOG("destroy\n");
}

static void
window_update(struct gui_window *g, const struct rect *r)
{
	struct element *e = &g->ui[UI_CONTENT];
	struct rect gr = {
		e->r.x0 + r->x0 - g->scroll.x, e->r.y0 + r->y0 - g->scroll.y,
		e->r.x1 + r->x1 - g->scroll.x, e->r.y1 + r->y1 - g->scroll.y,
	};

	platform_window_update(g->platform, &gr);
}

static void
window_redraw(struct gui_window *g)
{
	platform_window_update(g->platform, &(struct rect){0, 0, g->width, g->height});
}

static bool
window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	*sx = g->scroll.x;
	*sy = g->scroll.y;
	return true;
}

static void
window_set_scroll(struct gui_window *g, int sx, int sy)
{
	struct element *e = &g->ui[UI_CONTENT];

	sx = max(sx, 0);
	sy = max(sy, 0);
	sx = min(sx, g->extent.w - rectwidth(&e->r));
	sy = min(sy, g->extent.h - rectheight(&e->r));
	if (g->scroll.x != sx || g->scroll.y != sy) {
		g->scroll.x = sx;
		g->scroll.y = sy;
		scrollbar_set(g->scroll.h, sx, false);
		scrollbar_set(g->scroll.v, sy, false);
		platform_window_update(g->platform, &e->r);
		platform_window_update(g->platform, &g->ui[UI_HSCROLL].r);
		platform_window_update(g->platform, &g->ui[UI_VSCROLL].r);
	}
}

static void
window_get_dimensions(struct gui_window *g, int *width, int *height, bool scaled)
{
	struct element *e = &g->ui[UI_CONTENT];

	*width = rectwidth(&e->r);
	*height = rectheight(&e->r);
}

static void
window_update_extent(struct gui_window *g)
{
	struct element *e;
	bool reformat = false;
	int w, h;

	LOG("content %d, %d\n", rectwidth(&g->ui[UI_CONTENT].r), rectheight(&g->ui[UI_CONTENT].r));

	browser_window_get_extents(g->bw, false, &g->extent.w, &g->extent.h);
	w = g->width;
	h = g->height;

	// TODO: This should be simplified.
	e = &g->ui[UI_VSCROLL];
	if (g->extent.h > rectheight(&g->ui[UI_CONTENT].r)) {
		if (e->hidden) {
			e->hidden = false;
			e->r = (struct rect){w - SCROLLBAR_WIDTH, TOOLBAR_SIZE, w, h - STATUS_HEIGHT};
			g->ui[UI_CONTENT].r.x1 -= SCROLLBAR_WIDTH;
			scrollbar_set_extents(g->scroll.v, rectheight(&e->r), rectheight(&g->ui[UI_CONTENT].r), g->extent.h);
			reformat = true;
		} else {
			scrollbar_set_extents(g->scroll.v, -1, -1, g->extent.h);
		}
	} else if (!e->hidden) {
		e->hidden = true;
		g->ui[UI_CONTENT].r.x1 += SCROLLBAR_WIDTH;
	}

	e = &g->ui[UI_HSCROLL];
	if (g->extent.w > rectwidth(&g->ui[UI_CONTENT].r)) {
		if (e->hidden) {
			e->hidden = false;
			e->r = (struct rect){0, h - SCROLLBAR_WIDTH - STATUS_HEIGHT, w, h - STATUS_HEIGHT};
			if (!g->ui[UI_VSCROLL].hidden)
				e->r.x1 -= SCROLLBAR_WIDTH;
			g->ui[UI_CONTENT].r.y1 -= SCROLLBAR_WIDTH;
			scrollbar_set_extents(g->scroll.h, rectwidth(&e->r), rectwidth(&g->ui[UI_CONTENT].r), g->extent.w);
			reformat = true;
		} else {
			scrollbar_set_extents(g->scroll.h, -1, -1, g->extent.w);
		}
	} else if (!e->hidden) {
		e->hidden = true;
		g->ui[UI_CONTENT].r.y1 += SCROLLBAR_WIDTH;
	}

	if (reformat) {
		e = &g->ui[UI_CONTENT];
		browser_window_reformat(g->bw, false, rectwidth(&e->r), rectheight(&e->r));
	}

	LOG("update_extent %d, %d\n", g->extent.w, g->extent.h);
}

static void
window_reformat(struct gui_window *g)
{
	// TODO: When does this happen?
	LOG("reformat\n");
}

static void
window_set_title(struct gui_window *g, const char *title)
{
	platform_window_set_title(g->platform, title);
}

static nserror
window_set_url(struct gui_window *g, struct nsurl *url)
{
	if (g->kbd.focus == UI_URL)
		return NSERROR_OK;
	if (!textarea_set_text(g->url, nsurl_access(url)))
		return NSERROR_NOMEM;
	platform_window_update(g->platform, &g->ui[UI_URL].r);

	return NSERROR_OK;
}

static void
window_set_status(struct gui_window *g, const char *status)
{
	g->status = status;
	platform_window_update(g->platform, &g->ui[UI_STATUS].r);
}

static void
window_set_pointer(struct gui_window *g, enum gui_pointer_shape shape)
{
	platform_window_set_pointer(g->platform, shape);
}

static void
window_remove_caret(struct gui_window *g)
{
	if (g->kbd.focus == UI_CONTENT)
		removecaret(g);
}

static void
window_start_throbber(struct gui_window *g)
{
	g->throbbing = true;
	platform_window_update(g->platform, &g->ui[UI_BUTTONS].r);
}

static void
window_stop_throbber(struct gui_window *g)
{
	g->throbbing = false;
	platform_window_update(g->platform, &g->ui[UI_BUTTONS].r);
}

static void
window_place_caret(struct gui_window *g, int x, int y, int h, const struct rect *clip)
{
	struct element *e;

	e = &g->ui[UI_CONTENT];
	placecaret(g, e->r.x0 - g->scroll.x, e->r.y0 - g->scroll.y, x, y, h, clip);
}

static struct gui_window_table window_table = {
	.create = window_create,
	.destroy = window_destroy,
	.redraw = window_redraw,
	.update = window_update,
	.get_scroll = window_get_scroll,
	.set_scroll = window_set_scroll,
	.get_dimensions = window_get_dimensions,
	.update_extent = window_update_extent,
	.reformat = window_reformat,
	.set_title = window_set_title,
	.set_url = window_set_url,
	/* set_icon */
	.set_status = window_set_status,
	.set_pointer = window_set_pointer,
	.place_caret = window_place_caret,
	.remove_caret = window_remove_caret,
	.start_throbber = window_start_throbber,
	.stop_throbber = window_stop_throbber,
	/* drag_start */
	/* save_link */
	/* scroll_visible */
	/* scroll_start */
	/* new_content */
	/* create_form_select_menu */
	/* file_gadget_open */
	/* drag_save_object */
	/* drag_save_selection */
	/* start_selection */
};

struct gui_window_table *tiny_window_table = &window_table;

/**** gui_window internal interface ****/
void
gui_window_reformat(struct gui_window *g, int w, int h)
{
	const struct rect *r;

	LOG("reformat %d, %d\n", w, h);

	g->width = w;
	g->height = h;

	g->ui[UI_BUTTONS].r = (struct rect){0, 0, TOOLBAR_SIZE * 4, TOOLBAR_SIZE};
	g->ui[UI_URL].r = (struct rect){TOOLBAR_SIZE * 4, 0, w, TOOLBAR_SIZE};
	g->ui[UI_CONTENT].r = (struct rect){0, TOOLBAR_SIZE, w, h - STATUS_HEIGHT};
	g->ui[UI_STATUS].r = (struct rect){0, h - STATUS_HEIGHT, w, h};
	g->ui[UI_HSCROLL].hidden = true;
	g->ui[UI_VSCROLL].hidden = true;

	r = &g->ui[UI_URL].r;
	textarea_set_dimensions(g->url, rectwidth(r), rectheight(r));

	r = &g->ui[UI_CONTENT].r;
	browser_window_reformat(g->bw, false, rectwidth(r), rectheight(r));
}

void
gui_window_redraw(struct gui_window *g, const struct rect *clip, const struct redraw_context *ctx)
{
	struct rect subclip;
	plot_style_t style;
	int i;
	struct element *e;

	//printf("clip { %d, %d; %d, %d }\n", clip->x0, clip->y0, clip->x1, clip->y1);

	for (i = 0; i < UI_NUMELEMENTS; ++i) {
		e = &g->ui[i];
		if (e->hidden)
			continue;
		subclip = e->r;
		if (trimrect(&subclip, clip))
			e->impl->redraw(g, e, &subclip, ctx);
	}

	if (g->caret.h) {
		style.stroke_type = PLOT_OP_TYPE_SOLID;
		style.stroke_width = 1;
		style.stroke_colour = CARET_STROKE;
		ctx->plot->clip(NULL);
		ctx->plot->line(g->caret.x, g->caret.y, g->caret.x, g->caret.y + g->caret.h, &style);
	}

}

#if 0
static void
printmouse(browser_mouse_state mouse, int x, int y)
{
	const char *sep = "";
	printf("%4d, %4d ", x, y);
#define PRINT(x) \
	if (mouse & BROWSER_MOUSE_ ## x) { \
		printf("%s" #x, sep); \
		sep = "|"; \
	}
	PRINT(PRESS_1)
	PRINT(PRESS_2)
	PRINT(CLICK_1)
	PRINT(CLICK_2)
	PRINT(DOUBLE_CLICK)
	PRINT(TRIPLE_CLICK)
	PRINT(DRAG_1)
	PRINT(DRAG_2)
	PRINT(DRAG_ON)
	PRINT(HOLDING_1)
	PRINT(HOLDING_2)
	PRINT(MOD_1)
	PRINT(MOD_2)
	PRINT(MOD_3)
	putchar('\n');
}
#endif

static void
mousedispatch(struct gui_window *g, browser_mouse_state state)
{
	int x, y;
	struct element *e;

	e = &g->ui[g->ptr.focus];
	x = g->ptr.x;
	y = g->ptr.y;

	if (x < e->r.x0)
		x = e->r.x0;
	if (y < e->r.y0)
		y = e->r.y0;
	if (e->r.x1 < x)
		x = e->r.x1;
	if (e->r.y1 < y)
		y = e->r.y1;

	if (e->impl->mouse)
		e->impl->mouse(g, e, state, x - e->r.x0, y - e->r.y0);
}

static void
mousefocus(struct gui_window *g, bool click)
{
	int i;
	struct element *e;

	if (g->ptr.state & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_DRAG_ON))
		return;

	for (i = 0; i < UI_NUMELEMENTS; ++i) {
		e = &g->ui[i];
		if (e->hidden)
			continue;
		if (rectcontains(&e->r, g->ptr.x, g->ptr.y)) {
			// TODO: Some sort of focus change mechanism?
			if (g->ptr.focus != i) {
				g->ptr.focus = i;
				switch (i) {
				case UI_URL:
					platform_window_set_pointer(g->platform, GUI_POINTER_CARET);
					break;
				case UI_BUTTONS:
				case UI_HSCROLL:
				case UI_VSCROLL:
					platform_window_set_pointer(g->platform, GUI_POINTER_DEFAULT);
					platform_window_set_pointer(g->platform, GUI_POINTER_DEFAULT);
					break;
				}
			}
			if (click && g->kbd.focus != i && i != UI_HSCROLL && i != UI_VSCROLL) {
				switch (g->kbd.focus) {
				case UI_URL:
					textarea_clear_selection(g->url);
					textarea_set_caret(g->url, -1);
					break;
				case UI_CONTENT:
					browser_window_remove_caret(g->bw, false);
					break;
				}
				g->kbd.focus = i;
			}
		}
	}
}

void
gui_window_button(struct gui_window *g, int button, bool pressed)
{
	if (pressed) {
		mousefocus(g, true);
		switch (button) {
		case 1:
			g->ptr.state |= BROWSER_MOUSE_PRESS_1;
			mousedispatch(g, g->ptr.state);
			break;
		case 2:
			g->ptr.state |= BROWSER_MOUSE_PRESS_2;
			break;
		default:
			return;
		}
		mousedispatch(g, g->ptr.state);
	} else {
		switch (button) {
		case 1:
			// TODO: Can this be simpler?
			g->ptr.state &= ~(BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_HOLDING_1);
			if (!(g->ptr.state & BROWSER_MOUSE_HOLDING_2))
				g->ptr.state &= ~BROWSER_MOUSE_DRAG_ON;
			mousedispatch(g, g->ptr.state | BROWSER_MOUSE_CLICK_1);
			mousedispatch(g, g->ptr.state);
			break;
		case 2:
			g->ptr.state &= ~(BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_HOLDING_2);
			if (!(g->ptr.state & BROWSER_MOUSE_HOLDING_1))
				g->ptr.state &= ~BROWSER_MOUSE_DRAG_ON;
			mousedispatch(g, g->ptr.state | BROWSER_MOUSE_CLICK_2);
			mousedispatch(g, g->ptr.state);
			break;
		case 3:
			// TODO: Implement context menu.
			return;
		default:
			return;
		}
		mousefocus(g, false);
	}
}

void
gui_window_motion(struct gui_window *g, int x, int y)
{
	if (x == g->ptr.x && y == g->ptr.y)
		return;
	g->ptr.x = x;
	g->ptr.y = y;
	if (g->ptr.state & BROWSER_MOUSE_PRESS_1) {
		g->ptr.state &= ~BROWSER_MOUSE_PRESS_1;
		mousedispatch(g, g->ptr.state | BROWSER_MOUSE_DRAG_1);
		g->ptr.state |= BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
	}
	if (g->ptr.state & BROWSER_MOUSE_PRESS_2) {
		g->ptr.state &= ~BROWSER_MOUSE_PRESS_2;
		mousedispatch(g, g->ptr.state | BROWSER_MOUSE_DRAG_2);
		g->ptr.state |= BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
	}
	mousefocus(g, false);
	mousedispatch(g, g->ptr.state);
}

void
gui_window_key(struct gui_window *g, xkb_keysym_t sym, bool pressed)
{
	uint32_t key = 0;
	browser_mouse_state mods;
	struct element *e;

	if (!pressed)
		return;

	mods = platform_window_get_mods(g->platform);

	if (sym == XKB_KEY_l && mods & BROWSER_MOUSE_MOD_2) {
		g->kbd.focus = UI_URL;
		textarea_keypress(g->url, NS_KEY_SELECT_ALL);
		return;
	}

	switch (sym) {
	case XKB_KEY_Delete:
		key = NS_KEY_DELETE_RIGHT;
		break;
	case XKB_KEY_Page_Up:
		key = NS_KEY_PAGE_UP;
		break;
	case XKB_KEY_Page_Down:
		key = NS_KEY_PAGE_UP;
		break;
	case XKB_KEY_Right:
		if (mods & BROWSER_MOUSE_MOD_2)
			key = NS_KEY_LINE_END;
		else if (mods & BROWSER_MOUSE_MOD_1)
			key = NS_KEY_WORD_RIGHT;
		else
			key = NS_KEY_RIGHT;
		break;
	case XKB_KEY_Left:
		if (mods & BROWSER_MOUSE_MOD_2)
			key = NS_KEY_LINE_START;
		else if (mods & BROWSER_MOUSE_MOD_1)
			key = NS_KEY_WORD_LEFT;
		else
			key = NS_KEY_LEFT;
		break;
	case XKB_KEY_Up:
		key = NS_KEY_UP;
		break;
	case XKB_KEY_Down:
		key = NS_KEY_DOWN;
		break;
	default:
		if (mods & BROWSER_MOUSE_MOD_2) {
			switch (sym) {
			case XKB_KEY_a:
				key = NS_KEY_SELECT_ALL;
				break;
			case XKB_KEY_c:
				key = NS_KEY_COPY_SELECTION;
				break;
			case XKB_KEY_u:
				key = NS_KEY_DELETE_LINE;
				break;
			case XKB_KEY_v:
				key = NS_KEY_PASTE;
				break;
			case XKB_KEY_x:
				key = NS_KEY_CUT_SELECTION;
				break;
			case XKB_KEY_y:
				key = NS_KEY_REDO;
				break;
			case XKB_KEY_z:
				key = mods & BROWSER_MOUSE_MOD_1 ? NS_KEY_REDO : NS_KEY_UNDO;
				break;
			}
		}
		if (key == 0)
			key = xkb_keysym_to_utf32(sym);
	}

	e = &g->ui[g->kbd.focus];
	if (e->impl->key)
		e->impl->key(g, e, key);
}

void
gui_window_axis(struct gui_window *g, bool vert, int amount)
{
	struct element *e;
	struct scrollbar *s;
	int x, y, dx, dy;

	if (g->ptr.focus != UI_CONTENT)
		return;

	e = &g->ui[UI_CONTENT];
	amount *= 8;
	x = g->ptr.x - e->r.x0;
	y = g->ptr.y - e->r.y0;

	if (vert) {
		s = g->scroll.v;
		dx = 0;
		dy = amount;
	} else {
		s = g->scroll.h;
		dx = amount;
		dy = 0;
	}
	if (browser_window_scroll_at_point(g->bw, x, y, dx, dy))
		return;

	if (scrollbar_scroll(s, amount))
		platform_window_update(g->platform, &g->ui[vert ? UI_VSCROLL : UI_HSCROLL].r);
}

/**** gui_search implementation ****/
static void
search_status(bool found, void *p)
{
	LOG("search_status\n");
}

static void
search_hourglass(bool active, void *p)
{
	LOG("search_hourglass\n");
}

static void
search_add_recent(const char *string, void *p)
{
	LOG("search_add_recent\n");
}

static void
search_forward_state(bool active, void *p)
{
	LOG("search_forward_state\n");
}

static void
search_back_state(bool active, void *p)
{
	LOG("search_back_state\n");
}

static struct gui_search_table search_table = {
	.status = search_status,
	.hourglass = search_hourglass,
	.add_recent = search_add_recent,
	.forward_state = search_forward_state,
	.back_state = search_back_state,
};

struct gui_search_table *tiny_search_table = &search_table;
