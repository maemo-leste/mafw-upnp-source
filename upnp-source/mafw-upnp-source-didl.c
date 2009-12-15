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

#include <glib.h>
#include <string.h>
#include <libmafw/mafw.h>
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>

#include "mafw-upnp-source-didl.h"
#include "mafw-upnp-source-util.h"

/*----------------------------------------------------------------------------
  Resource information extraction
  ----------------------------------------------------------------------------*/

/**
 * didl_get_supported_resources:
 * @didl_object: An @xmlNode containing a DIDL-Lite <item> or <container>
 *
 * Filters out the supported resources, and returns a list of resources in XML.
 *
 * Return: supported resource-list. Should be freed with g_list_free;
 */
GList *didl_get_supported_resources(GUPnPDIDLLiteObject *didlobject)
{
	GList *resources, *node, *prev;

	resources = gupnp_didl_lite_object_get_resources(didlobject);

	node = resources;
	while (node)
	{
		const gchar *protocol = NULL;

		if (node->data == NULL)
		{
			prev = node;
			node = node->next;
			resources = g_list_delete_link(resources, prev);
			continue;
		}

		protocol = gupnp_protocol_info_get_protocol(
				gupnp_didl_lite_resource_get_protocol_info(
						(GUPnPDIDLLiteResource*)node->data));
		if (protocol != NULL &&
		    strcmp(protocol, DIDL_RES_PROTOCOL_INFO_HTTP) != 0)
		{
			g_object_unref(node->data);
			prev = node;
			node = node->next;
			resources = g_list_delete_link(resources, prev);
			continue;
		}
		node = node->next;
	}
	
	return resources;
}
/**
 * didl_check_filetype:
 * @didl_object: An @xmlNode containing a DIDL-Lite <item> or <container>
 * @is_supported:	Defines, whether the item is supported, or not
 *
 * Return: Returns TRUE, if item is audio
 */
gboolean didl_check_filetype(GUPnPDIDLLiteObject *didlobject, gboolean *is_supported)
{
	const gchar *class;
	gboolean is_audio = TRUE;

	class = gupnp_didl_lite_object_get_upnp_class(didlobject);
	if (class && strstr(class, DIDL_CLASS_AUDIO) != NULL)
	{
		is_audio = TRUE;
		*is_supported = TRUE;
	}
	else if (class && strstr(class, DIDL_CLASS_VIDEO) != NULL)
	{
		is_audio = FALSE;
		*is_supported = TRUE;
	}
	else
		*is_supported = FALSE;
	
	return is_audio;
}

/**
 * didl_get_http_res_uri:
 * @metadata:	Metadata hash-table to fill.
 * @properties:	Resource property list
 * @is_audio:	TRUE, if items are audio items
 *
 * Adds the URIs to the metadata. If it contains audio and video items, it will
 * add only one type of items, based on the is_audio flag.
 *
 **/
void didl_get_http_res_uri(GHashTable *metadata, GList *resources,
				gboolean is_audio)
{
	GList* node;
	GUPnPDIDLLiteResource* res;
	const gchar *uri;
	const gchar *mimetype;
	gboolean uri_added = FALSE;

	for (node = resources; node != NULL; node = node->next)
	{
		res = (GUPnPDIDLLiteResource*) node->data;

		/* Get the first resource with http-get protocol */
		mimetype = gupnp_protocol_info_get_mime_type(
				gupnp_didl_lite_resource_get_protocol_info(res));
		if (mimetype &&
			((is_audio && g_str_has_prefix(mimetype, "audio")) || 
				(!is_audio && g_str_has_prefix(mimetype, "video")))
			)
		{
			uri = gupnp_didl_lite_resource_get_uri(res);
			mafw_metadata_add_str(metadata, MAFW_METADATA_KEY_URI,
				uri);
			uri_added = TRUE;
		}
	}

	/* if we haven't added any URI, it is better to add all the supported
	resources */
	if (!uri_added)
	{
		for (node = resources; node != NULL; node = node->next)
		{
			res = (GUPnPDIDLLiteResource*) node->data;
			uri = gupnp_didl_lite_resource_get_uri(res);
			mafw_metadata_add_str(metadata, MAFW_METADATA_KEY_URI,
				uri);
		}
	}
}

/**
 * didl_get_mimetype:
 * @metadata:	Metadata hash-table to fill.
 * @properties:	Resource property list
 * @is_audio:	TRUE, if items are audio items
 * @is_container: TRUE, if item is a container
 *
 * Extracts the MIME type associated with the given DIDL-Lite object. Assigns
 * %MAFW_METADATA_VALUE_MIME_CONTAINER to container objects. For item objects,
 * the mimetype is extracted from the first http-get resource node, or if it
 * contains several items, mimetype will be MAFW_METADATA_VALUE_MIME_AUDIO or
 * MAFW_METADATA_VALUE_MIME_VIDEO.
 *
 **/
void didl_get_mimetype(GHashTable *metadata, gboolean is_container,
			gboolean is_audio, GList* resources)
{
	const gchar *value;

	if (is_container)
			mafw_metadata_add_str(metadata, MAFW_METADATA_KEY_MIME,
				MAFW_METADATA_VALUE_MIME_CONTAINER);
	else
	{
		if (!resources)
			return;
		if (g_list_length(resources) == 1)
		{
			value = gupnp_protocol_info_get_mime_type(
					gupnp_didl_lite_resource_get_protocol_info(
					(GUPnPDIDLLiteResource*)resources->data));
			mafw_metadata_add_str(metadata,
				MAFW_METADATA_KEY_MIME, value);
		}
		else
		{/* Multiple resources */
			if (is_audio)
			{
				mafw_metadata_add_str(metadata,
					MAFW_METADATA_KEY_MIME,
					MAFW_METADATA_VALUE_MIME_AUDIO);
			}
			else
			{
				mafw_metadata_add_str(metadata,
					MAFW_METADATA_KEY_MIME,
					MAFW_METADATA_VALUE_MIME_VIDEO);
			}
		}
	}
}

/**
 * didl_fallback:
 * @didl_object: A DIDL-Lite object to search the key from
 * @id:         The metadata id to search for
 * @res_node: An @xmlNode of the first resource
 *
 * Attempts to find the given metadata key either from the object's properties
 * or from the first http res property's attributes.
 *
 * Returns: A string containing the requested value (must be freed) or %NULL.
 */
gchar* didl_fallback(GUPnPDIDLLiteObject* didl_object,
			GUPnPDIDLLiteResource* first_res, gint id, gint* type)
{
	GList* list;
	gchar* val = NULL;
	const gchar* mapped_key;

	mapped_key = util_mafwkey_to_upnp_result(id, type);
	if (!mapped_key)
		return NULL;

	list = gupnp_didl_lite_object_get_properties(didl_object, mapped_key);
	if (list != NULL)
	{
		xmlChar *content = xmlNodeGetContent((xmlNode*) list->data);

		val = g_strdup((gchar*)content);
		xmlFree(content);

		g_list_free(list);
	}
	else
	{
		if (first_res != NULL)
		{
			xmlNode *node = gupnp_didl_lite_resource_get_xml_node(
						first_res);
			xmlAttr *attr;
			
			
			for (attr = node->properties; attr; attr = attr->next)
			{
				if (!attr->name)
					continue;
				if (!strcmp(mapped_key, (const char*)attr->name))
					break;
			}
			if (attr)
			{
				xmlChar *cont;

				cont = xmlNodeGetContent(attr->children);
				val = g_strdup((gchar*)cont);
				xmlFree(cont);
			}

		}
	}
	
	return val;
}

/*----------------------------------------------------------------------------
  Browse filter
  ----------------------------------------------------------------------------*/

/**
 * didl_mafwkey_array_to_upnp_filter:
 * @metadata_keys: An array of MAFW metadata keys to convert into a UPnP filter
 *
 * Converts the given MAFW metadata keys into a comma-separated string of
 * DIDL-Lite properties that can be used as a filter string in a UPnP browse
 * action.
 *
 * NOTE: @metadata_keys are not escaped, because they are not expected to
 * contain non-alphanumeric characters.
 *
 * Returns: Filter string (must be freed).
 */
gchar* didl_mafwkey_array_to_upnp_filter(const gchar* const* metadata_keys)
{
	GString* filter;
	gint i;

	g_assert(metadata_keys != NULL);

	filter = g_string_new("");
	for (i = 0; metadata_keys[i] != NULL; i++)
	{
		const gchar* mapping;
		
		/* Convert a MAFW metadata key into a UPnP filter key */
		mapping = didl_mafwkey_to_upnp_filter(metadata_keys[i]);

		/* Don't add the same key more than once */
		if (strstr(filter->str, mapping) == NULL)
		{
			/* Put a comma between all items, but don't
			   put it as the first character */
			if (strlen(filter->str) > 0)
				g_string_append(filter, ",");
			g_string_append(filter, mapping);
		}
	}
	
	/* Don't free the buffer -> FALSE */
	return g_string_free(filter, FALSE);
}

/**
 * didl_mafwkey_to_upnp_filter:
 * @mafwkey: The MAFW metadata key to convert
 *
 * Converts MAFW metadata keys to their UPnP equivalents to be used in a browse
 * action's filter. Note that some of these mappings don't go 1:1, since,
 * for example, MIMETYPE is a part or RES@protocolInfo, so we need to request a
 * RES element to get it.
 *
 * Returns: The UPnP-ified key or @mafwkey if mapping cannot be done.
 */
const gchar* didl_mafwkey_to_upnp_filter(const gchar* mafwkey)
{
	g_return_val_if_fail(mafwkey != NULL, NULL);

	/* TODO Use an associative array. */

	/* <res> derivatives */
	if (strcmp(mafwkey, MAFW_METADATA_KEY_URI) == 0)
		return DIDL_RES;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_PROTOCOL_INFO) == 0)
		return DIDL_RES "@" DIDL_RES_PROTOCOL_INFO;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_MIME) == 0)
		return DIDL_RES "@" DIDL_RES_PROTOCOL_INFO;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_DURATION) == 0)
		return DIDL_RES "@" DIDL_RES_DURATION;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_AUDIO_BITRATE) == 0)
		return DIDL_RES "@" DIDL_RES_BITRATE;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_VIDEO_BITRATE) == 0)
		return DIDL_RES "@" DIDL_RES_BITRATE;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_BITRATE) == 0)
		return DIDL_RES "@" DIDL_RES_BITRATE;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_FILESIZE) == 0)
		return DIDL_RES "@" DIDL_RES_SIZE;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_BPP) == 0)
		return DIDL_RES "@" DIDL_RES_COLORDEPTH;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_RES_X) == 0)
		return DIDL_RES "@" DIDL_RES_RESOLUTION;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_RES_Y) == 0)
		return DIDL_RES "@" DIDL_RES_RESOLUTION;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_THUMBNAIL_URI) == 0)
		return DIDL_RES "@" DIDL_RES_PROTOCOL_INFO;
	/* Properties from the `upnp' and `dc' namespaces */
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI) == 0)
		return DIDL_ALBUM_ART_URI;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM_ART_MEDIUM_URI)== 0)
		return DIDL_ALBUM_ART_URI;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM_ART_LARGE_URI) == 0)
		return DIDL_ALBUM_ART_URI;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_LYRICS_URI) == 0)
		return DIDL_LYRICS_URI;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ARTIST_INFO_URI) == 0)
		return DIDL_DISCOGRAPHY_URI;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_TITLE) == 0)
		return DIDL_TITLE;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ARTIST) == 0)
		return DIDL_ARTIST;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_GENRE) == 0)
		return DIDL_GENRE;
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM) == 0)
		return DIDL_ALBUM;
	else
		return mafwkey;
}
