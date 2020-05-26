/*
 * Copyright 2018 Vincent Sanders <vince@netsurf-browser.org>
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

#ifndef NS_MONKEY_OUTPUT_H
#define NS_MONKEY_OUTPUT_H

enum monkey_output_type {
	MOUT_DIE,
	MOUT_ERROR,
	MOUT_WARNING,
	MOUT_GENERIC,
	MOUT_WINDOW,
	MOUT_LOGIN,
	MOUT_DOWNLOAD,
	MOUT_PLOT,
};

int moutf(enum monkey_output_type mout_type, const char *fmt, ...);

#endif
