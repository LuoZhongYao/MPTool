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
#include "rtlbt.h"
#include "rtlmptool.h"
#include "transport.h"

static GtkButton *update_button;
static GtkButton *start_button, *stop_button;
static GtkRadioButton *firmware_radio, *factory_radio;
static GtkBox *usb_box, *com_box, *update_box, *factory_box;
static GtkEntry *entry_vid, *entry_pid, *entry_com;
static GtkComboBoxText *combox_transport;
static GtkComboBoxText *combox_speed;
static GtkComboBoxText *combox_channel;
static GtkTextBuffer *output_text_buffer;
static GtkFileChooser *chooser;
static GtkProgressBar *update_progress_bar;
static gchar *firmware = "MP_LBA1127_BLE_v1.1.2.bin";
static union transport_param trans_param;
static const char *trans_name;
static int trans_speed = 115200;
static int progress = -1, channel = 0;
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

	transport = transport_open(trans_name, &trans_param);
	if (transport == NULL) {
		gtk_text_printf("Unable to open transport(%s): %s\n", trans_name, strerror(errno));
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

static void *single_tone_handler(void *arg)
{
	int rc;
	struct transport *transport;

	transport = transport_open(trans_name, &trans_param);
	if (transport == NULL) {
		gtk_text_printf("Unable to open transport(%s): %s\n", trans_name, strerror(errno));
		return NULL;
	}

	rtlmptoo_set_tranport(transport);
	gtk_text_printf("Start single tone, Channel %d\n", channel);
	rc = rtlbt_single_tone(channel);
	if (rc == 1) {
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);
		pthread_mutex_unlock(&mutex);
	}

	transport_close(transport);
	gtk_text_printf("Stop single tone\n");
	return NULL;
}

static int do_bg_work(void* (*handler)(void *))
{
	uint16_t vid, pid;
	pthread_t tid;
	vid = strtol(gtk_entry_get_text(entry_vid), NULL, 16);
	pid = strtol(gtk_entry_get_text(entry_pid), NULL, 16);

	trans_name = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combox_transport));
	trans_speed = strtol(gtk_combo_box_text_get_active_text(combox_speed), NULL, 0);

	if (!strcmp(trans_name, TRANSPORT_IFACE_LIBUSB)) {
		trans_param.libusb.vid = vid;
		trans_param.libusb.pid = pid;
		trans_param.libusb.flags = 0x01;
		trans_param.libusb.iface = 0x00;
		gtk_text_printf("Select libusb %04x:%04x\n", vid, pid);
	} else if (!strcmp(trans_name, TRANSPORT_IFACE_HIDAPI)) {
		trans_param.hidapi.vid = vid;
		trans_param.hidapi.pid = pid;
		gtk_text_printf("Select hidapi %04x:%04x\n", vid, pid);
	} else if(!strcmp(trans_name, TRANSPORT_IFACE_SERAIL)) {
		const char *tty_name = "/dev/ttyS0";
		tty_name = gtk_entry_get_text(entry_com);
		trans_param.serial.tty = tty_name;
		trans_param.serial.speed = 115200;
		gtk_text_printf("Select serial %s\n", tty_name);
	} else {
		gtk_text_printf("Unsupported transport type %s\n", trans_name);
		return -1;
	}

	//gtk_widget_set_sensitive (GTK_WIDGET(update_button), FALSE);
	pthread_create(&tid, NULL, handler, NULL);
	return 0;
}

void on_start_button_clicked(void)
{
	channel = strtol(gtk_combo_box_text_get_active_text(combox_channel), NULL, 0);
	if (!do_bg_work(single_tone_handler)) {
		gtk_widget_set_sensitive (GTK_WIDGET(start_button), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET(stop_button), TRUE);
	}
}

void on_stop_button_clicked(void)
{
	gtk_widget_set_sensitive (GTK_WIDGET(start_button), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET(stop_button), FALSE);
	pthread_cond_signal(&cond);
}

void on_update_btn_clicked(void)
{
	gtk_progress_bar_set_fraction(update_progress_bar, 0.0);
	if (!do_bg_work(update_handler)) {
		gtk_widget_set_sensitive (GTK_WIDGET(update_button), FALSE);
	}
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

void on_radio_firmware_update_toggled(void)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(firmware_radio))) {
		gtk_widget_show(GTK_WIDGET(update_box));
	} else {
		gtk_widget_hide(GTK_WIDGET(update_box));
	}
}

void on_radio_factory_test_toggled(void)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(factory_radio))) {
		gtk_widget_show(GTK_WIDGET(factory_box));
	} else {
		gtk_widget_hide(GTK_WIDGET(factory_box));
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
		"on_start_button_clicked", on_start_button_clicked,
		"on_stop_button_clicked", on_stop_button_clicked,
		"on_combo_box_transport_changed", on_combo_box_transport_changed,
		"on_radio_factory_test_toggled", on_radio_factory_test_toggled,
		"on_radio_firmware_update_toggled", on_radio_firmware_update_toggled,
		NULL);
	gtk_builder_connect_signals(builder, NULL);

	usb_box = GTK_BOX(gtk_builder_get_object(builder, "usb_box"));
	com_box = GTK_BOX(gtk_builder_get_object(builder, "com_box"));
	update_box = GTK_BOX(gtk_builder_get_object(builder, "update_box"));
	factory_box = GTK_BOX(gtk_builder_get_object(builder, "factory_box"));
	entry_vid = GTK_ENTRY(gtk_builder_get_object(builder, "entry_vid"));
	entry_pid = GTK_ENTRY(gtk_builder_get_object(builder, "entry_pid"));
	entry_com = GTK_ENTRY(gtk_builder_get_object(builder, "entry_com"));
	chooser = GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "firmware_chooser"));
	output_text_buffer = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "output_text_buffer"));
	combox_transport = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_box_transport"));
	combox_speed = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_box_speed"));
	combox_channel = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combox_channel"));
	update_progress_bar = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "update_progress_bar"));
	update_button = GTK_BUTTON(gtk_builder_get_object(builder, "update_button"));
	start_button = GTK_BUTTON(gtk_builder_get_object(builder, "start_button"));
	stop_button = GTK_BUTTON(gtk_builder_get_object(builder, "stop_button"));
	firmware_radio = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "radio_firmware_update"));
	factory_radio = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "radio_factory_test"));

	assert(usb_box && com_box);
	assert(output_text_buffer);
	assert(entry_pid && entry_vid && entry_com);
	assert(combox_speed && combox_transport && combox_channel);
	assert(firmware_radio && factory_radio);

	window = GTK_APPLICATION_WINDOW(gtk_builder_get_object(builder, "window"));
	gtk_widget_show_all(GTK_WIDGET(window));
	gtk_widget_hide(GTK_WIDGET(com_box));
	gtk_widget_hide(GTK_WIDGET(factory_box));

	gtk_main();

	return 0;
}
