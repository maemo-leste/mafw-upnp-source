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

/*----------------------------------------------------------------------------
  Resource information extraction
  ----------------------------------------------------------------------------*/

/**
 * didl_get_http_res:
 * @didl_object: An @xmlNode containing a DIDL-Lite <item> or <container>
 *
 * Gets the first http resource node from the given DIDL object.
 *
 * Returns: An %xmlNode (must NOT be freed) or %NULL.
 **/
xmlNode* didl_get_http_res(xmlNode* didl_object)
{
	GList* properties;
	GList* node;
	xmlNode* xml_node;
	gchar* protocol;

	g_return_val_if_fail(didl_object != NULL, NULL);

	/* Get a list of <res> property nodes and iterate thru them */
	properties = gupnp_didl_lite_object_get_property(didl_object, DIDL_RES);
	for (node = properties; node != NULL; node = node->next)
	{
		xml_node = (xmlNode*) node->data;
		if (xml_node == NULL)
			continue;

		/* Get the first resource with http-get protocol */
		protocol = didl_res_get_protocol_info(xml_node, 0);
		if (protocol != NULL &&
		    strcmp(protocol, DIDL_RES_PROTOCOL_INFO_HTTP) == 0)
		{
			g_free(protocol);
			g_list_free(properties);
			return xml_node;
		}
		else if (protocol != NULL)
		{
			g_free(protocol);
		}
	}

	g_list_free(properties);
	return NULL;
}

/**
 * didl_get_http_res_uri:
 * @didl_object: An @xmlNode containing a DIDL-Lite <item> or <container>
 *
 * Gets the first http resource node from the given DIDL object and returns
 * its contents (i.e. the URI).
 *
 * Returns: A string containing the http URI (must be freed) or %NULL.
 **/
gchar* didl_get_http_res_uri(xmlNode* didl_object)
{
	xmlNode* res_node;
	gchar* uri;

	res_node = didl_get_http_res(didl_object);
	uri = gupnp_didl_lite_property_get_value(res_node);
	
	return uri;
}

/**
 * didl_res_get_protocol_info:
 * @res_node: An @xmlNode that contains a <res> element
 * @field:    The number of the field to extract (0=protocol, 1=network,
 *                                                2=mimetype, 3=additional info)
 *
 * Extracts the requested protocol info field from the given resource element.
 *
 * Returns: Protocol info string (must be freed) or %NULL.
 **/
gchar* didl_res_get_protocol_info(xmlNode* res_node, gint field)
{
	gchar* pinfo;
	gchar* value;
	gchar** array;

	g_return_val_if_fail(res_node != NULL, NULL);
	g_return_val_if_fail(field >= 0 || field < 4, NULL);

	/* Get protocolInfo attribute from the <res> element */
	pinfo = gupnp_didl_lite_property_get_attribute(res_node,
						       DIDL_RES_PROTOCOL_INFO);
	if (pinfo == NULL)
		return NULL;

	/* Split the protocol info field into 4 fields:
	   0:protocol, 1:network, 2:mime-type and 3:additional info. */
	array = g_strsplit(pinfo, DIDL_RES_PROTOCOL_INFO_DELIMITER, 4);
	g_free(pinfo);
	if (g_strv_length(array) < 4)
	{
		value = NULL;
		g_strfreev(array);
	}
	else
	{
		value = g_strdup(array[field]);
		g_strfreev(array);
	}

	return value;
}

/**
 * didl_get_duration:
 * @didl_object: An @xmlNode that contains a DIDL-Lite object
 *
 * Extracts the duration attribute from the object's first HTTP resource.
 *
 * Returns: Duration in seconds (-1 if not found).
 **/
gint didl_get_duration(xmlNode* didl_object)
{
	xmlNode* res_node;
	gchar* value;
	gint duration = 0;

	g_return_val_if_fail(didl_object != NULL, 0);

	/* Find the first http resource */
	res_node = didl_get_http_res(didl_object);
	if (res_node == NULL)
		return 0;

	/* Get duration as H+:MM:SS.F+ */
	value = gupnp_didl_lite_property_get_attribute(res_node,
						       DIDL_RES_DURATION);

	/* Convert H+:MM:SS.F+ to seconds */
	if (value != NULL)
		duration = didl_h_mm_ss_to_int(value);
	else
		duration = -1;
	g_free(value);

	return duration;
}

/**
 * didl_get_mimetype:
 * @didl_object: An @xmlNode that contains a DIDL-Lite object
 *
 * Extracts the MIME type associated with the given DIDL-Lite object. Assigns
 * %MAFW_METADATA_VALUE_MIME_CONTAINER to container objects. For item objects,
 * the mimetype is extracted from the first http-get resource node.
 *
 * Returns: The MIME type (must be freed) or %NULL.
 **/
gchar* didl_get_mimetype(xmlNode* didl_object)
{
	xmlNode* res_node;

	g_return_val_if_fail(didl_object != NULL, NULL);

	if (gupnp_didl_lite_object_is_container(didl_object) == TRUE)
	{
		/* This object is a container so let's fake its MIME type */
		return g_strdup(MAFW_METADATA_VALUE_MIME_CONTAINER);
	}
	else
	{
		/* This object is not an item so it has a "real" MIME type */
		res_node = didl_get_http_res(didl_object);
		if (res_node != NULL)
			return didl_res_get_protocol_info(res_node, 2);
		else
			return NULL;
	}
}

/**
 * didl_get_childcount:
 * @didl_object: An @xmlNode that contains a DIDL-Lite <container>
 *
 * Get the childcount attribute (i.e. number of child nodes under the given
 * container) from the given DIDL-Lite container element.
 *
 * Returns: Number of children (-1 if not a container).
 **/
gint didl_get_childcount(xmlNode* didl_object)
{
	gchar* value;
	gint count;

	g_return_val_if_fail(didl_object != NULL, 0);

	if (gupnp_didl_lite_object_is_container(didl_object) == FALSE)
		return -1;

	/* Map mafw childcount key to container's childCount attribute */
	value = gupnp_didl_lite_property_get_attribute(didl_object,
						       DIDL_CHILDCOUNT);
	if (value != NULL)
		count = atoi(value);
	else
		count = -1;
	g_free(value);
	
	return count;
}

/**
 * didl_get_thumbnail_uri:
 * @didl_object: An @xmlNode that contains a DIDL-Lite <item>
 *
 * Extracts the album art URI for the object if the object class is music,
 * and a thumbnail-sized image if the object class is image or video.
 *
 * Returns: A string containing the URI (must be freed) or %NULL.
 */
gchar* didl_get_thumbnail_uri(xmlNode* didl_object)
{
	gchar* class;
	gchar* uri;

	g_return_val_if_fail(didl_object != NULL, NULL);

	class = gupnp_didl_lite_object_get_upnp_class(didl_object);
	if (class == NULL)
		return NULL;
	
	if (strstr(class, DIDL_CLASS_AUDIO) != NULL)
	{
		uri = didl_get_album_art_uri(didl_object);
	}
	else if (strstr(class, DIDL_CLASS_IMAGE) != NULL)
	{
		uri = NULL;
	}
	else if (strstr(class, DIDL_CLASS_VIDEO) != NULL)
	{
		uri = NULL;
	}
	else
	{
	        uri = NULL;
	}

	g_free(class);
	return uri;
}

/**
 * didl_get_album_art_uri:
 * @didl_object: An @xmlNode that contains a DIDL-Lite <item> or <container>
 *
 * Extracts the album art URI for the object.
 *
 * Returns: A string containing the URI (must be freed) or %NULL.
 */
gchar* didl_get_album_art_uri(xmlNode* didl_object)
{
	GList* properties;
	GList* node;
	xmlNode* xml_node;
	gchar* uri;

	g_return_val_if_fail(didl_object != NULL, NULL);

	/* Get a list of <albumArtURI> property nodes and iterate thru them */
	properties = gupnp_didl_lite_object_get_property(didl_object,
							 DIDL_ALBUM_ART_URI);
	for (node = properties; node != NULL; node = node->next)
	{
		xml_node = (xmlNode*) node->data;
		if (xml_node == NULL)
			continue;

		/* Get the first non-null albumArtURI */
		uri = gupnp_didl_lite_property_get_value(xml_node);
		if (uri != NULL)
		{
			g_list_free(properties);
			return uri;
		}
	}

	g_list_free(properties);
	return NULL;
}

/**
 * didl_get_seekability:
 * @didl_object: An @xmlNode that contains a DIDL-Lite object
 *
 * Extracts the seekability associated with the given DIDL-Lite
 * object.
 *
 * Returns: -1 for container objects. For item objects, the
 * seekability is extracted and returns 1 if item is seekable, 0 if it
 * is not and -1 in case it could not be extracted.
 **/
gint8 didl_get_seekability(xmlNode* didl_object)
{
	gint8 seekability = -1;

	g_return_val_if_fail(didl_object != NULL, -1);

	if (!gupnp_didl_lite_object_is_container(didl_object))
	{
		xmlNode* res_node;

		/* For non container objects, we set default
		 * seekability to 0 */
		seekability = 0;

		res_node = didl_get_http_res(didl_object);
		if (res_node != NULL) {
			gchar *additional_info = NULL;

			additional_info = didl_res_get_protocol_info(res_node,
								     3);
			if (additional_info != NULL &&
			    strcmp(additional_info, "*") != 0) {
				gchar *dlna_org_op = NULL;

				dlna_org_op = strstr(additional_info,
						     "DLNA.ORG_OP=");

				/* In "DLNA.ORG_OP=ab" we need the
				 character b. If it is 1, it is
				 seekable, if 0, it is not */
				if (dlna_org_op != NULL &&
				    strlen(dlna_org_op) >= 13) {
					if (dlna_org_op[13] == '1') {
						seekability = 1;
						g_debug("seekability positive");
					}
				}
			}

			g_free(additional_info);
		}
	}

	g_debug("final seekability %d", seekability);
	return seekability;
}

/**
 * didl_fallback:
 * @didl_object: A DIDL-Lite object to search the key from
 * @key:         The metadata key to search for
 *
 * Attempts to find the given metadata key either from the object's properties
 * or from the first http res property's attributes.
 *
 * Returns: A string containing the requested value (must be freed) or %NULL.
 */
gchar* didl_fallback(xmlNode* didl_object, const gchar* key, gint* type)
{
	GList* list;
	gchar* val;
	xmlNode* res_node;
	const gchar* mapped_key;

	mapped_key = didl_mafwkey_to_upnp_result(key, type);

	list = gupnp_didl_lite_object_get_property(didl_object, mapped_key);
	if (list != NULL)
	{
		val = gupnp_didl_lite_property_get_value((xmlNode*) list->data);
		g_list_free(list);
	}
	else
	{
		res_node = didl_get_http_res(didl_object);
		if (res_node != NULL)
			val = gupnp_didl_lite_property_get_attribute(
				res_node, mapped_key);
		else
			val = NULL;
	}
	
	return val;
}

/**
 * didl_mafwkey_to_upnp_result:
 * @mafwkey: The MAFW metadata key to convert
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
 * Returns: The UPnP-ified key or @mafwkey if mapping cannot be done.
 */
const gchar* didl_mafwkey_to_upnp_result(const gchar* mafwkey, gint* type)
{
	g_return_val_if_fail(mafwkey != NULL, NULL);
	g_return_val_if_fail(type != NULL, NULL);

	if (strcmp(mafwkey, MAFW_METADATA_KEY_LYRICS_URI) == 0)
	{
		*type = G_TYPE_STRING;
		return DIDL_LYRICS_URI;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_PROTOCOL_INFO) == 0)
	{
		*type = G_TYPE_STRING;
		return DIDL_RES_PROTOCOL_INFO;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI)
		  == 0)
	{
		*type = G_TYPE_STRING;
		return DIDL_ALBUM_ART_URI;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM_ART_MEDIUM_URI)
		  == 0)
	{
		*type = G_TYPE_STRING;
		return DIDL_ALBUM_ART_URI;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ALBUM_ART_LARGE_URI) == 0)
	{
		*type = G_TYPE_STRING;
		return DIDL_ALBUM_ART_URI;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_ARTIST_INFO_URI) == 0)
	{
		*type = G_TYPE_STRING;
		return DIDL_DISCOGRAPHY_URI;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_AUDIO_BITRATE) == 0)
	{
		/* <res> attribute */
		*type = G_TYPE_INT;
		return DIDL_RES_BITRATE;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_VIDEO_BITRATE) == 0)
	{
		/* <res> attribute */
		*type = G_TYPE_INT;
		return DIDL_RES_BITRATE;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_BITRATE) == 0)
	{
		/* <res> attribute */
		*type = G_TYPE_INT;
		return DIDL_RES_BITRATE;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_FILESIZE) == 0)
	{
		/* <res> attribute */
		*type = G_TYPE_INT;
		return DIDL_RES_SIZE;
	}
	else if (strcmp(mafwkey, MAFW_METADATA_KEY_BPP) == 0)
	{
		/* <res> attribute */
		*type = G_TYPE_INT;
		return DIDL_RES_COLORDEPTH;
	}
	else
	{
		*type = G_TYPE_STRING;
		return mafwkey;
	}
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

/*----------------------------------------------------------------------------
  Time conversion
  ----------------------------------------------------------------------------*/

/**
 * didl_h_mm_ss_to_int:
 * @time: A CurrentTrackDuration string (H+:MM:SS.F+)
 *
 * Converts a CurrentTrackDuration data type into an integer (seconds).
 * Doesn't care about fractions (.F+ or .F0/F1).
 *
 * Returns: Time as an integer (0 if an error occurs)
 */
gint didl_h_mm_ss_to_int(gchar* time)
{
	int len = 0;
	int i = 0;
	int head = 0;
	int tail = 0;
	int result = 0;
	gchar* tmp = NULL;
	
	/* The passed parameter may be in format MM:SS or H+:MM:SS */
	gboolean has_hours = FALSE;

	if (time == NULL)
		return 0;

	len = strlen(time);
	tmp = g_new0(gchar, sizeof(gchar) * len);

	/* Find the first colon (it can be anywhere) and also count the
	 * amount of colons to know if there are hours or not */
	for (i = 0; i < len; i++)
	{
		if (time[i] == ':')
		{
			if (tail != 0)
				has_hours = TRUE;
			else
				tail = i;
		}
	}

	/* Bail out if tail goes too far or head is bigger than tail */
	if (tail > len || head > tail)
	{
		g_free(tmp);
		return 0;
	}

	/* Extract hours */
	if (has_hours == TRUE)
	{
		memcpy(tmp, time + head, tail - head);
		tmp[tail - head + 1] = '\0';

		result += 3600 * atoi(tmp);

		/* The next colon should be exactly 2 chars right */
		head = tail + 1;
		tail = head + 2;
	}
	else
	{
		/* The format is now exactly MM:SS */
		head = 0;
		tail = 2;
	}

	/* Bail out if tail goes too far or head is bigger than tail */
	if (tail > len || head > tail)
	{
		g_free(tmp);
		return 0;
	}

	/* Extract minutes */
	memcpy(tmp, time + head, tail - head);
	tmp[2] = '\0';

	result += 60 * atoi(tmp);

	/* The next colon should again be exactly 2 chars right */
	head = tail + 1;
	tail = head + 2;

	/* Bail out if tail goes too far or head is bigger than tail */
	if (tail > len || head > tail)
	{
		g_free(tmp);
		return 0;
	}

	/* Extract seconds */
	memcpy(tmp, time + head, tail - head);
	tmp[2] = '\0';
	result += atoi(tmp);

	g_free(tmp);

	return result;
}
