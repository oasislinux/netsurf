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

#ifndef NETSURF_TINY_RENDER_H
#define NETSURF_TINY_RENDER_H 1

typedef union pixman_image pixman_image_t;

extern struct gui_layout_table *tiny_layout_table;
extern struct gui_bitmap_table *tiny_bitmap_table;
extern const struct plotter_table *tiny_plotter_table;

nserror render_init(void);
void render_finalize(void);

nserror plot_icon(const struct redraw_context *ctx, struct bitmap *bitmap, int x, int y, bool active);

#endif
