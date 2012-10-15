/* output_gstreamer.c - Output module for GStreamer
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
 *
 * Adapted to gstreamer-0.10 2006 David Siorpaes
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gst/gst.h>

//#define ENABLE_TRACING

#include "logging.h"
#include "upnp_connmgr.h"
#include "output_module.h"
#include "output_gstreamer.h"

static void scan_caps(const GstCaps * caps)
{
	guint i;

	g_return_if_fail(caps != NULL);

	if (gst_caps_is_any(caps)) {
		return;
	}
	if (gst_caps_is_empty(caps)) {
		return;
	}

	for (i = 0; i < gst_caps_get_size(caps); i++) {
		GstStructure *structure = gst_caps_get_structure(caps, i);
		register_mime_type(gst_structure_get_name(structure));
	}

}

static void scan_pad_templates_info(GstElement *element,
				    GstElementFactory *factory)
{
	const GList *pads;
	GstPadTemplate *padtemplate;
	GstElementClass *class;

	class = GST_ELEMENT_GET_CLASS(element);

	if (!class->numpadtemplates) {
		return;
	}

	pads = class->padtemplates;
	while (pads) {
		padtemplate = (GstPadTemplate *) (pads->data);
		//GstPad *pad = (GstPad *) (pads->data);
		pads = g_list_next(pads);

		if ((padtemplate->direction == GST_PAD_SINK) &&
		    ((padtemplate->presence == GST_PAD_ALWAYS) ||
		     (padtemplate->presence == GST_PAD_SOMETIMES) ||
		     (padtemplate->presence == GST_PAD_REQUEST)) &&
		    (padtemplate->caps)) {
			scan_caps(padtemplate->caps);
		}
	}

}


static void scan_mime_list(void)
{
	GList *plugins;
	GstRegistry *registry = gst_registry_get_default();

	ENTER();

	plugins = gst_default_registry_get_plugin_list();

	while (plugins) {
		GList *features;
		GstPlugin *plugin;

		plugin = (GstPlugin *) (plugins->data);
		plugins = g_list_next(plugins);

		features =
			gst_registry_get_feature_list_by_plugin(registry,
							    gst_plugin_get_name
							    (plugin));

		while (features) {
			GstPluginFeature *feature;

			feature = GST_PLUGIN_FEATURE(features->data);

			if (GST_IS_ELEMENT_FACTORY(feature)) {
				GstElementFactory *factory;
				GstElement *element;
				factory = GST_ELEMENT_FACTORY(feature);
				element =
				    gst_element_factory_create(factory, NULL);
				if (element) {
					scan_pad_templates_info(element,
								factory);
				}
			}

			features = g_list_next(features);
		}
	}

	LEAVE();
}


static GstElement *player_ = NULL;
static char *gsuri_ = NULL;
static char *gs_next_uri_ = NULL;
static GstState player_state_ = GST_STATE_NULL;

static done_cb play_done_callback_ = NULL;

static void output_gstreamer_set_next_uri(const char *uri) {
	ENTER();
	printf("%s: setting next uri to '%s'\n", __FUNCTION__, uri);
	free(gs_next_uri_);
	gs_next_uri_ = strdup(uri);
	LEAVE();
}

static void output_gstreamer_set_uri(const char *uri) {
	ENTER();
	printf("%s: setting uri to '%s'\n", __FUNCTION__, uri);
	free(gsuri_);
	gsuri_ = strdup(uri);
	LEAVE();
}

static int output_gstreamer_play(done_cb callback) {
	int result = -1;
	play_done_callback_ = callback;
	ENTER();
	if (player_state_ != GST_STATE_PAUSED) {
		if (gst_element_set_state(player_, GST_STATE_READY) ==
		    GST_STATE_CHANGE_FAILURE) {
			printf("setting play state failed\n");
			goto out;
		}
		g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
	}
	if (gst_element_set_state(player_, GST_STATE_PLAYING) ==
	    GST_STATE_CHANGE_FAILURE) {
		printf("setting play state failed\n");
		goto out;
	} 
	result = 0;
out:
	LEAVE();
	return result;
}

static int output_gstreamer_stop(void) {
	if (gst_element_set_state(player_, GST_STATE_READY) ==
	    GST_STATE_CHANGE_FAILURE) {
		return -1;
	} else {
		return 0;
	}
}

static int output_gstreamer_pause(void) {
	if (gst_element_set_state(player_, GST_STATE_PAUSED) ==
	    GST_STATE_CHANGE_FAILURE) {
		return -1;
	} else {
		return 0;
	}
}

static int output_gstreamer_seek(gint64 position_nanos) {
	if (gst_element_seek(player_, 1.0, GST_FORMAT_TIME,
			     GST_SEEK_FLAG_FLUSH,
			     GST_SEEK_TYPE_SET, position_nanos,
			     GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		return -1;
	} else {
		return 0;
	}
}


static const char *gststate_get_name(GstState state)
{
	switch(state) {
	case GST_STATE_VOID_PENDING:
		return "VOID_PENDING";
	case GST_STATE_NULL:
		return "NULL";
	case GST_STATE_READY:
		return "READY";
	case GST_STATE_PAUSED:
		return "PAUSED";
	case GST_STATE_PLAYING:
		return "PLAYING";
	default:
		return "Unknown";
	}
}

static gboolean my_bus_callback(GstBus * bus, GstMessage * msg,
				gpointer data)
{
	//GMainLoop *loop = (GMainLoop *) data;
	GstMessageType msgType;
	GstObject *msgSrc;
	gchar *msgSrcName;

	msgType = GST_MESSAGE_TYPE(msg);
	msgSrc = GST_MESSAGE_SRC(msg);
	msgSrcName = GST_OBJECT_NAME(msgSrc);

	switch (msgType) {
	case GST_MESSAGE_EOS:
		g_print("GStreamer: %s: End-of-stream\n", msgSrcName);
		if (gs_next_uri_ != NULL) {
			// If playbin does not support gapless, then this
			// will trigger (only playbin2 does).
			free(gsuri_);
			gsuri_ = gs_next_uri_;
			gs_next_uri_ = NULL;
			gst_element_set_state(player_, GST_STATE_READY);
			g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
			gst_element_set_state(player_, GST_STATE_PLAYING);
			if (play_done_callback_) {
				play_done_callback_(PLAY_STARTED_NEXT_STREAM);
			}
		} else if (play_done_callback_) {
			play_done_callback_(PLAY_STOPPED);
		}
		break;

	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *err;

		gst_message_parse_error(msg, &err, &debug);
		g_free(debug);

		g_print("GStreamer: %s: Error: %s\n", msgSrcName, err->message);
		g_error_free(err);

		break;
	}
	case GST_MESSAGE_STATE_CHANGED: {
		GstState oldstate, newstate, pending;
		gst_message_parse_state_changed(msg, &oldstate, &newstate,
						&pending);
		g_print("GStreamer: %s: State change: '%s' -> '%s', "
			"PENDING: '%s'\n", msgSrcName,
			gststate_get_name(oldstate),
			gststate_get_name(newstate),
			gststate_get_name(pending));
		player_state_ = newstate;
		break;
	}
	default:
		g_print("GStreamer: %s: unhandled message type %d (%s)\n",
		        msgSrcName, msgType, gst_message_type_get_name(msgType));
		break;
	}

	return TRUE;
}

static gchar *audiosink = NULL;
static gchar *videosink = NULL;

/* Options specific to output_gstreamer */
static GOptionEntry option_entries[] = {
        { "gstout-audiosink", 0, 0, G_OPTION_ARG_STRING, &audiosink,
          "GStreamer audio sink to use "
	  "(autoaudiosink, alsasink, osssink, esdsink, ...)",
	  NULL },
        { "gstout-videosink", 0, 0, G_OPTION_ARG_STRING, &videosink,
          "GStreamer video sink to use "
	  "(autovideosink, xvimagesink, ximagesink, ...)",
	  NULL },
        { NULL }
};


static int output_gstreamer_add_options(GOptionContext *ctx)
{
	GOptionGroup *option_group;
	ENTER();
	option_group = g_option_group_new("gstout", "GStreamer Output Options",
	                                  "Show GStreamer Output Options",
	                                  NULL, NULL);
	g_option_group_add_entries(option_group, option_entries);

	g_option_context_add_group (ctx, option_group);
	
	g_option_context_add_group (ctx, gst_init_get_option_group ());
	LEAVE();
	return 0;
}

static int output_gstreamer_get_position(gint64 *track_duration,
					 gint64 *track_pos) {
	*track_duration = 0;
	*track_pos = 0;
	GstFormat fmt = GST_FORMAT_TIME;
	int rc = 0;
	if (!gst_element_query_duration(player_, &fmt, track_duration)) {
		fprintf(stderr, "Failed to get track duration\n");
		rc = -1;
	}
	if (!gst_element_query_position(player_, &fmt, track_pos)) {
		fprintf(stderr, "Failed to get track pos\n");
		rc = -1;
	}
	return rc;
}

static void prepare_next_stream(GstElement *obj, gpointer userdata) {
	free(gsuri_);
	gsuri_ = gs_next_uri_;
	gs_next_uri_ = NULL;
	if (gsuri_ != NULL) {
		fprintf(stderr, "HZ: about-to-finish cb: setting uri %s\n",
			gsuri_);
		g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
		if (play_done_callback_) {
			play_done_callback_(PLAY_STARTED_NEXT_STREAM);
		}
	}
}

static int output_gstreamer_init(void)
{
	GstBus *bus;

	ENTER();

	scan_mime_list();

	player_ = gst_element_factory_make("playbin2", "play");

	bus = gst_pipeline_get_bus(GST_PIPELINE(player_));
	gst_bus_add_watch(bus, my_bus_callback, NULL);
	gst_object_unref(bus);

	if (audiosink != NULL) {
		GstElement *sink = NULL;
		printf("Setting audio sink to %s\n", audiosink);
		sink = gst_element_factory_make (audiosink, "sink");
		g_object_set (G_OBJECT (player_), "audio-sink", sink, NULL);
	}
	if (videosink != NULL) {
		GstElement *sink = NULL;
		printf("Setting video sink to %s\n", videosink);
		sink = gst_element_factory_make (videosink, "sink");
		g_object_set (G_OBJECT (player_), "video-sink", sink, NULL);
	}

	if (gst_element_set_state(player_, GST_STATE_READY) ==
	    GST_STATE_CHANGE_FAILURE) {
		fprintf(stderr,	"Error: pipeline doesn't want to get ready\n");
	}

	g_signal_connect(G_OBJECT(player_), "about-to-finish",
			 G_CALLBACK(prepare_next_stream), NULL);
	LEAVE();

	return 0;
}

struct output_module gstreamer_output = {
        .shortname = "gst",
	.description = "GStreamer multimedia framework",
	.init        = output_gstreamer_init,
	.add_options = output_gstreamer_add_options,
	.set_uri     = output_gstreamer_set_uri,
	.set_next_uri= output_gstreamer_set_next_uri,
	.play        = output_gstreamer_play,
	.stop        = output_gstreamer_stop,
	.pause       = output_gstreamer_pause,
	.seek        = output_gstreamer_seek,
	.get_position = output_gstreamer_get_position,
};