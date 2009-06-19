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

#include <libmafw/mafw-source.h>

#ifndef MAFW_UPNP_SOURCE_H
#define MAFW_UPNP_SOURCE_H

G_BEGIN_DECLS

#define MAFW_UPNP_SOURCE_NAME "upnp_source_name"
#define MAFW_UPNP_SOURCE_UUID "upnp_source_uuid"

#define MAFW_UPNP_SOURCE_EXTENSION_NAME "mafw_upnp_source"

/* Valid metadata keys */
#define MAFW_UPNP_SOURCE_MDATA_KEY_FILETYPE "file-type"

/*----------------------------------------------------------------------------
  GObject type conversion macros
  ----------------------------------------------------------------------------*/

#define MAFW_TYPE_UPNP_SOURCE			\
	(mafw_upnp_source_get_type ())

#define MAFW_UPNP_SOURCE(obj)						\
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), MAFW_TYPE_UPNP_SOURCE,	\
				     MafwUPnPSource))
#define MAFW_IS_UPNP_SOURCE(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAFW_TYPE_UPNP_SOURCE))

#define MAFW_UPNP_SOURCE_CLASS(klass)					\
	(G_TYPE_CHECK_CLASS_CAST((klass), MAFW_TYPE_UPNP_SOURCE,	\
				 MafwUPnPSourceClass))

#define MAFW_IS_UPNP_SOURCE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass), MAFW_TYPE_UPNP_SOURCE))

#define MAFW_UPNP_SOURCE_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS ((obj), MAFW_TYPE_UPNP_SOURCE,	\
				    MafwUPnPSourceClass))

/*----------------------------------------------------------------------------
  Object structures
  ----------------------------------------------------------------------------*/

typedef struct _MafwUPnPSource MafwUPnPSource;
typedef struct _MafwUPnPSourceClass MafwUPnPSourceClass;
typedef struct _MafwUPnPSourcePrivate MafwUPnPSourcePrivate;

struct _MafwUPnPSource {
	MafwSource parent;
	MafwUPnPSourcePrivate *priv;
};

struct _MafwUPnPSourceClass {
	MafwSourceClass parent_class;
};


#define MUPnPSrc_MKey_URI			G_GUINT64_CONSTANT(0x1)
#define MUPnPSrc_MKey_Childcount		G_GUINT64_CONSTANT(0x2)
#define MUPnPSrc_MKey_MimeType			G_GUINT64_CONSTANT(0x4)
#define MUPnPSrc_MKey_Duration			G_GUINT64_CONSTANT(0x8)
#define MUPnPSrc_MKey_Thumbnail_URI		G_GUINT64_CONSTANT(0x10)
#define MUPnPSrc_MKey_DIDL			G_GUINT64_CONSTANT(0x20)
#define MUPnPSrc_MKey_Is_Seekable		G_GUINT64_CONSTANT(0x40)
#define MUPnPSrc_MKey_Lyrics_URI		G_GUINT64_CONSTANT(0x80)
#define MUPnPSrc_MKey_Protocol_Info		G_GUINT64_CONSTANT(0x100)
#define MUPnPSrc_MKey_AlbumArt_Small_Uri	G_GUINT64_CONSTANT(0x200)
#define MUPnPSrc_MKey_AlbumArt_Medium_Uri	G_GUINT64_CONSTANT(0x400)
#define MUPnPSrc_MKey_AlbumArt_Large_Uri	G_GUINT64_CONSTANT(0x800)
#define MUPnPSrc_MKey_Artist_Info_URI		G_GUINT64_CONSTANT(0x1000)
#define MUPnPSrc_MKey_Audio_Bitrate		G_GUINT64_CONSTANT(0x2000)
#define MUPnPSrc_MKey_Video_Bitrate		G_GUINT64_CONSTANT(0x4000)
#define MUPnPSrc_MKey_Bitrate			G_GUINT64_CONSTANT(0x8000)
#define MUPnPSrc_MKey_FileSize			G_GUINT64_CONSTANT(0x10000)
#define MUPnPSrc_MKey_Bpp			G_GUINT64_CONSTANT(0x20000)
#define MUPnPSrc_MKey_Title			G_GUINT64_CONSTANT(0x40000)
#define MUPnPSrc_MKey_Artist			G_GUINT64_CONSTANT(0x80000)
#define MUPnPSrc_MKey_Album			G_GUINT64_CONSTANT(0x100000)
#define MUPnPSrc_MKey_Genre			G_GUINT64_CONSTANT(0x200000)
#define MUPnPSrc_MKey_Track			G_GUINT64_CONSTANT(0x400000)
#define MUPnPSrc_MKey_Year			G_GUINT64_CONSTANT(0x800000)
#define MUPnPSrc_MKey_Count			G_GUINT64_CONSTANT(0x1000000)
#define MUPnPSrc_MKey_Playcount			G_GUINT64_CONSTANT(0x2000000)
#define MUPnPSrc_MKey_Description		G_GUINT64_CONSTANT(0x4000000)
#define MUPnPSrc_MKey_Encoding			G_GUINT64_CONSTANT(0x8000000)
#define MUPnPSrc_MKey_Added			G_GUINT64_CONSTANT(0x10000000)
#define MUPnPSrc_MKey_Thumbnail			G_GUINT64_CONSTANT(0x40000000)
#define MUPnPSrc_MKey_Res_X			G_GUINT64_CONSTANT(0x80000000)
#define MUPnPSrc_MKey_Res_Y			G_GUINT64_CONSTANT(0x100000000)
#define MUPnPSrc_MKey_Comment			G_GUINT64_CONSTANT(0x200000000)
#define MUPnPSrc_MKey_Tags			G_GUINT64_CONSTANT(0x400000000)
#define MUPnPSrc_MKey_Album_Info_URI		G_GUINT64_CONSTANT(0x800000000)
#define MUPnPSrc_MKey_Lyrics			G_GUINT64_CONSTANT(0x1000000000)
#define MUPnPSrc_MKey_Rating			G_GUINT64_CONSTANT(0x2000000000)
#define MUPnPSrc_MKey_Composer			G_GUINT64_CONSTANT(0x4000000000)
#define MUPnPSrc_MKey_FileName			G_GUINT64_CONSTANT(0x8000000000)
#define MUPnPSrc_MKey_CopyRight			G_GUINT64_CONSTANT(0x10000000000)
#define MUPnPSrc_MKey_Audio_Codec		G_GUINT64_CONSTANT(0x20000000000)
#define MUPnPSrc_MKey_AlbumArt_Uri		G_GUINT64_CONSTANT(0x40000000000)
#define MUPnPSrc_MKey_AlbumArt			G_GUINT64_CONSTANT(0x80000000000)
#define MUPnPSrc_MKey_Video_Codec		G_GUINT64_CONSTANT(0x100000000000)
#define MUPnPSrc_MKey_Video_FrameRate		G_GUINT64_CONSTANT(0x200000000000)
#define MUPnPSrc_MKey_ExifXML			G_GUINT64_CONSTANT(0x400000000000)
#define MUPnPSrc_MKey_Icon_URI			G_GUINT64_CONSTANT(0x800000000000)
#define MUPnPSrc_MKey_Icon			G_GUINT64_CONSTANT(0x1000000000000)

/*----------------------------------------------------------------------------
  Public API
  ----------------------------------------------------------------------------*/

/* Plugin initialization */
void mafw_upnp_source_plugin_initialize(MafwRegistry *registry);
void mafw_upnp_source_plugin_deinitialize(void);

GObject *mafw_upnp_source_new(const gchar *name, const gchar *uuid);
GType mafw_upnp_source_get_type(void);

G_END_DECLS

#endif /* MAFW_UPNP_SOURCE_H */

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
