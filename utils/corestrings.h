/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
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

/** \file
 * Useful interned string pointers (interface).
 */

#ifndef NETSURF_UTILS_CORESTRINGS_H_
#define NETSURF_UTILS_CORESTRINGS_H_

#include <libwapcaplet/libwapcaplet.h>
#include "utils/errors.h"

nserror corestrings_init(void);
void corestrings_fini(void);

/* lwc_string strings */
extern lwc_string *corestring_lwc_a;
extern lwc_string *corestring_lwc_abscenter;
extern lwc_string *corestring_lwc_absmiddle;
extern lwc_string *corestring_lwc_align;
extern lwc_string *corestring_lwc_applet;
extern lwc_string *corestring_lwc_base;
extern lwc_string *corestring_lwc_baseline;
extern lwc_string *corestring_lwc_body;
extern lwc_string *corestring_lwc_bottom;
extern lwc_string *corestring_lwc_button;
extern lwc_string *corestring_lwc_caption;
extern lwc_string *corestring_lwc_center;
extern lwc_string *corestring_lwc_checkbox;
extern lwc_string *corestring_lwc_circle;
extern lwc_string *corestring_lwc_col;
extern lwc_string *corestring_lwc_default;
extern lwc_string *corestring_lwc_div;
extern lwc_string *corestring_lwc_embed;
extern lwc_string *corestring_lwc_file;
extern lwc_string *corestring_lwc_font;
extern lwc_string *corestring_lwc_h1;
extern lwc_string *corestring_lwc_h2;
extern lwc_string *corestring_lwc_h3;
extern lwc_string *corestring_lwc_h4;
extern lwc_string *corestring_lwc_h5;
extern lwc_string *corestring_lwc_h6;
extern lwc_string *corestring_lwc_head;
extern lwc_string *corestring_lwc_hidden;
extern lwc_string *corestring_lwc_hr;
extern lwc_string *corestring_lwc_html;
extern lwc_string *corestring_lwc_iframe;
extern lwc_string *corestring_lwc_image;
extern lwc_string *corestring_lwc_img;
extern lwc_string *corestring_lwc_input;
extern lwc_string *corestring_lwc_justify;
extern lwc_string *corestring_lwc_left;
extern lwc_string *corestring_lwc_link;
extern lwc_string *corestring_lwc_middle;
extern lwc_string *corestring_lwc_multipart_form_data;
extern lwc_string *corestring_lwc_object;
extern lwc_string *corestring_lwc_p;
extern lwc_string *corestring_lwc_password;
extern lwc_string *corestring_lwc_poly;
extern lwc_string *corestring_lwc_polygon;
extern lwc_string *corestring_lwc_post;
extern lwc_string *corestring_lwc_radio;
extern lwc_string *corestring_lwc_rect;
extern lwc_string *corestring_lwc_rectangle;
extern lwc_string *corestring_lwc_reset;
extern lwc_string *corestring_lwc_right;
extern lwc_string *corestring_lwc_submit;
extern lwc_string *corestring_lwc_table;
extern lwc_string *corestring_lwc_tbody;
extern lwc_string *corestring_lwc_td;
extern lwc_string *corestring_lwc_text;
extern lwc_string *corestring_lwc_textarea;
extern lwc_string *corestring_lwc_texttop;
extern lwc_string *corestring_lwc_tfoot;
extern lwc_string *corestring_lwc_th;
extern lwc_string *corestring_lwc_thead;
extern lwc_string *corestring_lwc_title;
extern lwc_string *corestring_lwc_top;
extern lwc_string *corestring_lwc_tr;
extern lwc_string *corestring_lwc__blank;
extern lwc_string *corestring_lwc__parent;
extern lwc_string *corestring_lwc__self;
extern lwc_string *corestring_lwc__top;

struct dom_string;

/* dom_string strings */
extern struct dom_string *corestring_dom_a;
extern struct dom_string *corestring_dom_align;
extern struct dom_string *corestring_dom_area;
extern struct dom_string *corestring_dom_background;
extern struct dom_string *corestring_dom_bgcolor;
extern struct dom_string *corestring_dom_border;
extern struct dom_string *corestring_dom_bordercolor;
extern struct dom_string *corestring_dom_cellpadding;
extern struct dom_string *corestring_dom_cellspacing;
extern struct dom_string *corestring_dom_color;
extern struct dom_string *corestring_dom_cols;
extern struct dom_string *corestring_dom_content;
extern struct dom_string *corestring_dom_coords;
extern struct dom_string *corestring_dom_height;
extern struct dom_string *corestring_dom_href;
extern struct dom_string *corestring_dom_hreflang;
extern struct dom_string *corestring_dom_hspace;
extern struct dom_string *corestring_dom_http_equiv;
extern struct dom_string *corestring_dom_id;
extern struct dom_string *corestring_dom_link;
extern struct dom_string *corestring_dom_map;
extern struct dom_string *corestring_dom_media;
extern struct dom_string *corestring_dom_name;
extern struct dom_string *corestring_dom_nohref;
extern struct dom_string *corestring_dom_rect;
extern struct dom_string *corestring_dom_rel;
extern struct dom_string *corestring_dom_rows;
extern struct dom_string *corestring_dom_size;
extern struct dom_string *corestring_dom_sizes;
extern struct dom_string *corestring_dom_shape;
extern struct dom_string *corestring_dom_src;
extern struct dom_string *corestring_dom_target;
extern struct dom_string *corestring_dom_text;
extern struct dom_string *corestring_dom_text_javascript;
extern struct dom_string *corestring_dom_type;
extern struct dom_string *corestring_dom_valign;
extern struct dom_string *corestring_dom_vlink;
extern struct dom_string *corestring_dom_vspace;
extern struct dom_string *corestring_dom_width;

#endif