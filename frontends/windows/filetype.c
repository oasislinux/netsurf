/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
 * Fetch operation implementation for win32
 */

#include <stdlib.h>
#include <string.h>

#include "utils/log.h"
#include "content/fetch.h"
#include "netsurf/fetch.h"

#include "windows/filetype.h"

/**
 * determine the MIME type of a local file.
 *
 * \param unix_path The unix style path to the file.
 * \return The mime type of the file.
 */
static const char *fetch_filetype(const char *unix_path)
{
	int l;
	NSLOG(netsurf, INFO, "unix path %s", unix_path);
	l = strlen(unix_path);
	if (2 < l && strcasecmp(unix_path + l - 3, "css") == 0)
		return "text/css";
	if (2 < l && strcasecmp(unix_path + l - 3, "jpg") == 0)
		return "image/jpeg";
	if (3 < l && strcasecmp(unix_path + l - 4, "jpeg") == 0)
		return "image/jpeg";
	if (2 < l && strcasecmp(unix_path + l - 3, "gif") == 0)
		return "image/gif";
	if (2 < l && strcasecmp(unix_path + l - 3, "png") == 0)
		return "image/png";
	if (2 < l && strcasecmp(unix_path + l - 3, "jng") == 0)
		return "image/jng";
	if (2 < l && strcasecmp(unix_path + l - 3, "svg") == 0)
		return "image/svg";
	if (2 < l && strcasecmp(unix_path + l - 3, "bmp") == 0)
		return "image/x-ms-bmp";
	return "text/html";
}

/** win32 fetch operation table */
static struct gui_fetch_table fetch_table = {
	.filetype = fetch_filetype,
};

struct gui_fetch_table *win32_fetch_table = &fetch_table;
