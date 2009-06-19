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

#include <libgupnp-av/gupnp-av.h>
#include <libgupnp/gupnp.h>
#include <libmafw/mafw.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "mafw-upnp-source.h"
#include "mafw-upnp-source-util.h"
#include "mafw-upnp-source-didl.h"

/**
 * util_udn_to_uuid:
 * @udn: The UDN string to convert
 *
 * Converts a UPnP UDN to a MAFW UUID that is compatible with DBus,
 * except for the length restrictions.
 *
 * Returns: A newly-created string (must be freed)
 */
gchar* util_udn_to_uuid(const gchar* udn)
{
	guint i;
	GString *uuid;

	/* Encode all non-alphanumeric characters as "_<hex><hex>".
	 * Make sure UUID doesn't begin with a digit. */
	uuid = g_string_new("_");
	for (i = 0; udn[i]; i++)
	{
		if (g_ascii_isalnum(udn[i]))
			g_string_append_c(uuid, udn[i]);
		else
			g_string_append_printf(uuid, "_%.2X", udn[i]);
	}

	return g_string_free(uuid, FALSE);
}

/**
 * util_uuid_to_udn:
 * @uuid: The UUID string to convert
 *
 * Returns the original UDN from a UUID created by util_udn_to_uuid().
 *
 * Returns: A newly-created string (must be freed)
 */
gchar* util_uuid_to_udn(const gchar* uuid)
{
	guint i;
	GString *udn;

	/* Skip the initial underscore. */
	g_return_val_if_fail(uuid != NULL, NULL);
	g_assert(uuid[0] == '_');
	uuid++;

	udn = g_string_new(NULL);
	for (i = 0; uuid[i]; i++)
	{
		if (uuid[i] == '_') {
			unsigned c;

			/* Encoded nonalphanumeric character. */
			if (sscanf(&uuid[i], "_%2X", &c) != 1)
				g_assert_not_reached();
			g_string_append_c(udn, (char)c);
			uuid += 2;
		} else {
			g_string_append_c(udn, uuid[i]);
		}
	}

	return g_string_free(udn, FALSE);
}

struct _upnp_map{
	GType gtype;
	const gchar *upnp_key;
	const gchar *mafw_key;
};

static struct _upnp_map upnpmaps[] = {
	{0, NULL, MAFW_METADATA_KEY_URI},
	{0, NULL, MAFW_METADATA_KEY_CHILDCOUNT_1},
	{0, NULL, MAFW_METADATA_KEY_MIME},
	{0, NULL, MAFW_METADATA_KEY_DURATION},
	{0, NULL, MAFW_METADATA_KEY_THUMBNAIL_URI},
	{0, NULL, MAFW_METADATA_KEY_DIDL},
	{0, NULL, MAFW_METADATA_KEY_IS_SEEKABLE},
	{G_TYPE_STRING, DIDL_LYRICS_URI, MAFW_METADATA_KEY_LYRICS_URI},
	{G_TYPE_STRING, DIDL_RES_PROTOCOL_INFO, MAFW_METADATA_KEY_PROTOCOL_INFO},
	{G_TYPE_STRING, DIDL_ALBUM_ART_URI, MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI},
	{G_TYPE_STRING, DIDL_ALBUM_ART_URI, MAFW_METADATA_KEY_ALBUM_ART_MEDIUM_URI},
	{G_TYPE_STRING, DIDL_ALBUM_ART_URI, MAFW_METADATA_KEY_ALBUM_ART_LARGE_URI},
	{G_TYPE_STRING, DIDL_DISCOGRAPHY_URI, MAFW_METADATA_KEY_ARTIST_INFO_URI},
	{G_TYPE_INT, DIDL_RES_BITRATE, MAFW_METADATA_KEY_AUDIO_BITRATE},
	{G_TYPE_INT, DIDL_RES_BITRATE, MAFW_METADATA_KEY_VIDEO_BITRATE},
	{G_TYPE_INT, DIDL_RES_BITRATE, MAFW_METADATA_KEY_BITRATE},
	{G_TYPE_INT, DIDL_RES_SIZE, MAFW_METADATA_KEY_FILESIZE},
	{G_TYPE_INT, DIDL_RES_COLORDEPTH, MAFW_METADATA_KEY_BPP},
	{G_TYPE_STRING, MAFW_METADATA_KEY_TITLE, MAFW_METADATA_KEY_TITLE},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ARTIST, MAFW_METADATA_KEY_ARTIST},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ALBUM, MAFW_METADATA_KEY_ALBUM},
	{G_TYPE_STRING, MAFW_METADATA_KEY_GENRE, MAFW_METADATA_KEY_GENRE},
	{G_TYPE_STRING, MAFW_METADATA_KEY_TRACK, MAFW_METADATA_KEY_TRACK},
	{G_TYPE_INT, MAFW_METADATA_KEY_YEAR, MAFW_METADATA_KEY_YEAR},
	{G_TYPE_INT, MAFW_METADATA_KEY_COUNT, MAFW_METADATA_KEY_COUNT},
	{G_TYPE_INT, MAFW_METADATA_KEY_PLAY_COUNT, MAFW_METADATA_KEY_PLAY_COUNT},
	{G_TYPE_STRING, MAFW_METADATA_KEY_DESCRIPTION, MAFW_METADATA_KEY_DESCRIPTION},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ENCODING, MAFW_METADATA_KEY_ENCODING},
	{G_TYPE_LONG, MAFW_METADATA_KEY_ADDED, MAFW_METADATA_KEY_ADDED},
	{G_TYPE_STRING, MAFW_METADATA_KEY_THUMBNAIL, MAFW_METADATA_KEY_THUMBNAIL},
	{G_TYPE_INT, MAFW_METADATA_KEY_RES_X, MAFW_METADATA_KEY_RES_X},
	{G_TYPE_INT, MAFW_METADATA_KEY_RES_Y, MAFW_METADATA_KEY_RES_Y},
	{G_TYPE_STRING, MAFW_METADATA_KEY_COMMENT, MAFW_METADATA_KEY_COMMENT},
	{G_TYPE_STRING, MAFW_METADATA_KEY_TAGS, MAFW_METADATA_KEY_TAGS},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ALBUM_INFO_URI, MAFW_METADATA_KEY_ALBUM_INFO_URI},
	{G_TYPE_STRING, MAFW_METADATA_KEY_LYRICS, MAFW_METADATA_KEY_LYRICS},
	{G_TYPE_INT, MAFW_METADATA_KEY_RATING, MAFW_METADATA_KEY_RATING},
	{G_TYPE_STRING, MAFW_METADATA_KEY_COMPOSER, MAFW_METADATA_KEY_COMPOSER},
	{G_TYPE_STRING, MAFW_METADATA_KEY_FILENAME, MAFW_METADATA_KEY_FILENAME},
	{G_TYPE_STRING, MAFW_METADATA_KEY_COPYRIGHT, MAFW_METADATA_KEY_COPYRIGHT},
	{G_TYPE_STRING, MAFW_METADATA_KEY_AUDIO_CODEC, MAFW_METADATA_KEY_AUDIO_CODEC},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ALBUM_ART_URI, MAFW_METADATA_KEY_ALBUM_ART_URI},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ALBUM_ART, MAFW_METADATA_KEY_ALBUM_ART},
	{G_TYPE_STRING, MAFW_METADATA_KEY_VIDEO_CODEC, MAFW_METADATA_KEY_VIDEO_CODEC},
	{G_TYPE_FLOAT, MAFW_METADATA_KEY_VIDEO_FRAMERATE, MAFW_METADATA_KEY_VIDEO_FRAMERATE},
	{G_TYPE_STRING, MAFW_METADATA_KEY_EXIF_XML, MAFW_METADATA_KEY_EXIF_XML},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ICON_URI, MAFW_METADATA_KEY_ICON_URI},
	{G_TYPE_STRING, MAFW_METADATA_KEY_ICON, MAFW_METADATA_KEY_ICON}
};

static GHashTable *_mafw_to_upnphash;

/**
 * util_init:
 *
 * Initializes the needed hash-table, to speed up the mafw-key->flags mapping
 */
void util_init(void)
{
	gint i = 0, keynum = G_N_ELEMENTS(upnpmaps);

	if (_mafw_to_upnphash)
		return;

	_mafw_to_upnphash = g_hash_table_new(g_str_hash, g_str_equal);
	g_assert(_mafw_to_upnphash);
	
	for(i=0; i < keynum; i++)
	{
		g_hash_table_insert(_mafw_to_upnphash,
					(gpointer)upnpmaps[i].mafw_key,
					GINT_TO_POINTER(i+1));
	}
}

/**
 * util_get_id_from_mafwkey:
 *
 * Returns the ID of a mafw-key, or -1 if not supported
 */
static gint util_get_id_from_mafwkey(const gchar *mafwkey)
{
	return GPOINTER_TO_INT(g_hash_table_lookup(_mafw_to_upnphash,
					mafwkey)) - 1;
}

/**
 * util_get_upnpflag_from_mafwkey:
 * @mafwkey:	MAFW metadata key
 *
 * Returns the flag of the asked metadata-key, or 0, if not supported
 */
static guint64 util_get_upnpflag_from_mafwkey(const gchar *mafwkey)
{
	guint64 flagn = 1;
	gint id = util_get_id_from_mafwkey(mafwkey);

	if (id == -1)
		return 0;

	flagn <<= id;
	return flagn;
}


/**
 * didl_mafwkey_to_upnp_result:
 * @id: The ID to convert
 * @type:    The G_TYPE of the returned value
 *
 * Converts MAFW metadata keys to their UPnP equivalents and tells what the
 * parameter's type is so that we can put the correct type into a GValue.
 * This function is mainly used when parsing the DIDL-Lite result thru the
 * didl_fallback() function.
 *
 * Note: some of these mappings are actually attributes of a <res> element,
 * but it doesn't matter that much since both cases (property & res attr)
 * are checked.
 *
 * Returns: The UPnP-ified key or NULL if mapping cannot be done.
 */
const gchar* util_mafwkey_to_upnp_result(gint id, gint* type)
{
	struct _upnp_map curmap;

	if (id > G_N_ELEMENTS(upnpmaps)-1)
		return NULL;
	curmap = upnpmaps[id];
	
	*type = curmap.gtype;
	return curmap.upnp_key;
}

/**
 * util_get_metadatakey_from_id:
 * @id:	ID of the metadata
 *
 * Returns the mafw-metadata-key, defined by the ID.
 */
const gchar *util_get_metadatakey_from_id(gint id)
{
	struct _upnp_map curmap;

	if (id > G_N_ELEMENTS(upnpmaps)-1)
		return NULL;
	curmap = upnpmaps[id];
	
	return curmap.mafw_key;
}

/**
 * util_compile_mdata_keys:
 * @original:	metadatakeys
 *
 * Converts the list of metadata-keys into flags.
 */
guint64 util_compile_mdata_keys(const gchar* const* original)
{
	guint64 mkeys = 0;
	gint i;
	
	if (original == NULL || original[0] == NULL)
		return 0;

	if (strcmp(MAFW_SOURCE_ALL_KEYS[0], original[0]) == 0)
		return G_MAXUINT64;
	
	for (i = 0; original[i]; i++)
	{
		mkeys |= util_get_upnpflag_from_mafwkey(original[i]);
	}
	return mkeys;
}

/**
 * util_compare_uint:
 * @a: First uint value to compare
 * @b: Second uint value to compare
 *
 * GTree guint key comparator function.
 *
 * Returns: if (a < b) -1; else if (a > b) 1; else 0
 */
gint util_compare_uint(guint a, guint b)
{
	if (a < b)
		return -1;
	else if (a > b)
		return  1;
	else
		return  0;
}

/**
 * util_create_objectid:
 * @source: A #MafwUPnPSource that provided the @didl_node
 * @didl_node: An xmlNode containing a DIDL-Lite object
 *
 * Creates an object ID (sourceid::itemid) by combining the UUID from the
 * given #MafwUPnPSource and the DIDL-Lite item/container ID found from
 * the #xmlNode.
 *
 * Returns: A MAFW object ID. Must be freed after use.
 */
gchar* util_create_objectid(MafwUPnPSource* source, xmlNode* didl_node)
{
	const gchar* uuid;
	gchar* itemid;
	gchar* objectid = NULL;

	/* Construct an object ID from UDN & item ID */
	uuid = mafw_extension_get_uuid(MAFW_EXTENSION(source));
	itemid = gupnp_didl_lite_object_get_id(didl_node);
	if (itemid != NULL)
	{
		objectid = g_strdup_printf("%s::%s", uuid, itemid);
		g_free(itemid);
	}

	return objectid;
}
