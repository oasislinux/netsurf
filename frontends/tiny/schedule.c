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
	struct timespec t;
	void (*fn)(void *p);
	void *p;

	struct callback *next;
};

static nserror
schedule_remove(void (*fn)(void *p), void *p)
{
	struct callback *cb, **last;

	for (last = &schedule_list; *last; last = &cb->next) {
		cb = *last;
		if (cb->fn == fn && cb->p == p) {
			*last = cb->next;
			free(cb);
			return NSERROR_OK;
		}
	}
	return NSERROR_NOT_FOUND;
}

nserror
tiny_schedule(int delay, void (*fn)(void *p), void *p)
{
	struct timespec t;
	struct callback *cb;

	if (delay < 0)
		return schedule_remove(fn, p);
	if (clock_gettime(CLOCK_MONOTONIC, &t) < 0)
		return NSERROR_UNKNOWN;
	t.tv_sec += delay / 1000;
	t.tv_nsec += (delay % 1000) * 1000000;
	if (t.tv_nsec >= 1000000000) {
		t.tv_nsec -= 1000000000;
		t.tv_sec += 1;
	}
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
	struct timespec now;
	int timeout = -1, left;

	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		return -1;
	for (;;) {
		pending = NULL;
		for (last = &schedule_list; *last;) {
			cb = *last;
			if (now.tv_sec > cb->t.tv_sec || (now.tv_sec == cb->t.tv_sec && now.tv_nsec >= cb->t.tv_nsec)) {
				*last = cb->next;
				cb->next = pending;
				pending = cb;
			} else {
				left = (cb->t.tv_sec - now.tv_sec) * 1000 + (cb->t.tv_nsec - now.tv_nsec) / 1000000;
				if (timeout < 0 || left < timeout)
					timeout = left;
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

	return timeout;
}
