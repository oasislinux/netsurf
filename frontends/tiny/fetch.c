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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "utils/errors.h"
#include "utils/file.h"
#include "utils/filepath.h"
#include "utils/nsurl.h"
#include "netsurf/fetch.h"

extern char **respaths;

static const char *
fetch_filetype(const char *unix_path)
{
	int l;

	l = strlen(unix_path);
	if (2 < l && strcasecmp(unix_path + l - 3, "css") == 0)
		return "text/css";
	if (2 < l && strcasecmp(unix_path + l - 3, "svg") == 0)
		return "image/svg+xml";
	// TODO: more mimetypes
	return "text/html";
}

static nsurl *
get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	nsurl *url = NULL;

	netsurf_path_to_nsurl(filepath_sfind(respaths, buf, path), &url);
	return url;
}

static struct gui_fetch_table fetch_table = {
	.filetype = fetch_filetype,
	.get_resource_url = get_resource_url,
	/* get_resource_data */
	/* release_resource_data */
	/* mimetype */
};

struct gui_fetch_table *tiny_fetch_table = &fetch_table;
