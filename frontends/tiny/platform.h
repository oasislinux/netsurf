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

#ifndef NETSURF_TINY_PLATFORM_H
#define NETSURF_TINY_PLATFORM_H 1

#include "netsurf/mouse.h"

struct rect;

nserror tiny_init(void);
void tiny_run(void);

struct platform_window *platform_window_create(struct gui_window *g);
void platform_window_update(struct platform_window *p, const struct rect *r);
browser_mouse_state platform_window_get_mods(struct platform_window *p);
void platform_window_set_title(struct platform_window *p, const char *title);
void platform_window_set_pointer(struct platform_window *p, enum gui_pointer_shape shape);

extern struct gui_clipboard_table *tiny_clipboard_table;

#endif
