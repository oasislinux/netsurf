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

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "utils/errors.h"
#include "utils/file.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "netsurf/browser_window.h"
#include "netsurf/cookie_db.h"
#include "netsurf/misc.h"
#include "netsurf/netsurf.h"
#include "desktop/searchweb.h"

#include "tiny/download.h"
#include "tiny/fetch.h"
#include "tiny/icons.h"
#include "tiny/platform.h"
#include "tiny/render.h"
#include "tiny/schedule.h"
#include "tiny/ui.h"

char **respaths;

static char *confighome;

static void
die(const char *error)
{
	fprintf(stderr, "%s", error);
	exit(1);
}

static nserror
tiny_warning(const char *warning, const char *detail)
{
	return NSERROR_OK;
}

static void
tiny_quit(void)
{
	urldb_save_cookies(nsoption_charp(cookie_jar));
}

static nserror
tiny_launch_url(struct nsurl *url)
{
	NSLOG(netsurf, DEBUG, "launch url %s\n", nsurl_access(url));
	return NSERROR_NO_FETCH_HANDLER;
}

static struct gui_misc_table tiny_misc_table = {
	.schedule = tiny_schedule,
	.warning = tiny_warning,
	.quit = tiny_quit,
	.launch_url = tiny_launch_url,
	/* cert_verify */
	/* login */
	/* pdf_password */
};

static nserror
setdefaults(struct nsoption_s *defaults)
{
	char *name;

	name = NULL;
	netsurf_mkpath(&name, NULL, 2, confighome, "Cookies");
	if (name)
		nsoption_setnull_charp(cookie_file, name);

	name = NULL;
	netsurf_mkpath(&name, NULL, 2, confighome, "Cookies");
	if (name)
		nsoption_setnull_charp(cookie_jar, name);

	if (!nsoption_charp(cookie_file) || !nsoption_charp(cookie_jar))
		return NSERROR_BAD_PARAMETER;

	return NSERROR_OK;
}

static nserror
initrespaths(void)
{
	nserror err;
	char *home, *xdgconfighome, buf[PATH_MAX];
	size_t len;

	xdgconfighome = getenv("XDG_CONFIG_HOME");
	if (xdgconfighome) {
		err = netsurf_mkpath(&confighome, &len, 4, xdgconfighome, "netsurf", "/");
	} else {
		home = getenv("HOME");
		if (!home)
			return NSERROR_NOT_DIRECTORY;
		err = netsurf_mkpath(&confighome, &len, 4, home, ".config", "netsurf", "/");
	}
	if (err != NSERROR_OK)
		return err;
	err = netsurf_mkdir_all(confighome);
	if (err != NSERROR_OK) {
		free(confighome);
		return err;
	}
	confighome[len - 1] = '\0';
	if (snprintf(buf, sizeof(buf), "%s:${NETSURFRES}:" TINY_RESPATH, confighome) >= sizeof(buf))
		return NSERROR_NOSPACE;
	respaths = filepath_path_to_strvec(buf);
	if (!respaths)
		return NSERROR_NOMEM;

	return err;
}

int
main(int argc, char *argv[])
{
	nserror err;
	char *addr;
	nsurl *url;
	struct netsurf_table tiny_table = {
		.misc = &tiny_misc_table,
		.window = tiny_window_table,
		.download = tiny_download_table,
		.clipboard = tiny_clipboard_table,
		.fetch = tiny_fetch_table,
		/* file */
		/* utf8 */
		.search = tiny_search_table,
		/* search_web */
		/* llcache */
		.bitmap = tiny_bitmap_table,
		.layout = tiny_layout_table,
	};

	err = netsurf_register(&tiny_table);
	if (err != NSERROR_OK)
		die("NetSurf operation table failed registration\n");

	err = nslog_init(NULL, &argc, argv);
	if (err != NSERROR_OK)
		die("failed to initialize logging\n");

	err = initrespaths();
	if (err != NSERROR_OK)
		die("failed to initialize resource path\n");

	err = nsoption_init(setdefaults, &nsoptions, &nsoptions_default);
	if (err != NSERROR_OK)
		die("failed to initialize options\n");

	err = nsoption_commandline(&argc, argv, nsoptions);
	if (err != NSERROR_OK)
		die("bad arguments\n");

	err = netsurf_init(NULL);
	if (err != NSERROR_OK)
		die("failed to initialize NetSurf\n");

	err = search_web_init(filepath_find(respaths, "SearchEngines"));
	if (err != NSERROR_OK)
		die("failed to initialize web search\n");

	urldb_load_cookies(nsoption_charp(cookie_file));

	err = icons_init();
	if (err != NSERROR_OK)
		die("failed to initialize icons\n");

	err = render_init();
	if (err != NSERROR_OK)
		die("failed to initialize renderer\n");

	if (argc > 1) {
		struct stat st;
		if (stat(argv[1], &st) == 0) {
			char buf[PATH_MAX + 7] = "file://";

			if (!realpath(argv[1], buf + 7))
				die("failed to locate local path address");
			addr = strdup(buf);
		} else {
			addr = strdup(argv[1]);
		}
	} else if (nsoption_charp(homepage_url)) {
		addr = strdup(nsoption_charp(homepage_url));
	} else {
		addr = strdup(NETSURF_HOMEPAGE);
	}
	if (!addr)
		die("failed to allocate address\n");

	err = nsurl_create(addr, &url);
	if (err != NSERROR_OK)
		die("failed to create url\n");

	err = platform_init();
	if (err != NSERROR_OK)
		die("failed to initialize platform\n");

	err = browser_window_create(BW_CREATE_HISTORY, url, NULL, NULL, NULL);
	if (err != NSERROR_OK)
		die("failed to create browser window\n");

	platform_run();

	netsurf_exit();
	nsoption_finalise(nsoptions, nsoptions_default);
	search_web_finalise();
	render_finalize();
	nslog_finalise();

	return 0;
}
