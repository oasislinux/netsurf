/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <unistd.h>

#include "utils/log.h"
#include "utils/corestrings.h"

#include "atari/gemtk/gemtk.h"
#include "atari/findfile.h"
#include "atari/gui.h"
#include "atari/osspec.h"

char * local_file_to_url( const char * filename )
{
	#define BACKSLASH	0x5C
	char * url;

	NSLOG(netsurf, INFO, "in: %s", filename);

	if( strlen(filename) <= 2){
		return( NULL );
	}

	char * fname_local = malloc( strlen(filename)+1 );
	char * start = (char*)fname_local;
	strcpy( start, filename );

	/* convert backslashes: */
	for( unsigned int i=0; i<strlen(start); i++ ){
		if( start[i] == BACKSLASH ){
			start[i] = '/';
		}
	}

	// TODO: make file path absolute if it isn't yet.
	url = malloc( strlen(start) + FILE_SCHEME_PREFIX_LEN + 1);
	strcpy(url, FILE_SCHEME_PREFIX );
	strcat(url, start );

	free(fname_local);

	NSLOG(netsurf, INFO, "out: %s", url);

	return( url );
	#undef BACKSLASH
}


/**
 * Locate a shared resource file by searching known places in order.
 * Search order is: ./, NETSURF_GEM_RESPATH, ./$HOME/.netsurf/, $NETSURFRES/
 * (where NETSURFRES is an environment variable)
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return buf
 *
 */
#ifndef NETSURF_GEM_RESPATH
	#define NETSURF_GEM_RESPATH "./res/"
#endif

char * atari_find_resource(char *buf, const char *filename, const char *def)
{
	char *cdir = NULL;
	char t[PATH_MAX];
	NSLOG(netsurf, INFO, "%s (def: %s)", filename, def);
	strcpy(t, NETSURF_GEM_RESPATH);
	strcat(t, filename);
	NSLOG(netsurf, INFO, "checking %s", (char *)&t);
	if (gemdos_realpath(t, buf) != NULL) {
		if (access(buf, R_OK) == 0) {
			return buf;
		}
	}
	strcpy(t, "./");
	strcat(t, filename);
	NSLOG(netsurf, INFO, "checking %s", (char *)&t);
	if (gemdos_realpath(t, buf) != NULL) {
		if (access(buf, R_OK) == 0) {
			return buf;
		}
	}

	cdir = getenv("HOME");
	if (cdir != NULL) {
		strcpy(t, cdir);
		strcat(t, "/.netsurf/");
		strcat(t, filename);
		NSLOG(netsurf, INFO, "checking %s", (char *)&t);
		if (gemdos_realpath(t, buf) != NULL) {
			if (access(buf, R_OK) == 0)
				return buf;
		}
	}

	cdir = getenv("NETSURFRES");
	if (cdir != NULL) {
		if (gemdos_realpath(cdir, buf) != NULL) {
			strcat(buf, "/");
			strcat(buf, filename);
			NSLOG(netsurf, INFO, "checking %s", (char *)&t);
			if (access(buf, R_OK) == 0)
				return buf;
		}
	}
	if (def[0] == '~') {
		snprintf(t, PATH_MAX, "%s%s", getenv("HOME"), def + 1);
		NSLOG(netsurf, INFO, "checking %s", (char *)&t);
		if (gemdos_realpath(t, buf) == NULL) {
			strcpy(buf, t);
		}
	} else {
		NSLOG(netsurf, INFO, "checking %s", (char *)def);
		if (gemdos_realpath(def, buf) == NULL) {
			strcpy(buf, def);
		}
	}

	return buf;
}

/*
 * Local Variables:
 * c-basic-offset: 8
 * End:
 */
