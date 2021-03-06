/*
 * Nestopia UE
 * 
 * Copyright (C) 2012-2018 R. Danbrook
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <SDL.h>

#include "nstcommon.h"
#include "cli.h"
#include "config.h"
#include "audio.h"
#include "video.h"
#include "input.h"

#include "sdlinput.h"

#include "gtkui.h"
#include "gtkui_archive.h"
#include "gtkui_callbacks.h"
#include "gtkui_config.h"
#include "gtkui_cheats.h"
#include "gtkui_dialogs.h"
#include "gtkui_input.h"

GtkWidget *gtkwindow;
static GtkWidget *menubar;
static GtkWidget *drawingarea;

static GThread *emuthread;

char iconpath[512];
char padpath[512];

extern bool (*nst_archive_select)(const char*, char*, size_t);

extern Input::Controllers *cNstPads;
extern nstpaths_t nstpaths;
int nst_quit = 0;

gpointer gtkui_emuloop(gpointer data) {
	while(!nst_quit) { nst_emuloop(); }
	g_thread_exit(emuthread);
	return NULL;
}

void gtkui_emuloop_start() {
	nst_quit = 0;
	emuthread = g_thread_new("emuloop", gtkui_emuloop, NULL);
}

void gtkui_emuloop_stop() {
	nst_quit = true;
}

void gtkui_quit() {
	gtkui_emuloop_stop();
	gtk_main_quit();
}

void gtkui_init(int argc, char *argv[]) {
	// Initialize the GTK GUI
	gtk_init(&argc, &argv);
	gtkui_create();
}

static void gtkui_glarea_realize(GtkGLArea *glarea) {
	gtk_gl_area_make_current(glarea);
	gtk_gl_area_set_has_depth_buffer(glarea, FALSE);
	nst_ogl_init();
}

static void gtkui_swapbuffers() {
	gtk_widget_queue_draw(drawingarea);
	gtk_widget_queue_draw(menubar); // Needed on some builds of GTK3
	nst_ogl_render();
	nst_emuloop();
	
	// Move this later FIXME
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_JOYHATMOTION:
			case SDL_JOYAXISMOTION:
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				nstsdl_input_process(cNstPads, event);
				break;
			default: break;
		}	
	}
}

void gtkui_state_quickload(GtkWidget *widget, gpointer userdata) {
	// Wrapper function to quickload states
	nst_state_quickload(GPOINTER_TO_INT(userdata));
}

void gtkui_state_quicksave(GtkWidget *widget, gpointer userdata) {
	// Wrapper function to quicksave states
	nst_state_quicksave(GPOINTER_TO_INT(userdata));
}

void gtkui_open_recent(GtkWidget *widget, gpointer userdata) {
	// Open a recently used item
	gchar *uri = gtk_recent_chooser_get_current_uri((GtkRecentChooser*)widget);
	nst_load(g_filename_from_uri(uri, NULL, NULL));
	gtkui_set_title(nstpaths.gamename);
	gtkui_play();
}

dimensions_t gtkui_video_get_dimensions() {
	// Return the dimensions of the current screen
	dimensions_t scrsize;
	GdkDisplay *display = gdk_display_get_default();
	GdkWindow *gdkwindow = gtk_widget_get_window(GTK_WIDGET(gtkwindow));
	GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, gdkwindow);
	GdkRectangle geom;
	gdk_monitor_get_geometry(monitor, &geom);
	scrsize.w = geom.width;
	scrsize.h = geom.height;
	return scrsize;
}

void gtkui_create() {
	// Create the GTK Window
	
	gtkui_image_paths();
	GdkPixbuf *icon = gdk_pixbuf_new_from_file(iconpath, NULL);
	
	char title[24];
	snprintf(title, sizeof(title), "Nestopia UE");
	
	gtkwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon(GTK_WINDOW(gtkwindow), icon);
	gtk_window_set_title(GTK_WINDOW(gtkwindow), title);
	gtk_window_set_resizable(GTK_WINDOW(gtkwindow), FALSE);
	
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(gtkwindow), box);
	
	// Define the menubar and menus
	menubar = gtk_menu_bar_new();
	
	// Define the File menu
	GtkWidget *filemenu = gtk_menu_new();
	GtkWidget *file = gtk_menu_item_new_with_label("File");
	GtkWidget *open = gtk_menu_item_new_with_label("Open...");
	GtkWidget *recent = gtk_menu_item_new_with_label("Open Recent");
	GtkWidget *sep_open = gtk_separator_menu_item_new();
	GtkWidget *stateload = gtk_menu_item_new_with_label("Load State...");
	GtkWidget *statesave = gtk_menu_item_new_with_label("Save State...");
	
	GtkWidget *quickload = gtk_menu_item_new_with_label("Quick Load");
	GtkWidget *qloadmenu = gtk_menu_new();
		GtkWidget *qload0 = gtk_menu_item_new_with_label("0");
		GtkWidget *qload1 = gtk_menu_item_new_with_label("1");
		GtkWidget *qload2 = gtk_menu_item_new_with_label("2");
		GtkWidget *qload3 = gtk_menu_item_new_with_label("3");
		GtkWidget *qload4 = gtk_menu_item_new_with_label("4");
	
	GtkWidget *quicksave = gtk_menu_item_new_with_label("Quick Save");
	GtkWidget *qsavemenu = gtk_menu_new();
		GtkWidget *qsave0 = gtk_menu_item_new_with_label("0");
		GtkWidget *qsave1 = gtk_menu_item_new_with_label("1");
		GtkWidget *qsave2 = gtk_menu_item_new_with_label("2");
		GtkWidget *qsave3 = gtk_menu_item_new_with_label("3");
		GtkWidget *qsave4 = gtk_menu_item_new_with_label("4");
	
	GtkWidget *sep_state = gtk_separator_menu_item_new();
	GtkWidget *palette = gtk_menu_item_new_with_label("Open Palette...");
	GtkWidget *sep_palette = gtk_separator_menu_item_new();
	GtkWidget *screenshot = gtk_menu_item_new_with_label("Screenshot...");
	GtkWidget *sep_screenshot = gtk_separator_menu_item_new();
	GtkWidget *movieload = gtk_menu_item_new_with_label("Load Movie...");
	GtkWidget *moviesave = gtk_menu_item_new_with_label("Record Movie...");
	GtkWidget *moviestop = gtk_menu_item_new_with_label("Stop Movie");
	GtkWidget *sep_movie = gtk_separator_menu_item_new();
	GtkWidget *quit = gtk_menu_item_new_with_label("Quit");
	
	// Set up the recently used items
	GtkWidget *recent_items = gtk_recent_chooser_menu_new();
	GtkRecentFilter *recent_filter = gtk_recent_filter_new();
		gtk_recent_filter_add_pattern(recent_filter, "*.nes");
		gtk_recent_filter_add_pattern(recent_filter, "*.fds");
		gtk_recent_filter_add_pattern(recent_filter, "*.unf");
		gtk_recent_filter_add_pattern(recent_filter, "*.unif");
		gtk_recent_filter_add_pattern(recent_filter, "*.nsf");
		gtk_recent_filter_add_pattern(recent_filter, "*.zip");
		gtk_recent_filter_add_pattern(recent_filter, "*.7z");
		gtk_recent_filter_add_pattern(recent_filter, "*.txz");
		gtk_recent_filter_add_pattern(recent_filter, "*.tar.xz");
		gtk_recent_filter_add_pattern(recent_filter, "*.xz");
		gtk_recent_filter_add_pattern(recent_filter, "*.tgz");
		gtk_recent_filter_add_pattern(recent_filter, "*.tar.gz");
		gtk_recent_filter_add_pattern(recent_filter, "*.gz");
		gtk_recent_filter_add_pattern(recent_filter, "*.tbz");
		gtk_recent_filter_add_pattern(recent_filter, "*.tar.bz2");
		gtk_recent_filter_add_pattern(recent_filter, "*.bz2");
	gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(recent_items), recent_filter);
	
	// Populate the File menu
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), filemenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), open);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), recent);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(recent), recent_items);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), sep_open);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), stateload);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), statesave);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quickload);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(quickload), qloadmenu);
		gtk_menu_shell_append(GTK_MENU_SHELL(qloadmenu), qload0);
		gtk_menu_shell_append(GTK_MENU_SHELL(qloadmenu), qload1);
		gtk_menu_shell_append(GTK_MENU_SHELL(qloadmenu), qload2);
		gtk_menu_shell_append(GTK_MENU_SHELL(qloadmenu), qload3);
		gtk_menu_shell_append(GTK_MENU_SHELL(qloadmenu), qload4);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quicksave);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(quicksave), qsavemenu);
		gtk_menu_shell_append(GTK_MENU_SHELL(qsavemenu), qsave0);
		gtk_menu_shell_append(GTK_MENU_SHELL(qsavemenu), qsave1);
		gtk_menu_shell_append(GTK_MENU_SHELL(qsavemenu), qsave2);
		gtk_menu_shell_append(GTK_MENU_SHELL(qsavemenu), qsave3);
		gtk_menu_shell_append(GTK_MENU_SHELL(qsavemenu), qsave4);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), sep_state);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), palette);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), sep_palette);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), screenshot);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), sep_screenshot);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), movieload);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), moviesave);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), moviestop);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), sep_movie);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quit);
	
	// Define the Emulator menu
	GtkWidget *emulatormenu = gtk_menu_new();
	GtkWidget *emu = gtk_menu_item_new_with_label("Emulator");
	GtkWidget *cont = gtk_menu_item_new_with_label("Continue");
	GtkWidget *pause = gtk_menu_item_new_with_label("Pause");
	GtkWidget *sep_pause = gtk_separator_menu_item_new();
	GtkWidget *resetsoft = gtk_menu_item_new_with_label("Reset (Soft)");
	GtkWidget *resethard = gtk_menu_item_new_with_label("Reset (Hard)");
	GtkWidget *sep_reset = gtk_separator_menu_item_new();
	GtkWidget *fullscreen = gtk_menu_item_new_with_label("Fullscreen");
	GtkWidget *sep_fullscreen = gtk_separator_menu_item_new();
	GtkWidget *diskflip = gtk_menu_item_new_with_label("Flip FDS Disk");
	GtkWidget *diskswitch = gtk_menu_item_new_with_label("Switch FDS Disk");
	GtkWidget *sep_disk = gtk_separator_menu_item_new();
	GtkWidget *cheats = gtk_menu_item_new_with_label("Cheats...");
	GtkWidget *sep_cheats = gtk_separator_menu_item_new();
	GtkWidget *configuration = gtk_menu_item_new_with_label("Configuration...");
	
	// Populate the Emulator menu
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(emu), emulatormenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), cont);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), pause);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), sep_pause);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), resetsoft);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), resethard);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), sep_reset);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), fullscreen);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), sep_fullscreen);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), diskflip);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), diskswitch);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), sep_disk);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), cheats);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), sep_cheats);
	gtk_menu_shell_append(GTK_MENU_SHELL(emulatormenu), configuration);
	
	// Define the Help menu
	GtkWidget *helpmenu = gtk_menu_new();
	GtkWidget *help = gtk_menu_item_new_with_label("Help");
	GtkWidget *about = gtk_menu_item_new_with_label("About");
	
	// Populate the Help menu
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), helpmenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(helpmenu), about);
	
	// Put the menus into the menubar
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), emu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help);
	
	// Create the DrawingArea/OpenGL context
	///drawingarea = gtk_drawing_area_new();
	drawingarea = gtk_gl_area_new();
	g_signal_connect(G_OBJECT(drawingarea), "realize", G_CALLBACK(gtkui_glarea_realize), NULL);
	g_signal_connect(G_OBJECT(drawingarea), "render", gtkui_swapbuffers, NULL);
	//gtk_widget_add_tick_callback(drawingarea, gtkui_tick, drawingarea, NULL);
	
	g_object_set_data(G_OBJECT(gtkwindow), "area", drawingarea);
	
	// Set the Drawing Area to be the size of the game output
	dimensions_t rendersize = nst_video_get_dimensions_render();
	gtk_widget_set_size_request(drawingarea, rendersize.w, rendersize.h);
	
	// Pack the box with the menubar, drawingarea, and statusbar
	gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), drawingarea, TRUE, TRUE, 0);
	
	// Make it dark if there's a dark theme
	GtkSettings *gtksettings = gtk_settings_get_default();
	g_object_set(G_OBJECT(gtksettings), "gtk-application-prefer-dark-theme", TRUE, NULL);
	
	// Set up the Drag and Drop target
	GtkTargetEntry target_entry[1];

	target_entry[0].target = (gchar*)"text/uri-list";
	target_entry[0].flags = 0;
	target_entry[0].info = 0;
	
	gtk_drag_dest_set(drawingarea, (GtkDestDefaults)(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP), 
		target_entry, sizeof(target_entry) / sizeof(GtkTargetEntry), (GdkDragAction)(GDK_ACTION_MOVE | GDK_ACTION_COPY));
	
	// Connect the signals
	g_signal_connect(G_OBJECT(drawingarea), "drag-data-received",
		G_CALLBACK(gtkui_drag_data), NULL);
	
	g_signal_connect(G_OBJECT(gtkwindow), "delete_event",
		G_CALLBACK(gtkui_quit), NULL);
	
	// File menu
	g_signal_connect(G_OBJECT(open), "activate",
		G_CALLBACK(gtkui_file_open), NULL);
	
	g_signal_connect(G_OBJECT(recent_items), "item-activated",
		G_CALLBACK(gtkui_open_recent), NULL);
	
	g_signal_connect(G_OBJECT(stateload), "activate",
		G_CALLBACK(gtkui_state_load), NULL);
	
	g_signal_connect(G_OBJECT(statesave), "activate",
		G_CALLBACK(gtkui_state_save), NULL);
	
	g_signal_connect(G_OBJECT(qload0), "activate",
		G_CALLBACK(gtkui_state_quickload), gpointer(0));
	
	g_signal_connect(G_OBJECT(qload1), "activate",
		G_CALLBACK(gtkui_state_quickload), gpointer(1));
	
	g_signal_connect(G_OBJECT(qload2), "activate",
		G_CALLBACK(gtkui_state_quickload), gpointer(2));
	
	g_signal_connect(G_OBJECT(qload3), "activate",
		G_CALLBACK(gtkui_state_quickload), gpointer(3));
	
	g_signal_connect(G_OBJECT(qload4), "activate",
		G_CALLBACK(gtkui_state_quickload), gpointer(4));
	
	g_signal_connect(G_OBJECT(qsave0), "activate",
		G_CALLBACK(gtkui_state_quicksave), gpointer(0));
	
	g_signal_connect(G_OBJECT(qsave1), "activate",
		G_CALLBACK(gtkui_state_quicksave), gpointer(1));
	
	g_signal_connect(G_OBJECT(qsave2), "activate",
		G_CALLBACK(gtkui_state_quicksave), gpointer(2));
	
	g_signal_connect(G_OBJECT(qsave3), "activate",
		G_CALLBACK(gtkui_state_quicksave), gpointer(3));
	
	g_signal_connect(G_OBJECT(qsave4), "activate",
		G_CALLBACK(gtkui_state_quicksave), gpointer(4));
	
	g_signal_connect(G_OBJECT(screenshot), "activate",
		G_CALLBACK(gtkui_screenshot_save), NULL);
	
	g_signal_connect(G_OBJECT(palette), "activate",
		G_CALLBACK(gtkui_palette_load), NULL);
	
	g_signal_connect(G_OBJECT(moviesave), "activate",
		G_CALLBACK(gtkui_movie_save), NULL);
	
	g_signal_connect(G_OBJECT(movieload), "activate",
		G_CALLBACK(gtkui_movie_load), NULL);
	
	g_signal_connect(G_OBJECT(moviestop), "activate",
		G_CALLBACK(gtkui_movie_stop), NULL);
	
	g_signal_connect(G_OBJECT(quit), "activate",
		G_CALLBACK(gtkui_quit), NULL);
	
	// Emulator menu
	g_signal_connect(G_OBJECT(cont), "activate",
		G_CALLBACK(gtkui_play), NULL);
	
	g_signal_connect(G_OBJECT(pause), "activate",
		G_CALLBACK(gtkui_pause), NULL);
	
	g_signal_connect(G_OBJECT(resetsoft), "activate",
		G_CALLBACK(gtkui_cb_reset), gpointer(0));
	
	g_signal_connect(G_OBJECT(resethard), "activate",
		G_CALLBACK(gtkui_cb_reset), gpointer(1));
	
	g_signal_connect(G_OBJECT(fullscreen), "activate",
		G_CALLBACK(gtkui_video_toggle_fullscreen), NULL);
	
	g_signal_connect(G_OBJECT(diskflip), "activate",
		G_CALLBACK(nst_fds_flip), NULL);
	
	g_signal_connect(G_OBJECT(diskswitch), "activate",
		G_CALLBACK(nst_fds_switch), NULL);
	
	g_signal_connect(G_OBJECT(cheats), "activate",
		G_CALLBACK(gtkui_cheats), NULL);
	
	g_signal_connect(G_OBJECT(configuration), "activate",
		G_CALLBACK(gtkui_config), NULL);
	
	// Help menu
	g_signal_connect(G_OBJECT(about), "activate",
		G_CALLBACK(gtkui_about), NULL);
	
	// Mouse input events
	gtk_widget_add_events(GTK_WIDGET(drawingarea), GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(GTK_WIDGET(drawingarea), GDK_BUTTON_RELEASE_MASK);
	
	gtk_widget_show_all(gtkwindow);
	
	nst_video_set_dimensions_screen(gtkui_video_get_dimensions());
	
	if (conf.video_fullscreen) { gtkui_video_toggle_fullscreen(); }
}

void gtkui_signals_init() {
	// Key translation
	if (nst_nsf()) {
		g_signal_connect(G_OBJECT(gtkwindow), "key-press-event",
			G_CALLBACK(gtkui_input_process_key_nsf), NULL);
		
		g_signal_connect(G_OBJECT(gtkwindow), "key-release-event",
			G_CALLBACK(gtkui_input_process_key_nsf), NULL);
	}
	else {
		g_signal_connect(G_OBJECT(gtkwindow), "key-press-event",
			G_CALLBACK(gtkui_input_process_key), NULL);
		
		g_signal_connect(G_OBJECT(gtkwindow), "key-release-event",
			G_CALLBACK(gtkui_input_process_key), NULL);
	}
	// Mouse translation
	g_signal_connect(G_OBJECT(drawingarea), "button-press-event",
		G_CALLBACK(gtkui_input_process_mouse), NULL);
	
	g_signal_connect(G_OBJECT(drawingarea), "button-release-event",
		G_CALLBACK(gtkui_input_process_mouse), NULL);
}

void gtkui_signals_deinit() {
	// Key translation
	g_signal_connect(G_OBJECT(gtkwindow), "key-press-event",
		gtkui_input_null, NULL);
	
	g_signal_connect(G_OBJECT(gtkwindow), "key-release-event",
		gtkui_input_null, NULL);
	
	// Mouse translation
	g_signal_connect(G_OBJECT(drawingarea), "button-press-event",
		gtkui_input_null, NULL);
	
	g_signal_connect(G_OBJECT(drawingarea), "button-release-event",
		gtkui_input_null, NULL);
}

void gtkui_resize() {
	// Resize the GTK window
	if (gtkwindow) {
		video_set_dimensions();
		dimensions_t rendersize = nst_video_get_dimensions_render();
		gtk_widget_set_size_request(drawingarea, rendersize.w, rendersize.h);
	}
}

void gtkui_set_title(const char *title) {
	gtk_window_set_title(GTK_WINDOW(gtkwindow), title);
}

void gtkui_video_toggle_fullscreen() {
	
	video_toggle_fullscreen();
	
	if (conf.video_fullscreen) {
		gtk_widget_hide(menubar);
		gtk_window_fullscreen(GTK_WINDOW(gtkwindow));
		if (nst_input_zapper_present()) {
			gtkui_cursor_set(conf.misc_disable_cursor_special ? 0 : 2);
		}
		else {gtkui_cursor_set(0); }
	}
	else {
		gtk_window_unfullscreen(GTK_WINDOW(gtkwindow));
		gtk_widget_show(menubar);
		if (nst_input_zapper_present()) {
			gtkui_cursor_set(conf.misc_disable_cursor_special ? 0 : 2);
		}
		else {gtkui_cursor_set(1); }
	}
	nst_video_set_dimensions_screen(gtkui_video_get_dimensions());
	video_init();
	gtkui_resize();
}

void gtkui_video_toggle_filter() {
	video_toggle_filter();
	gtkui_resize();
	video_init();
}

void gtkui_video_toggle_scale() {
	video_toggle_scalefactor();
	gtkui_resize();
	video_init();
}

GtkWidget *gtkui_about() {
	// Pull up the About dialog
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(iconpath, 192, 192, NULL);
	GtkWidget *aboutdialog = gtk_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(aboutdialog), GTK_WINDOW(gtkwindow));
	
	gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(aboutdialog), pixbuf);
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(aboutdialog), "Nestopia UE");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(aboutdialog), "vx.xx");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(aboutdialog), "Cycle-Accurate Nintendo Entertainment System Emulator");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(aboutdialog), "http://0ldsk00l.ca/nestopia/");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(aboutdialog), "(c) 2012-2020, R. Danbrook\n(c) 2007-2008, R. Belmont\n(c) 2003-2008, Martin Freij\n\nIcon based on art from Trollekop");
	gtk_dialog_run(GTK_DIALOG(aboutdialog));
	gtk_widget_destroy(aboutdialog);
	
	return aboutdialog;
}

void gtkui_image_paths() {
	// Set paths to SVG icons/images
	snprintf(iconpath, sizeof(iconpath), "%s/icons/hicolor/scalable/apps/nestopia.svg", DATAROOTDIR);
	snprintf(padpath, sizeof(padpath), "%s/icons/hicolor/scalable/apps/nespad.svg", DATAROOTDIR);
	
	// Load the SVG from local source dir if make install hasn't been done
	struct stat svgstat;
	if (stat(iconpath, &svgstat) == -1) {
		snprintf(iconpath, sizeof(iconpath), "icons/svg/nestopia.svg");
	}
	if (stat(padpath, &svgstat) == -1) {
		snprintf(padpath, sizeof(padpath), "icons/svg/nespad.svg");
	}
}

void gtkui_message(const char* message) {    
	GtkWidget *messagewindow = gtk_message_dialog_new(
				GTK_WINDOW(gtkwindow),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_OK,
				"%s", message);
	gtk_dialog_run(GTK_DIALOG(messagewindow));
	gtk_widget_destroy(messagewindow);
}

void gtkui_cursor_set(int curtype) {
	// Set the cursor
	GdkCursor *cursor;
	GdkDisplay *display = gdk_display_get_default();
	
	switch (curtype) {
		case 0: cursor = gdk_cursor_new_from_name(display, "none"); break;
		case 1: cursor = gdk_cursor_new_from_name(display, "default"); break;
		case 2: cursor = gdk_cursor_new_from_name(display, "crosshair"); break;
		default: cursor = gdk_cursor_new_from_name(display, "default"); break;
	}
	
	GdkWindow *gdkwindow = gtk_widget_get_window(GTK_WIDGET(drawingarea));
	gdk_window_set_cursor(gdkwindow, cursor);
	gdk_display_flush(display);
	g_object_unref(cursor);
}

void gtkui_play() {
	gtkui_signals_init();
	nst_play();
	//gtkui_emuloop_start();
	if (nst_input_zapper_present()) {
		gtkui_cursor_set(conf.misc_disable_cursor_special ? 0 : 2);
	}
	else {
		gtkui_cursor_set(conf.misc_disable_cursor ? 0 : 1);
	}
}

void gtkui_pause() {
	gtkui_signals_deinit();
	//gtkui_emuloop_stop();
	nst_pause();
}

int main(int argc, char *argv[]) {
	
	// Set up directories
	nst_set_dirs();
	
	// Set default config options
	config_set_default();
	
	// Read the config file and override defaults
	config_file_read(nstpaths.nstdir);
	
	// Handle command line arguments
	cli_handle_command(argc, argv);
	
	// Set default input keys
	gtkui_input_set_default();
	
	// Read the input config file and override defaults
	gtkui_input_config_read();
	
	// Set the video dimensions
	video_set_dimensions();
	
	// Set up callbacks
	nst_set_callbacks();
	
	// Initialize SDL Audio and Joystick
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return 1;
	}
	
	// Set archive handler function pointer
	nst_archive_select = &gtkui_archive_select;
	
	// Set audio function pointers
	audio_set_funcs();
	
	// Detect and set up Joysticks
	nstsdl_input_joysticks_detect();
	nstsdl_input_conf_defaults();
	nstsdl_input_conf_read();
	
	// Initialize and load FDS BIOS and NstDatabase.xml
	nst_fds_bios_load();
	nst_db_load();
	
	gtkui_init(argc, argv);
	
	// Load a rom from the command line
	if (argc > 1) {
		nst_load(argv[argc - 1]);
		gtkui_play();
		gtkui_set_title(nstpaths.gamename);
	}
	
	// Start GTK main loop
	gtk_main();
	
	// Remove the cartridge and shut down the NES
	nst_unload();
	
	// Unload the FDS BIOS, NstDatabase.xml, and the custom palette
	nst_db_unload();
	nst_fds_bios_unload();
	nst_palette_unload();
	
	// Deinitialize audio
	audio_deinit();
	
	// Deinitialize joysticks
	nstsdl_input_joysticks_close();
	
	// Write the input config file
	nstsdl_input_conf_write();
	
	// Write the input config file
	gtkui_input_config_write();
	
	// Write the config file
	config_file_write(nstpaths.nstdir);

	return 0;
}
