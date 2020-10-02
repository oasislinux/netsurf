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

#include <fcntl.h>
#include <linux/input.h>
#include <linux/memfd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <pixman.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xdg-shell-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "netsurf/browser.h"
#include "netsurf/browser_window.h"
#include "netsurf/clipboard.h"
#include "netsurf/keypress.h"
#include "netsurf/mouse.h"
#include "netsurf/plotters.h"

#include "tiny/platform.h"
#include "tiny/render.h"
#include "tiny/schedule.h"
#include "tiny/ui.h"

struct eventsource {
	void (*dispatch)(struct epoll_event *ev);
};

struct selectiondata {
	struct wl_data_source *ds;
	char *buf;
	size_t len;
	int ref;
};

struct selectionreader {
	struct eventsource ev;
	struct selectiondata data;
	struct wl_data_offer *offer;
	int fd;
};

struct selectionwriter {
	struct eventsource ev;
	struct selectiondata *data;
	int fd;
	size_t pos;
};

struct wlimage {
	pixman_image_t *pixman;
	size_t size;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
};

struct wlstate {
	int epoll;
	struct eventsource source;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_shm *shm;
	struct wl_keyboard *keyboard;
	struct wl_pointer *pointer;
	struct wl_data_device_manager *datadevicemanager;
	struct wl_data_device *datadevice;
	struct xdg_wm_base *wm;

	uint32_t lastserial;

	struct platform_window *kbdfocus, *ptrfocus;

	struct {
		struct xkb_context *ctx;
		struct xkb_state *state;
		struct xkb_keymap *map;
		xkb_mod_index_t ctrl, shift, alt;
	} xkb;
	browser_mouse_state mods;

	struct {
		int delay, interval;
		uint32_t code;
		xkb_keysym_t sym;
	} repeat;

	struct {
		enum gui_pointer_shape shape;
		struct wl_surface *surface;
		struct wl_cursor_theme *theme;
		struct wl_cursor_image *image;
	} cursor;

	struct {
		struct selectiondata data;
		struct selectionreader reader;
	} selection;
};

struct platform_window {
	struct gui_window *g;  // TODO: should this be here?

	struct wl_callback *frame;

	struct wl_surface *surface;
	struct wlimage *image;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;

	struct {
		wl_fixed_t x, y;
		browser_mouse_state state;
	} ptr;

	int width, height;
	int nextwidth, nextheight;

	pixman_region32_t damage;
};

static struct wlstate *wl;
static bool running = true;

static void redraw(void *p);

static void
selectionunref(struct selectiondata *d)
{
	if (!d->buf || --d->ref > 0)
		return;
	if (d->ds)
		wl_data_source_destroy(d->ds);
	free(d->buf);
}

static void
selectionread(struct epoll_event *ev)
{
	static char buf[BUFSIZ];
	struct selectionreader *r;
	char *newbuf;
	ssize_t n;

	r = ev->data.ptr;
	n = read(r->fd, buf, sizeof(buf));
	if (n == 0) {
		wl->selection.data = r->data;
		r->data.buf = NULL;
	} else if (n > 0) {
		newbuf = realloc(r->data.buf, r->data.len + n);
		if (newbuf) {
			r->data.buf = newbuf;
			memcpy(r->data.buf + r->data.len, buf, n);
			r->data.len += n;
			return;
		}
	}

	wl_data_offer_destroy(r->offer);
	r->offer = NULL;
	close(r->fd);
	free(r->data.buf);
}

static void
selectionwrite(struct epoll_event *ev)
{
	struct selectionwriter *w;
	ssize_t n;

	w = ev->data.ptr;
	n = write(w->fd, w->data->buf + w->pos, w->data->len - w->pos);
	if (n > 0)
		w->pos += n;
	if (n == 0 && w->pos < w->data->len)
		return;
	close(w->fd);
	selectionunref(w->data);
	free(w);
}

static void
registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_compositor") == 0) {
		wl->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, MIN(version, 4));
	} else if (strcmp(interface, "wl_seat") == 0) {
		wl->seat = wl_registry_bind(reg, name, &wl_seat_interface, MIN(version, 4));
	} else if (strcmp(interface, "wl_shm") == 0) {
		wl->shm = wl_registry_bind(reg, name, &wl_shm_interface, MIN(version, 1));
	} else if (strcmp(interface, "wl_data_device_manager") == 0) {
		wl->datadevicemanager = wl_registry_bind(reg, name, &wl_data_device_manager_interface, MIN(version, 2));
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		wl->wm = wl_registry_bind(reg, name, &xdg_wm_base_interface, MIN(version, 1));
	}
}

static void
registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
}

static struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static void
frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	struct platform_window *p = data;

	wl_callback_destroy(p->frame);
	p->frame = NULL;
	if (pixman_region32_not_empty(&p->damage))
		redraw(p);
}

static struct wl_callback_listener frame_listener = {
	.done = frame_done,
};

static void
keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size)
{
	char *str;

	str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (str == MAP_FAILED)
		return;
	close(fd);

	xkb_keymap_unref(wl->xkb.map);
	wl->xkb.map = xkb_keymap_new_from_buffer(wl->xkb.ctx, str, strnlen(str, size), XKB_KEYMAP_FORMAT_TEXT_V1, 0);
	if (!wl->xkb.map)
		return;
	munmap(str, size);

	xkb_state_unref(wl->xkb.state);
	wl->xkb.state = xkb_state_new(wl->xkb.map);
	if (!wl->xkb.state)
		return;

	wl->xkb.ctrl = xkb_keymap_mod_get_index(wl->xkb.map, XKB_MOD_NAME_CTRL);
	wl->xkb.shift = xkb_keymap_mod_get_index(wl->xkb.map, XKB_MOD_NAME_SHIFT);
	wl->xkb.alt = xkb_keymap_mod_get_index(wl->xkb.map, XKB_MOD_NAME_ALT);
}

static void
keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
	struct platform_window *p = wl_surface_get_user_data(surface);

	wl->kbdfocus = p;
}

static void
keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
	struct platform_window *p;

	if (!surface)
		return;

	p = wl_surface_get_user_data(surface);
	wl->repeat.sym = XKB_KEY_NoSymbol;
	if (wl->kbdfocus == p)
		wl->kbdfocus = NULL;
}

static void
keyrepeat(void *data)
{
	struct platform_window *p = data;

	gui_window_key(p->g, wl->repeat.sym, true);
	tiny_schedule(wl->repeat.interval, keyrepeat, p);
}

static void
keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state)
{
	xkb_keysym_t sym;
	struct platform_window *p = wl->kbdfocus;

	if (!wl->xkb.state || !wl->xkb.map || !p)
		return;

	code += 8;
	sym = xkb_state_key_get_one_sym(wl->xkb.state, code);
	gui_window_key(p->g, sym, state == WL_KEYBOARD_KEY_STATE_PRESSED);
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		wl->lastserial = serial;
		if (xkb_keymap_key_repeats(wl->xkb.map, code)) {
			wl->repeat.code = code;
			wl->repeat.sym = sym;
			tiny_schedule(wl->repeat.delay, keyrepeat, p);
		}
	} else if (code == wl->repeat.code) {
		tiny_schedule(-1, keyrepeat, p);
	}
}

static void
keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
	if (!wl->xkb.state)
		return;

	xkb_state_update_mask(wl->xkb.state, depressed, latched, locked, group, 0, 0);

	wl->mods = 0;
	if (wl->xkb.shift != XKB_MOD_INVALID && xkb_state_mod_index_is_active(wl->xkb.state, wl->xkb.shift, XKB_STATE_MODS_EFFECTIVE))
		wl->mods |= BROWSER_MOUSE_MOD_1;
	if (wl->xkb.ctrl != XKB_MOD_INVALID && xkb_state_mod_index_is_active(wl->xkb.state, wl->xkb.ctrl, XKB_STATE_MODS_EFFECTIVE))
		wl->mods |= BROWSER_MOUSE_MOD_2;
	if (wl->xkb.alt != XKB_MOD_INVALID && xkb_state_mod_index_is_active(wl->xkb.state, wl->xkb.alt, XKB_STATE_MODS_EFFECTIVE))
		wl->mods |= BROWSER_MOUSE_MOD_3;
}

static void
keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
	wl->repeat.delay = delay;
	wl->repeat.interval = 1000 / rate;
}

static struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

static void
updatecursor(struct wlstate *wl)
{
	struct wl_cursor_image *image = wl->cursor.image;
	struct wl_buffer *buffer;

	if (!image)
		return;
	wl_pointer_set_cursor(wl->pointer, 0, wl->cursor.surface, image->hotspot_x, image->hotspot_y);
	buffer = wl_cursor_image_get_buffer(wl->cursor.image);
	wl_surface_damage(wl->cursor.surface, 0, 0, image->width, image->height);
	wl_surface_attach(wl->cursor.surface, buffer, 0, 0);
	wl_surface_commit(wl->cursor.surface);
}

static void
pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
	struct platform_window *p = wl_surface_get_user_data(surface);

	updatecursor(wl);
	wl->ptrfocus = p;
	gui_window_motion(p->g, wl_fixed_to_int(x), wl_fixed_to_int(y));
}

static void
pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
	struct platform_window *p = wl_surface_get_user_data(surface);

	// TODO: send button releases
	if (wl->ptrfocus == p)
		wl->ptrfocus = NULL;
}

static void
pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t fx, wl_fixed_t fy)
{
	struct platform_window *p = wl->ptrfocus;

	gui_window_motion(p->g, wl_fixed_to_int(fx), wl_fixed_to_int(fy));
}

static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	struct platform_window *p = wl->ptrfocus;
	int b;

	switch (button) {
	case BTN_LEFT:
		b = 1;
		break;
	case BTN_MIDDLE:
		b = 2;
		break;
	case BTN_RIGHT:
		b = 3;
		break;
	default:
		return;
	}

	gui_window_button(p->g, time, b, state == WL_POINTER_BUTTON_STATE_PRESSED);
}

static void
pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	struct platform_window *p = wl->ptrfocus;

	gui_window_axis(p->g, axis == WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_to_int(value));
}

static struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
};

static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !wl->keyboard) {
		wl->keyboard = wl_seat_get_keyboard(wl->seat);
		if (wl->keyboard != NULL)
			wl_keyboard_add_listener(wl->keyboard, &keyboard_listener, wl);
	}
	if (capabilities & WL_SEAT_CAPABILITY_POINTER && !wl->pointer) {
		wl->pointer = wl_seat_get_pointer(wl->seat);
		if (wl->pointer != NULL)
			wl_pointer_add_listener(wl->pointer, &pointer_listener, wl);
	}
}

static void
seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

static void
wm_base_ping(void *data, struct xdg_wm_base *wm, uint32_t serial)
{
	xdg_wm_base_pong(wm, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	.ping = wm_base_ping,
};

static void
dataoffer_offer(void *data, struct wl_data_offer *offer, const char *mimetype)
{
	if (strncmp(mimetype, "text/plain", 10) == 0)
		wl_data_offer_accept(offer, wl->lastserial, mimetype);
}

static const struct wl_data_offer_listener dataoffer_listener = {
	.offer = dataoffer_offer,
};

static void
datadevice_data_offer(void *data, struct wl_data_device *d, struct wl_data_offer *offer)
{
	wl_data_offer_add_listener(offer, &dataoffer_listener, NULL);
}

static void
datadevice_enter(void *data, struct wl_data_device *d, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *offer)
{
}

static void
datadevice_leave(void *data, struct wl_data_device *d)
{
}

static void
datadevice_motion(void *data, struct wl_data_device *d, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
}

static void
datadevice_selection(void *data, struct wl_data_device *d, struct wl_data_offer *offer)
{
	struct epoll_event ev;
	struct selectionreader *r;
	int fd[2];

	r = &wl->selection.reader;
	if (r->offer) {
		wl_data_offer_destroy(r->offer);
		close(r->fd);
		free(r->data.buf);
		r->offer = NULL;
	}
	if (!offer) {
		selectionunref(&wl->selection.data);
		wl->selection.data.buf = NULL;
		return;
	}
	if (pipe(fd) < 0) {
		wl_data_offer_destroy(offer);
		return;
	}
	wl_data_offer_receive(offer, "text/plain;charset=utf-8", fd[1]);
	close(fd[1]);

	ev.events = EPOLLIN;
	ev.data.ptr = r;
	if (epoll_ctl(wl->epoll, EPOLL_CTL_ADD, fd[0], &ev) < 0) {
		close(fd[0]);
		wl_data_offer_destroy(offer);
	}

	r->ev.dispatch = selectionread;
	r->offer = offer;
	r->fd = fd[0];
	r->data.buf = NULL;
	r->data.len = 0;
	r->data.ds = NULL;
	r->data.ref = 1;
}

static const struct wl_data_device_listener datadevice_listener = {
	.data_offer = datadevice_data_offer,
	.enter = datadevice_enter,
	.leave = datadevice_leave,
	.motion = datadevice_motion,
	.selection = datadevice_selection,
};

static void
datasource_target(void *data, struct wl_data_source *source, const char *mimetype)
{
}

static void
datasource_send(void *data, struct wl_data_source *source, const char *mimetype, int32_t fd)
{
	struct selectiondata *d;
	struct selectionwriter *w;
	struct epoll_event ev;

	d = wl_data_source_get_user_data(source);
	w = malloc(sizeof(*w));
	if (!w) {
		close(fd);
		return;
	}
	w->ev.dispatch = selectionwrite;
	w->data = d;
	w->fd = fd;
	w->pos = 0;

	ev.events = EPOLLOUT;
	ev.data.ptr = w;

	if (epoll_ctl(wl->epoll, EPOLL_CTL_ADD, fd, &ev) < 0) {
		close(fd);
		free(w);
	}

	++d->ref;
}

static void
datasource_cancelled(void *data, struct wl_data_source *source)
{
	wl->selection.data.ds = NULL;
	wl_data_source_destroy(source);
}

static const struct wl_data_source_listener datasource_listener = {
	.target = datasource_target,
	.send = datasource_send,
	.cancelled = datasource_cancelled,
};

static void
redraw(void *data)
{
	struct platform_window *p = data;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = tiny_plotter_table,
	};
	struct rect clip;
	pixman_box32_t *b;
	int n;

	if (p->frame)
		return;

	pixman_region32_intersect_rect(&p->damage, &p->damage, 0, 0, p->width, p->height);
	b = pixman_region32_rectangles(&p->damage, &n);
	for (; n; --n, ++b)
		wl_surface_damage(p->surface, b->x1, b->y1, b->x2 - b->x1, b->y2 - b->y1);

	clip.x0 = p->damage.extents.x1;
	clip.y0 = p->damage.extents.y1;
	clip.x1 = p->damage.extents.x2;
	clip.y1 = p->damage.extents.y2;
	pixman_image_set_clip_region32(p->image->pixman, &p->damage);
	render_settarget(p->image->pixman);

	gui_window_redraw(p->g, &clip, &ctx);

	p->frame = wl_surface_frame(p->surface);
	if (p->frame)
		wl_callback_add_listener(p->frame, &frame_listener, p);
	wl_surface_attach(p->surface, p->image->buffer, 0, 0);
	wl_surface_commit(p->surface);
	pixman_region32_clear(&p->damage);
}

static struct wlimage *
createimage(int w, int h)
{
	struct wlimage *i;
	void *data;
	int fd, stride;

	i = calloc(1, sizeof(*i));
	if (i == NULL)
		goto err0;
	stride = w * 4;
	i->size = h * stride;
	fd = syscall(SYS_memfd_create, "netsurf", 0);
	if (fd < 0)
		goto err1;
	if (posix_fallocate(fd, 0, i->size) < 0)
		goto err2;
	data = mmap(NULL, i->size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		goto err2;
	i->pool = wl_shm_create_pool(wl->shm, fd, i->size);
	if (i->pool == NULL)
		goto err3;
	i->buffer = wl_shm_pool_create_buffer(i->pool, 0, w, h, stride, WL_SHM_FORMAT_XRGB8888);
	if (i->buffer == NULL)
		goto err4;
	i->pixman = pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h, data, stride);
	if (i->pixman == NULL)
		goto err5;

	close(fd);
	return i;

err5:
	wl_buffer_destroy(i->buffer);
err4:
	wl_shm_pool_destroy(i->pool);
err3:
	munmap(data, i->size);
err2:
	close(fd);
err1:
	free(i);
err0:
	return NULL;
}

static void
destroyimage(struct wlimage *i)
{
	wl_buffer_destroy(i->buffer);
	wl_shm_pool_destroy(i->pool);
	munmap(pixman_image_get_data(i->pixman), i->size);
	pixman_image_unref(i->pixman);
	free(i);
}

static void
resize(void *data)
{
	struct platform_window *p = data;

	if (p->image && p->width == p->nextwidth && p->height == p->nextheight) {
		wl_surface_attach(p->surface, p->image->buffer, 0, 0);
		wl_surface_commit(p->surface);
	} else {
		if (p->image)
			destroyimage(p->image);
		p->width = p->nextwidth;
		p->height = p->nextheight;
		p->image = createimage(p->width, p->height);
		if (p->image == NULL)
			return;
		wl_surface_attach(p->surface, p->image->buffer, 0, 0);
		pixman_region32_clear(&p->damage);
		platform_window_update(p, &(struct rect){0, 0, p->width, p->height});
		gui_window_reformat(p->g, p->width, p->height);
	}
}

static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	struct platform_window *p = data;

	xdg_surface_ack_configure(xdg_surface, serial);
	tiny_schedule(0, resize, p);
}

static struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	struct platform_window *p = data;

	if (width)
		p->nextwidth = width;
	if (height)
		p->nextheight = height;
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct platform_window *p = data;

	gui_window_destroy(p->g);
}

static struct xdg_toplevel_listener toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
};

static void
wldispatch(struct epoll_event *ev)
{
	wl_display_dispatch(wl->display);
}

nserror
platform_init(void)
{
	int fd;
	struct epoll_event ev;
	nserror err;

	wl = calloc(1, sizeof(*wl));
	if (wl == NULL) {
		err = NSERROR_NOMEM;
		goto err0;
	}

	wl->epoll = epoll_create1(EPOLL_CLOEXEC);
	if (wl->epoll == -1) {
		err = NSERROR_INIT_FAILED;
		goto err1;
	}
	wl->source.dispatch = wldispatch;
	wl->selection.reader.ev.dispatch = selectionread;

	wl->xkb.ctx = xkb_context_new(0);
	if (!wl->xkb.ctx) {
		err = NSERROR_NOMEM;
		goto err2;
	}

	wl->repeat.delay = 500;
	wl->repeat.interval = 25;

	wl->display = wl_display_connect(NULL);
	if (!wl->display) {
		err = NSERROR_INIT_FAILED;
		goto err3;
	}

	fd = wl_display_get_fd(wl->display);
	ev.events = EPOLLIN;
	ev.data.ptr = &wl->source;
	if (epoll_ctl(wl->epoll, EPOLL_CTL_ADD, fd, &ev) == -1) {
		err = NSERROR_INIT_FAILED;
		goto err4;
	}

	wl->registry = wl_display_get_registry(wl->display);
	if (!wl->registry) {
		err = NSERROR_NOMEM;
		goto err4;
	}
	wl_registry_add_listener(wl->registry, &registry_listener, NULL);

	wl_display_roundtrip(wl->display);

	if (!wl->compositor || !wl->seat || !wl->shm || !wl->wm) {
		NSLOG(netsurf, ERROR, "display is missing required globals");
		err = NSERROR_INIT_FAILED;
		goto err5;
	}

	if (wl->datadevicemanager) {
		wl->datadevice = wl_data_device_manager_get_data_device(wl->datadevicemanager, wl->seat);
		if (wl->datadevice)
			wl_data_device_add_listener(wl->datadevice, &datadevice_listener, NULL);
	}

	wl_seat_add_listener(wl->seat, &seat_listener, NULL);
	xdg_wm_base_add_listener(wl->wm, &wm_base_listener, NULL);

	wl->cursor.theme = wl_cursor_theme_load(NULL, 32, wl->shm);
	if (!wl->cursor.theme) {
		err = NSERROR_INIT_FAILED;
		goto err5;
	}
	wl->cursor.surface = wl_compositor_create_surface(wl->compositor);
	if (!wl->cursor.surface) {
		err = NSERROR_NOMEM;
		goto err6;
	}

	browser_set_dpi(TINY_DPI);

	return NSERROR_OK;

err6:
	wl_cursor_theme_destroy(wl->cursor.theme);
err5:
	if (wl->compositor)
		wl_compositor_destroy(wl->compositor);
	if (wl->seat)
		wl_seat_destroy(wl->seat);
	if (wl->shm)
		wl_shm_destroy(wl->shm);
	if (wl->wm)
		xdg_wm_base_destroy(wl->wm);
	if (wl->datadevicemanager)
		wl_data_device_manager_destroy(wl->datadevicemanager);
	if (wl->datadevice)
		wl_data_device_destroy(wl->datadevice);
	wl_registry_destroy(wl->registry);
err4:
	wl_display_disconnect(wl->display);
err3:
	xkb_context_unref(wl->xkb.ctx);
err2:
	close(wl->epoll);
err1:
	free(wl);
err0:
	return err;
}

void
platform_run(void)
{
	struct epoll_event ev[32];
	int timeout, i, n;
	struct eventsource *source;

	while (running) {
		timeout = schedule_run();
		wl_display_flush(wl->display);
		n = epoll_wait(wl->epoll, ev, sizeof(ev) / sizeof(ev[0]), timeout);
		if (n <= 0)
			continue;
		for (i = 0; i < n; ++i) {
			source = ev[i].data.ptr;
			source->dispatch(&ev[i]);
		}
	}
}

void
platform_quit(void)
{
	running = false;
}

/**** platform window ****/
struct platform_window *
platform_window_create(struct gui_window *g)
{
	struct platform_window *p;

	p = malloc(sizeof(*p));
	if (!p)
		goto err0;
	p->g = g;
	p->nextwidth = 800;
	p->nextheight = 600;
	p->image = NULL;
	p->frame = NULL;
	p->surface = wl_compositor_create_surface(wl->compositor);
	if (!p->surface)
		goto err1;
	wl_surface_set_user_data(p->surface, p);
	p->xdg_surface = xdg_wm_base_get_xdg_surface(wl->wm, p->surface);
	if (!p->xdg_surface)
		goto err2;
	xdg_surface_add_listener(p->xdg_surface, &xdg_surface_listener, p);
	p->toplevel = xdg_surface_get_toplevel(p->xdg_surface);
	if (!p->toplevel)
		goto err3;
	xdg_toplevel_add_listener(p->toplevel, &toplevel_listener, p);
	wl_surface_commit(p->surface);

	wl_display_roundtrip(wl->display);

	p->width = p->nextwidth;
	p->height = p->nextheight;
	pixman_region32_init(&p->damage);
	tiny_schedule(0, resize, p);

	return p;

err3:
	xdg_surface_destroy(p->xdg_surface);
err2:
	wl_surface_destroy(p->surface);
err1:
	free(p);
err0:
	return NULL;
}

void
platform_window_destroy(struct platform_window *p)
{
	if (p->frame)
		wl_callback_destroy(p->frame);
	xdg_surface_destroy(p->xdg_surface);
	wl_surface_destroy(p->surface);
	destroyimage(p->image);
	pixman_region32_fini(&p->damage);
	free(p);

	tiny_schedule(-1, redraw, p);
	tiny_schedule(-1, resize, p);
	tiny_schedule(-1, keyrepeat, p);
	if (wl->kbdfocus == p)
		wl->kbdfocus = NULL;
	if (wl->ptrfocus == p)
		wl->ptrfocus = NULL;
}

void
platform_window_update(struct platform_window *p, const struct rect *r)
{
	if (!pixman_region32_not_empty(&p->damage))
		tiny_schedule(0, redraw, p);
	//printf("update { %d, %d; %d, %d }\n", r->x0, r->y0, r->x1, r->y1);
	pixman_region32_union_rect(&p->damage, &p->damage, r->x0, r->y0, r->x1 - r->x0, r->y1 - r->y0);
}

browser_mouse_state
platform_window_get_mods(struct platform_window *p)
{
	return wl->mods;
}

void
platform_window_set_title(struct platform_window *p, const char *title)
{
	xdg_toplevel_set_title(p->toplevel, title);
}

void
platform_window_set_pointer(struct platform_window *p, enum gui_pointer_shape shape)
{
	const char *str = NULL;
	struct wl_cursor *cursor;
	struct wl_cursor_image *image;

	if (wl->cursor.shape == shape || wl->ptrfocus != p)
		return;
	wl->cursor.shape = shape;

	switch (shape) {
	case GUI_POINTER_DEFAULT:
		str = "left_ptr";
		break;
	case GUI_POINTER_POINT:
		str = "hand2";
		break;
	case GUI_POINTER_CARET:
		str = "xterm";
		break;
	case GUI_POINTER_MENU:
		break;
	case GUI_POINTER_UP:
		str = "top_side";
		break;
	case GUI_POINTER_DOWN:
		str = "bottom_side";
		break;
	case GUI_POINTER_LEFT:
		str = "left_side";
		break;
	case GUI_POINTER_RIGHT:
		str = "right_side";
		break;
	case GUI_POINTER_RU:
		str = "top_right_corner";
		break;
	case GUI_POINTER_LD:
		str = "bottom_left_corner";
		break;
	case GUI_POINTER_LU:
		str = "top_left_corner";
		break;
	case GUI_POINTER_RD:
		str = "bottom_right_corner";
		break;
	case GUI_POINTER_CROSS:
		str = "cross";
		break;
	case GUI_POINTER_MOVE:
		str = "grabbing";
		break;
	case GUI_POINTER_WAIT:
		str = "watch";
		break;
	case GUI_POINTER_HELP:
		str = "question_arrow";
		break;
	case GUI_POINTER_NO_DROP:
		break;
	case GUI_POINTER_NOT_ALLOWED:
		break;
	case GUI_POINTER_PROGRESS:
		str = "watch";
		break;
	}
	if (!str)
		str = "left_ptr";

	cursor = wl_cursor_theme_get_cursor(wl->cursor.theme, str);
	if (!cursor)
		return;

	image = cursor->images[0];
	wl->cursor.image = image;
	updatecursor(wl);
}

/**** clipboard ****/
static void
clipboard_get(char **buf, size_t *len)
{
	struct selectiondata *d;

	d = &wl->selection.data;
	if (d->buf) {
		*buf = malloc(d->len);
		if (!*buf)
			return;
		memcpy(*buf, d->buf, d->len);
		*len = d->len;
	} else {
		*buf = NULL;
		*len = 0;
	}
}

static void
clipboard_set(const char *buf, size_t len, nsclipboard_styles styles[], int nstyles)
{
	struct selectiondata *d;

	d = &wl->selection.data;
	selectionunref(d);

	d->buf = malloc(len);
	if (!d->buf)
		return;
	memcpy(d->buf, buf, len);
	d->len = len;
	d->ds = wl_data_device_manager_create_data_source(wl->datadevicemanager);
	if (d->ds) {
		wl_data_source_add_listener(d->ds, &datasource_listener, d);
		wl_data_source_offer(d->ds, "text/plain;charset=utf-8");
		wl_data_device_set_selection(wl->datadevice, d->ds, wl->lastserial);
	}
	d->ref = 1;
}

static struct gui_clipboard_table clipboard_table = {
	.get = clipboard_get,
	.set = clipboard_set,
};

struct gui_clipboard_table *tiny_clipboard_table = &clipboard_table;

void
platform_clipboard_get(char **buf, size_t *len)
{
	clipboard_get(buf, len);
}

void
platform_clipboard_set(const char *buf, size_t len)
{
	clipboard_set(buf, len, NULL, 0);
}
