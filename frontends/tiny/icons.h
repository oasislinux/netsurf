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

#ifndef NETSURF_TINY_ICONS_H
#define NETSURF_TINY_ICONS_H 1

#include "utils/errors.h"

enum {
	ICON_BACK,
	ICON_FORWARD,
	ICON_HOME,
	ICON_STOP,
	ICON_RELOAD,
	ICON_UP,
	ICON_DOWN,

	NUM_ICONS,
};

nserror icons_init(void);

extern struct bitmap *tiny_icons[NUM_ICONS];

#endif

