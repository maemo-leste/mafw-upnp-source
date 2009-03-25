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

void test_didl_item_cb(GUPnPDIDLLiteParser* parser, xmlNode* node,
		       gpointer user_data)
{
	xmlNode* res_node;
	gchar* value;
	gint type;
	gint num;

	fail_if(node == NULL, "GUPnP is %s", "broken");

	/* Protocol info stuff */
	res_node = didl_get_http_res(node);
	fail_if(res_node == NULL, "Unable to get a resource %s", "node");

	value = didl_res_get_protocol_info(res_node, 0);
	fail_if(value == NULL);
	fail_if(strcmp(value, "http-get") != 0, "Wrong protocol: %s", value);
	g_free(value);

	value = didl_res_get_protocol_info(res_node, 1);
	fail_if(value == NULL);
	fail_if(strcmp(value, "*") != 0, "Wrong network: %s", value);
	g_free(value);

	value = didl_res_get_protocol_info(res_node, 2);
	fail_if(value == NULL);
	fail_if(strcmp(value, "audio/mpeg") != 0, "Wrong MIME: %s", value);
	g_free(value);

	value = didl_res_get_protocol_info(res_node, 3);
	fail_if(value == NULL);
	fail_if(strcmp(value, "*") != 0, "Wrong additional info: %s", value);
	g_free(value);

	/* Properties */
	value = didl_get_mimetype(node);
	fail_if(value == NULL);
	fail_if(strcmp(value, "audio/mpeg") != 0, "Wrong MIME: %s", value);
	g_free(value);

	num = didl_get_duration(node);
	fail_if(num != 272, "Wrong duration: %d", num);

	num = didl_get_childcount(node);
	fail_if(num != -1, "Childcount %d found for an item node", num);

	value = didl_get_http_res_uri(node);
	fail_if(strcmp(value,
		       "http://172.23.117.242:9000/disk/music/O18132.mp3") != 0,
		"Wrong URI: %s", value);
	g_free(value);

	value = didl_get_thumbnail_uri(node);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong thumbnail URI: %s", value);
	g_free(value);

	value = didl_get_album_art_uri(node);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong album art URI: %s", value);
	g_free(value);
	
	/* Fallbacks */
	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_LYRICS_URI, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/lyrics.txt") != 0,
		"Wrong lyrics URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);
	
	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong small album art URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_ALBUM_ART_MEDIUM_URI, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong medium album art URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_ALBUM_ART_LARGE_URI, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/albumArt.png") != 0,
		"Wrong large album art URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_ARTIST_INFO_URI, &type);
	fail_if(strcmp(value, "http://foo.bar.com:31337/disco.html") != 0,
		"Wrong artist info URI: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_AUDIO_BITRATE, &type);
	fail_if(strcmp(value, "31337") != 0,
		"Wrong audio bitrate: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);
	
	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_VIDEO_BITRATE, &type);
	fail_if(strcmp(value, "31337") != 0,
		"Wrong video bitrate: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_BITRATE, &type);
	fail_if(strcmp(value, "31337") != 0,
		"Wrong bitrate: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_FILESIZE, &type);
	fail_if(strcmp(value, "6548309") != 0, "Wrong filesize: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, MAFW_METADATA_KEY_BPP, &type);
	fail_if(strcmp(value, "32") != 0, "Wrong BPP: %s", value);
	fail_if(type != G_TYPE_INT, "Wrong type");
	g_free(value);

	type = 0;
	value = didl_fallback(node, "foo", &type);
	fail_if(strcmp(value, "bar") != 0, "Wrong foo: %s", value);
	fail_if(type != G_TYPE_STRING, "Wrong type");
	g_free(value);
}

void test_didl_container_cb(GUPnPDIDLLiteParser* parser, xmlNode* node,
			    gpointer user_data)
{
	gchar* value;
	gint num;
	xmlNode* res_node;

	fail_if(node == NULL, "GUPnP is %s", "broken");

	value = didl_get_mimetype(node);
	fail_if(strcmp(value, MAFW_METADATA_VALUE_MIME_CONTAINER) != 0,
		"Wrong MIME for container: %s", value);
	g_free(value);

	res_node = didl_get_http_res(node);
	fail_if(res_node == NULL, "Unable to get a resource %s", "node");

	num = didl_get_childcount(node);
	fail_if(num != 10, "wrong childcount: %d", num);
}

START_TEST(test_didl_item)
{
	GUPnPDIDLLiteParser* parser;

        g_type_init();
	g_thread_init(NULL);

	parser = gupnp_didl_lite_parser_new();
	gupnp_didl_lite_parser_parse_didl(parser, DIDL_ITEM,
					  test_didl_item_cb, NULL, NULL);
	g_object_unref(parser);

}
END_TEST

START_TEST(test_didl_container)
{
	GUPnPDIDLLiteParser* parser;

        g_type_init();
	g_thread_init(NULL);

	parser = gupnp_didl_lite_parser_new();
	gupnp_didl_lite_parser_parse_didl(parser, DIDL_CONTAINER,
					  test_didl_container_cb, NULL, NULL);
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
