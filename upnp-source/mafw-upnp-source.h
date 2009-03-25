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
