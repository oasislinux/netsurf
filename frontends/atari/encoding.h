/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_ENCODING_H
#define NS_ATARI_ENCODING_H

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>

#include "utils/utf8.h"

extern struct gui_utf8_table *atari_utf8_table;

nserror utf8_to_local_encoding(const char *string, size_t len, char **result);
nserror utf8_from_local_encoding(const char *string, size_t len, char **result);

int atari_to_ucs4( unsigned char atarichar);

#endif
