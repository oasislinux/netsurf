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

#ifndef NETSURF_TINY_UI_H
#define NETSURF_TINY_UI_H 1

struct rect;
struct redraw_context;

void gui_window_destroy(struct gui_window *g);

void gui_window_reformat(struct gui_window *g, int w, int h);
void gui_window_redraw(struct gui_window *g, const struct rect *clip, const struct redraw_context *ctx);

void gui_window_button(struct gui_window *g, uint32_t time, int button, bool pressed);
void gui_window_motion(struct gui_window *g, int x, int y);
void gui_window_key(struct gui_window *g, uint32_t key, bool pressed);
void gui_window_axis(struct gui_window *g, bool vert, int amount);

extern struct gui_window_table *tiny_window_table;
extern struct gui_search_table *tiny_search_table;

#endif
