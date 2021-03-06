/*
 * This file is a part of MAFW
 *
 * Copyright (C) 2007, 2008, 2009 Nokia Corporation, all rights reserved.
 *
 * Contact: Visa Smolander <visa.smolander@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "config.h"

#include <glib.h>
#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libmafw/mafw.h>
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>

#include "../upnp-source/mafw-upnp-source-didl.h"
#include "../upnp-source/mafw-upnp-source.h"
#include "../upnp-source/mafw-upnp-source-util.h"

static gchar* DIDL_ITEM = \
	"<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">" \
	 "<item id=\"18132\" refID=\"18073\" parentID=\"18131\" restricted=\"1\">" \
	  "<dc:title>Test Animals</dc:title>" \
	  "<upnp:albumArtURI>http://foo.bar.com:31337/albumArt.png</upnp:albumArtURI>" \
	  "<upnp:lyricsURI>http://foo.bar.com:31337/lyrics.txt</upnp:lyricsURI>" \
	  "<upnp:artistDiscographyURI>http://foo.bar.com:31337/disco.html</upnp:artistDiscographyURI>" \
	  "<res colorDepth=\"32\" bitrate=\"31337\" size=\"6548309\" duration=\"0:04:32.770\" protocolInfo=\"http-get:*:audio/mpeg:*\" >http://172.23.117.242:9000/disk/music/O18132.mp3</res>" \
	  "<upnp:class>object.item.audioItem.musicTrack</upnp:class>" \
	  "<foo>bar</foo>" \
	 "</item>" \
	"</DIDL-Lite>";

static gchar* DIDL_CONTAINER = \
	"<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">" \
	 "<container id=\"18131\" parentID=\"6\" childCount=\"10\" restricted=\"0\" searchable=\"1\">" \
	  "<dc:title>Velcra</dc:title>" \
	  "<res protocolInfo=\"http-get:*:audio/x-mpegurl:*\">http://172.23.117.242:9000/m3u/18131.m3u</res>" \
	  "<upnp:class>object.container.person.musicArtist</upnp:class>" \
	 "</container>" \
	"</DIDL-Lite>";

void test_didl_item_cb(GUPnPDIDLLiteParser* parser, 
		       GUPnPDIDLLiteObject* didlobject,
		       gpointer user_data)
{
	GList *resources;
	GUPnPDIDLLiteResource* res_node;
	gchar* value;
	gint type;
	gint num;
	GHashTable *mdata = mafw_metadata_new();
	GValue *val;
	gboolean is_audio, is_supported = FALSE;
	MafwUPnPSource* source;

	fail_if(didlobject == NULL, "GUPnP is %s", "broken");
	
	source = MAFW_UPNP_SOURCE(mafw_upnp_source_new("name", "uuid"));
	g_assert(source != NULL);

	value= util_create_objectid(source, didlobject);
	fail_unless(strcmp(value, "uuid::18132") == 0, "Wrong object ID");
	g_free(value);
	g_object_unref(source);

	/* Protocol info stuff */
	resources = didl_get_supported_resources(didlobject);
	fail_if(resources == NULL || resources->data == NULL,
				"Unable to get a resource node");
	fail_if(g_list_length(resources) != 1, "Resource list not correct");
	
	res_node = resources->data;

	/* Properties */
	
	is_audio = didl_check_filetype(didlobject, &is_supported);
	
	fail_if(is_audio == FALSE, "Item should be audio");
	fail_if(is_supported == FALSE, "Item should be supported");
	
	didl_get_mimetype(mdata, FALSE, TRUE, resources);
	val = mafw_metadata_first(mdata, MAFW_METADATA_KEY_MIME);
	fail_if(val == NULL);
	fail_if(strcmp(g_value_get_string(val), "audio/mpeg") != 0, "Wrong MIME: %s", value);


	didl_get_http_res_uri(mdata, resources, TRUE);
	val = mafw_metadata_first(mdata, MAFW_METADATA_KEY_URI);
	fail_if(strcmp(g_value_get_string(val),
		       "http://172.23.117.242:9000/disk/music/O18132.mp3") != 0,
		"Wrong URI: %s", g_value_get_string(val));
	
	/* Fallbacks */
	type = 0;
	value = didl_fallback(didlobject, res_node, 7, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/lyrics.txt") != 0,
		"Wrong lyrics URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);
	
	type = 0;
	value = didl_fallback(didlobject, res_node, 9, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong small album art URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(didlobject, res_node, 10, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong medium album art URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(didlobject, res_node, 11, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong large album art URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(didlobject, res_node, 12, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/disco.html") != 0,
		"Wrong artist info URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(didlobject, res_node, 13, &type);
	fail_if(strcmp(value, "31337") != 0,
		"Wrong audio bitrate: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);
	
	type = 0;
	value = didl_fallback(didlobject, res_node, 14, &type);
	fail_if(strcmp(value, "31337") != 0,
		"Wrong video bitrate: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(didlobject, res_node, 17, &type);
	fail_if(strcmp(value, "32") != 0, "Wrong BPP: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);
}

void test_didl_container_cb(GUPnPDIDLLiteParser* parser, GUPnPDIDLLiteObject* didlobject,
			    gpointer user_data)
{
	gchar* value;
	gint num;
	xmlNode* res_node;
	GHashTable *mdata = mafw_metadata_new();
	GValue *val;
	GList *resources;

	fail_if(didlobject == NULL, "GUPnP is %s", "broken");

	didl_get_mimetype(mdata, TRUE, TRUE, resources);
	val = mafw_metadata_first(mdata, MAFW_METADATA_KEY_MIME);
	fail_if(val == NULL);
	fail_if(strcmp(g_value_get_string(val), MAFW_METADATA_VALUE_MIME_CONTAINER) != 0,
		"Wrong MIME for container: %s", value);

	resources = didl_get_supported_resources(didlobject);
	fail_if(resources == NULL || resources->data == NULL,
				"Unable to get a resource node");
	fail_if(g_list_length(resources) != 1, "Resource list not correct");
}

START_TEST(test_didl_item)
{
	GUPnPDIDLLiteParser* parser;
#if !GLIB_CHECK_VERSION(2,35,0)
	g_type_init();
#endif
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	parser = gupnp_didl_lite_parser_new();
	g_signal_connect(parser, "object-available", (GCallback)test_didl_item_cb,
					NULL);
	gupnp_didl_lite_parser_parse_didl(parser, DIDL_ITEM, NULL);
	g_object_unref(parser);

}
END_TEST

START_TEST(test_didl_container)
{
	GUPnPDIDLLiteParser* parser;

#if !GLIB_CHECK_VERSION(2,35,0)
	g_type_init();
#endif
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
	parser = gupnp_didl_lite_parser_new();
	g_signal_connect(parser, "object-available", (GCallback)test_didl_container_cb,
					NULL);
	gupnp_didl_lite_parser_parse_didl(parser, DIDL_CONTAINER, NULL);
	g_object_unref(parser);

}
END_TEST

int main(void)
{
	SRunner* sr;
        TCase *tc;
        Suite *suite;
	gint nf;

        suite = suite_create("MAFW UPnP Source DIDL-Helpers");

	tc = tcase_create("DIDL-Lite helpers");
	suite_add_tcase(suite, tc);
	tcase_add_test(tc, test_didl_item);
	tcase_add_test(tc, test_didl_container);

	sr = srunner_create(suite);
	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
