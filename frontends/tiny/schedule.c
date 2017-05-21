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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "utils/errors.h"

static struct callback *schedule_list;

struct callback {
	int t;
	void (*fn)(void *p);
	void *p;

	struct callback *next;
};

static int
now(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		return -1;
	return ts.tv_sec * 1000 + ts.tv_nsec / 1e6;
}

static void
schedule_remove(void (*fn)(void *p), void *p)
{
	struct callback *cb, **last;

	for (last = &schedule_list; *last; last = &cb->next) {
		cb = *last;
		if (cb->fn == fn && cb->p == p) {
			*last = cb->next;
			free(cb);
			break;
		}
	}
}

nserror
tiny_schedule(int t, void (*fn)(void *p), void *p)
{
	struct callback *cb;

	if (t < 0) {
		schedule_remove(fn, p);
		return NSERROR_OK;
	}
	t += now();
	// TODO: handle errors
	for (cb = schedule_list; cb; cb = cb->next) {
		if (cb->fn == fn && cb->p == p) {
			cb->t = t;
			return NSERROR_OK;
		}
	}

	cb = malloc(sizeof(*cb));
	if (!cb)
		return NSERROR_NOMEM;
	cb->t = t;
	cb->fn = fn;
	cb->p = p;
	cb->next = schedule_list;
	schedule_list = cb;

	return NSERROR_OK;
}

int
schedule_run(void)
{
	struct callback *pending, *cb, **last;
	int t, left = -1;

	t = now();
	for (;;) {
		pending = NULL;
		for (last = &schedule_list; *last;) {
			cb = *last;
			if (cb->t <= t) {
				*last = cb->next;
				cb->next = pending;
				pending = cb;
			} else {
				if (left < 0 || cb->t - t < left)
					left = cb->t - t;
				last = &cb->next;
			}
		}
		if (!pending)
			break;
		do {
			cb = pending;
			cb->fn(cb->p);
			pending = cb->next;
			free(cb);
		} while (pending);
	}

	return left;
}
