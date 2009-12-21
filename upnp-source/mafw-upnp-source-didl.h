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

#ifndef MAFW_UPNP_SOURCE_DIDL_H
#define MAFW_UPNP_SOURCE_DIDL_H

/*----------------------------------------------------------------------------
  DIDL-Lite identifiers
  ----------------------------------------------------------------------------*/
#define DIDL_TITLE "dc:title"
#define DIDL_ARTIST "upnp:artist"
#define DIDL_GENRE "upnp:genre"
#define DIDL_ALBUM "upnp:album"

#define DIDL_RES "res"
#define DIDL_RES_DURATION "duration"
#define DIDL_RES_PROTOCOL_INFO "protocolInfo"
#define DIDL_RES_RESOLUTION "resolution"
#define DIDL_LYRICS_URI "lyricsURI"
#define DIDL_ALBUM_ART_URI "albumArtURI"
#define DIDL_DISCOGRAPHY_URI "artistDiscographyURI"
#define DIDL_RES_BITRATE "bitrate"
#define DIDL_RES_SIZE "size"
#define DIDL_RES_COLORDEPTH "colorDepth"

#define DIDL_RES_PROTOCOL_INFO_HTTP "http-get"

#define DIDL_CLASS_AUDIO "object.item.audioItem"
#define DIDL_CLASS_VIDEO "object.item.videoItem"

/*----------------------------------------------------------------------------
  Resource information extraction
  ----------------------------------------------------------------------------*/
GList *didl_get_supported_resources(GUPnPDIDLLiteObject *didlobject);
void didl_get_http_res_uri(GHashTable *metadata, GList *resources,
				gboolean is_audio);
gboolean didl_check_filetype(GUPnPDIDLLiteObject *didlobject, gboolean *is_supported);

void didl_get_mimetype(GHashTable *metadata, gboolean is_container,
			gboolean is_audio, GList* resources);

gchar* didl_fallback(GUPnPDIDLLiteObject* didl_object,
			GUPnPDIDLLiteResource* first_res, gint id, gint* type);


#endif /* MAFW_UPNP_SOURCE_DIDL_H */
