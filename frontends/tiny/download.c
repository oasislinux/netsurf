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
#include <stdlib.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/errors.h"
#include "desktop/download.h"
#include "netsurf/download.h"

#include "tiny/download.h"

struct gui_download_window {
	struct download_context *ctx;
	int fd;
};

static struct gui_download_window *
download_create(struct download_context *ctx, struct gui_window *parent)
{
	struct gui_download_window *dw;
	const char *filename;

	dw = malloc(sizeof(*dw));
	if (!dw)
		goto err0;
	filename = download_context_get_filename(ctx);
	dw->fd = creat(filename, 0666);
	if (dw->fd == -1)
		goto err1;

	return dw;

err1:
	free(dw);
err0:
	return NULL;
}

static nserror
download_data(struct gui_download_window *dw, const char *data, unsigned int size)
{
	const char *p;
	ssize_t n;

	for (p = data; size; p += n, size -= n) {
		n = write(dw->fd, p, size);
		if (n < 0)
			return NSERROR_SAVE_FAILED;
	}

	return NSERROR_OK;
}

static void
download_error(struct gui_download_window *dw, const char *msg)
{
	NSLOG(netsurf, ERROR, "download error: %s", msg);
}

static void
download_done(struct gui_download_window *dw)
{
	close(dw->fd);
	free(dw);
}

static struct gui_download_table download_table = {
	.create = download_create,
	.data = download_data,
	.error = download_error,
	.done = download_done,
};

struct gui_download_table *tiny_download_table = &download_table;
