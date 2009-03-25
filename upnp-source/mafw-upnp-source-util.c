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

/**
 * util_strvdup:
 * @original: A %NULL-terminated array of strings to copy or %NULL.
 *
 * Duplicates @original, turning %NULL-arrays into empty ones.
 */
gchar** util_strvdup(const gchar* const* original)
{
	gchar** copy;
	guint i;

	if (original == NULL)
		return g_new0(gchar*, 1);

	/* May it be anyone who designed g_strv_length() must be a moron
	 * for not makeing it const. */
	copy = g_new(gchar*, g_strv_length((gchar **)original) + 1);
	for (i = 0; original[i]; i++)
		copy[i] = g_strdup(original[i]);
	copy[i] = NULL;

	return copy;
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

