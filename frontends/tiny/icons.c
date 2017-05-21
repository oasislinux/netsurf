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

#include <pixman.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <limits.h>
#include <string.h>

#include "utils/filepath.h"

#include "tiny/icons.h"

extern char **respaths;

struct bitmap *tiny_icons[NUM_ICONS];

static nserror
loadicon(const char *name, pixman_image_t **image)
{
	char path[PATH_MAX];
	FILE *f;
	uint32_t hdr[4];
	uint32_t w, h, *c;
	uint16_t px[4];
	int x, y;
	nserror err;

	if (!filepath_sfind(respaths, path, name)) {
		err = NSERROR_NOT_FOUND;
		goto err0;
	}
	f = fopen(path, "r");
	if (!f) {
		err = NSERROR_NOT_FOUND;
		goto err0;
	}
	if (fread(hdr, sizeof(hdr), 1, f) != 1) {
		err = NSERROR_INVALID;
		goto err1;
	}
	if (memcmp(hdr, "farbfeld", 8) != 0) {
		err = NSERROR_INVALID;
		goto err1;
	}
	w = ntohl(hdr[2]);
	h = ntohl(hdr[3]);
	*image = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, w, h, NULL, 0);
	if (!*image) {
		err = NSERROR_NOMEM;
		goto err1;
	}
	c = pixman_image_get_data(*image);
	for (y = 0; y < h; ++y) {
		for (x = 0; x < w; ++x, ++c) {
			if (fread(px, sizeof(px[0]), 4, f) != 4) {
				err = NSERROR_INVALID;
				goto err2;
			}
			*c = ((ntohs(px[3]) / 0x101) << 24) |
			     ((ntohs(px[0]) / 0x101) << 16) |
			     ((ntohs(px[1]) / 0x101) << 8) |
			     (ntohs(px[2]) / 0x101);
		}
	}
	fclose(f);

	return NSERROR_OK;

err2:
	pixman_image_unref(*image);
err1:
	fclose(f);
err0:
	return -1;
}

nserror icons_init(void)
{
	nserror err;

	err = loadicon("icons/back.ff", (void *)&tiny_icons[ICON_BACK]);
	if (err)
		return err;
	err = loadicon("icons/forward.ff", (void *)&tiny_icons[ICON_FORWARD]);
	if (err)
		return err;
	err = loadicon("icons/home.ff", (void *)&tiny_icons[ICON_HOME]);
	if (err)
		return err;
	err = loadicon("icons/stop.ff", (void *)&tiny_icons[ICON_STOP]);
	if (err)
		return err;
	err = loadicon("icons/reload.ff", (void *)&tiny_icons[ICON_RELOAD]);
	if (err)
		return err;

	return NSERROR_OK;
}
