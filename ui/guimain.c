/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include "rtlmptool.h"
#include "transport.h"

static GtkButton *update_button;
static GtkBox *usb_box, *com_box;
static GtkEntry *entry_vid, *entry_pid, *entry_com;
static GtkComboBoxText *combox_transport;
static GtkComboBoxText *combox_speed;
static GtkTextBuffer *output_text_buffer;
static GtkFileChooser *chooser;
static GtkProgressBar *update_progress_bar;
static gchar *firmware = "MP_LBA1127_BLE_v1.1.2.bin";
static union transport_param trans_param;
static const char *trans_name;
static int trans_speed = 115200;
static int progress = -1;

static int output_handler(void *buf)
{
	GtkTextIter	iter;

	gtk_text_buffer_get_end_iter(output_text_buffer, &iter);
	gtk_text_buffer_insert (output_text_buffer, &iter, buf, -1);
	free(buf);
	return 0;
}

static int progress_handler(void *buf)
{
	if (progress >= 0) {
		gtk_progress_bar_set_fraction(update_progress_bar, progress / 100.0);
		return 1;
	}

	return 0;
}

static int update_button_sensitive(void *arg)
{
	gtk_widget_set_sensitive (GTK_WIDGET(update_button), TRUE);
	return 0;
}

void gtk_text_printf(const char *fmt, ...)
{
	va_list va;
	char *buf = malloc(1024);

	va_start(va, fmt);
	vsnprintf(buf, 1024, fmt, va);	
	va_end(va);

	g_idle_add(output_handler, buf);
}

static void *update_handler(void *arg)
{
	int rc;
	struct transport *transport;

	gtk_text_printf("Use the %s interface to upgrade %s\n", trans_name, firmware);

	transport = transport_open(trans_name, &trans_param);
	if (transport == NULL) {
		gtk_text_printf("Unable to open transport: %s\n", strerror(errno));
		goto quit;
	}

	progress = 0;

	g_idle_add(progress_handler, NULL);
	rc = rtlmptool_download_firmware(transport, trans_speed,
		"image/firmware0.bin", firmware, &progress);
	transport_close(transport);

	if (rc != 0) {
		gtk_text_printf("Update %s failure: %s\n", firmware, strerror(errno));
	} else {
		gtk_text_printf("Update Finish\n");		
	}

	progress = -1;
quit:
	g_idle_add(update_button_sensitive, NULL);
	return NULL;
}

void on_update_btn_clicked(void)
{
	pthread_t tid;
	uint16_t vid, pid;

	vid = strtol(gtk_entry_get_text(entry_vid), NULL, 16);
	pid = strtol(gtk_entry_get_text(entry_pid), NULL, 16);

	trans_name = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combox_transport));
	trans_speed = strtol(gtk_combo_box_text_get_active_text(combox_speed), NULL, 0);

	gtk_progress_bar_set_fraction(update_progress_bar, 0.0);
	if (!strcmp(trans_name, TRANSPORT_IFACE_LIBUSB)) {
		trans_param.libusb.vid = vid;
		trans_param.libusb.pid = pid;
		trans_param.libusb.flags = 0x01;
		trans_param.libusb.iface = 0x00;
	} else if (!strcmp(trans_name, TRANSPORT_IFACE_HIDAPI)) {
		trans_param.hidapi.vid = vid;
		trans_param.hidapi.pid = pid;
	} else if(!strcmp(trans_name, TRANSPORT_IFACE_SERAIL)) {
		const char *tty_name = "/dev/ttyS0";
		tty_name = gtk_entry_get_text(entry_com);
		trans_param.serial.tty = tty_name;
		trans_param.serial.speed = 115200;
	} else {
		gtk_text_printf("Unsupported transport type %s\n", trans_name);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET(update_button), FALSE);
	pthread_create(&tid, NULL, update_handler, NULL);
}

void on_firmware_chooser_file_set(void)
{
	firmware = gtk_file_chooser_get_filename(chooser);
	gtk_text_printf("Select firmware %s\n", firmware);
}

void on_combo_box_transport_changed(void)
{
	trans_name = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combox_transport));
	if (!strcmp(trans_name, TRANSPORT_IFACE_SERAIL)) {
		gtk_widget_hide(GTK_WIDGET(usb_box));
		gtk_widget_show(GTK_WIDGET(com_box));
	} else {               
		gtk_widget_hide(GTK_WIDGET(com_box));
		gtk_widget_show(GTK_WIDGET(usb_box));
	}
}

int main(int argc, char **argv)
{
	GtkBuilder *builder;
	GtkApplicationWindow *window;

	gtk_init(&argc, &argv);

	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, "gui.glade", NULL);
	gtk_builder_add_callback_symbols(builder,
		"on_firmware_chooser_file_set", on_firmware_chooser_file_set,
		"on_update_btn_clicked", on_update_btn_clicked,
		"on_combo_box_transport_changed", on_combo_box_transport_changed,
		NULL);
	gtk_builder_connect_signals(builder, NULL);

	usb_box = GTK_BOX(gtk_builder_get_object(builder, "usb_box"));
	com_box = GTK_BOX(gtk_builder_get_object(builder, "com_box"));
	entry_vid = GTK_ENTRY(gtk_builder_get_object(builder, "entry_vid"));
	entry_pid = GTK_ENTRY(gtk_builder_get_object(builder, "entry_pid"));
	entry_com = GTK_ENTRY(gtk_builder_get_object(builder, "entry_com"));
	chooser = GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "firmware_chooser"));
	output_text_buffer = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "output_text_buffer"));
	combox_transport = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_box_transport"));
	combox_speed = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_box_speed"));
	update_progress_bar = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "update_progress_bar"));
	update_button = GTK_BUTTON(gtk_builder_get_object(builder, "update_button"));

	assert(usb_box && com_box);
	assert(output_text_buffer);
	assert(entry_pid && entry_vid && entry_com);
	assert(combox_speed && combox_transport);

	window = GTK_APPLICATION_WINDOW(gtk_builder_get_object(builder, "window"));
	gtk_widget_show_all(GTK_WIDGET(window));
	gtk_widget_hide(GTK_WIDGET(com_box));

	gtk_main();

	return 0;
}
