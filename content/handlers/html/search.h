/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * Interface to HTML searching.
 */

#ifndef NETSURF_HTML_SEARCH_H
#define NETSURF_HTML_SEARCH_H

#include <ctype.h>
#include <string.h>

#include "desktop/search.h"

struct search_context;

/**
 * create a search_context
 *
 * \param c       The content the search_context is connected to
 * \param type    The content type of c
 * \param context A context pointer passed to the provider routines.
 * \return A new search context or NULL on error.
 */
struct search_context *search_create_context(struct content *c,
				      content_type type, void *context);

/**
 * Ends the search process, invalidating all state
 * freeing the list of found boxes
 */
void search_destroy_context(struct search_context *context);

/**
 * Begins/continues the search process
 *
 * \note that this may be called many times for a single search.
 *
 * \param context The search context in use.
 * \param flags   The flags forward/back etc
 * \param string  The string to match
 */
void search_step(struct search_context *context, search_flags_t flags,
		const char * string);

/**
 * Specifies whether all matches or just the current match should
 * be highlighted in the search text.
 */
void search_show_all(bool all, struct search_context *context);

/**
 * Determines whether any portion of the given text box should be
 * selected because it matches the current search string.
 *
 * \param c            The content to hilight within.
 * \param start_offset byte offset within text of string to be checked
 * \param end_offset   byte offset within text
 * \param start_idx    byte offset within string of highlight start
 * \param end_idx      byte offset of highlight end
 * \param context      The search context to hilight entries from.
 * \return true iff part of the box should be highlighted
 */
bool search_term_highlighted(struct content *c,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx,
		struct search_context *context);

#endif
