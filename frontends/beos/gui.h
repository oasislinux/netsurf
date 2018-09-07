/*
 * Copyright 2015 Adrián Arroyo Calle <adrian.arroyocalle@gmail.com>
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#include <stdbool.h>
#include <Application.h>
#include <FilePanel.h>
#include <Window.h>
#include <BeBuild.h>

#ifndef B_BEOS_VERSION_DANO
#define B_UI_SETTINGS_CHANGED '_UIC'
#endif

#define CALLED() fprintf(stderr, "%s()\n", __FUNCTION__);

extern bool nsbeos_done;

extern bool replicated;
int gui_init_replicant(int argc, char** argv);

extern "C" nserror gui_401login_open(struct nsurl *url, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw);

extern "C" void nsbeos_gui_poll(void);

extern char *options_file_location;

class NSBrowserApplication : public BApplication {
public:
	NSBrowserApplication();
	virtual	~NSBrowserApplication();

	virtual void	MessageReceived(BMessage *message);
	virtual void	RefsReceived(BMessage *message);
	virtual void	ArgvReceived(int32 argc, char **argv);

	virtual void	AboutRequested();
	virtual bool	QuitRequested();
};

extern BWindow *wndAbout;

extern BWindow *wndTooltip;

extern BFilePanel *wndOpenFile;

void nsbeos_pipe_message(BMessage *message, BView *_this, struct gui_window *gui);
void nsbeos_pipe_message_top(BMessage *message, BWindow *_this, struct beos_scaffolding *scaffold);

void nsbeos_gui_view_source(struct hlcache_handle *content);
image_id nsbeos_find_app_path(char *path);
char *find_resource(char *buf, const char *filename, const char *def);

void nsbeos_update_system_ui_colors(void);

/**
 * Cause an abnormal program termination.
 *
 * \note This never returns and is intended to terminate without any cleanup.
 *
 * \param error The message to display to the user.
 */
void die(const char * const error) __attribute__ ((noreturn));

/**
 * Display a warning for a serious problem (eg memory exhaustion).
 *
 * \param warning  message key for warning message
 * \param detail   additional message, or 0
 */
nserror beos_warn_user(const char *warning, const char *detail);
