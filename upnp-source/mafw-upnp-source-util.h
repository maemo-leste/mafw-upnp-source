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

#ifndef MAFW_UPNP_RENDERER_UTIL_H
#define MAFW_UPNP_RENDERER_UTIL_H

#include "mafw-upnp-source.h"

gchar* util_udn_to_uuid(const gchar* uuid);
gchar* util_uuid_to_udn(const gchar* uuid);

guint64 util_compile_mdata_keys(const gchar* const* original);

gint util_compare_uint(guint a, guint b);
gchar* util_create_objectid(MafwUPnPSource* source, xmlNode* didl_node);

const gchar* util_mafwkey_to_upnp_result(gint id, gint* type);
const gchar *util_get_metadatakey_from_id(gint id);
void util_init(void);

#endif
