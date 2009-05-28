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

#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <gmodule.h>

#ifdef HAVE_CONIC /* MAEMO */
#include <conic.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif /* MAEMO */

#include <libmafw/mafw.h>
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>
#include <libxml/debugXML.h>

#include "mafw-upnp-source.h"
#include "mafw-upnp-source-didl.h"
#include "mafw-upnp-source-util.h"

#define MAFW_UPNP_SOURCE_PLUGIN_NAME "MAFW-UPnP-Source"

/** Maximum number of items requested at a time */
#define DEFAULT_REQUESTED_COUNT 50

#ifndef CONTENT_DIR_NO_VERSION
#define CONTENT_DIR_NO_VERSION "urn:schemas-upnp-org:service:ContentDirectory"
#endif

#define SYSTEM_UPDATE_ID     "SystemUpdateID"
#define CONTAINER_UPDATE_IDS "ContainerUpdateIDs"

#define KNOWN_METADATA_KEYS "uri", "mime-type", "title", "duration", \
	"artist", "album", "genre", "track", "year", "bitrate", "count", \
	"play-count", "description", "encoding", "added", "thumbnail-uri", \
	"thumbnail", "is-seekable", "resolution", "res-x", "res-y", \
	"comment", "tags", "didl", "artist-info-uri", "album-info-uri", \
	"lyrics-uri", "lyrics", "rating", "composer", "filename", "filesize", \
	"copyright", "protocol-info", "audio-bitrate", "audio-codec", \
	"album-art-uri", "album-art", "video-bitrate", "video-codec", \
        "video-framerate", "bpp", "exif-xml", "icon-uri", "icon", \
        MAFW_METADATA_KEY_CHILDCOUNT_1

typedef struct _BrowseArgs BrowseArgs;

/*----------------------------------------------------------------------------
  Static prototypes
  ----------------------------------------------------------------------------*/

/* MAFW Plugin initialization */
static gboolean mafw_upnp_source_initialize(MafwRegistry* registry,
					    GError** error);
static void mafw_upnp_source_deinitialize(GError** error);

/* GObject initialization */
static void mafw_upnp_source_init(MafwUPnPSource* self);
static void mafw_upnp_source_class_init(MafwUPnPSourceClass* klass);
static void mafw_upnp_source_dispose(GObject* object);

/* UPnP service callbacks */
void mafw_upnp_source_notify_callback(GUPnPServiceProxy* service,
				       const gchar* variable,
				       GValue* value,
				       gpointer user_data);

static void mafw_upnp_source_device_proxy_available(GUPnPControlPoint* cp,
						    GUPnPDeviceProxy* proxy,
						    gpointer user_data);
static void mafw_upnp_source_device_proxy_unavailable(GUPnPControlPoint* cp,
						      GUPnPDeviceProxy* proxy,
						      gpointer user_data);

/* Browse */
static GUPnPServiceProxyAction* mafw_upnp_source_browse_internal(BrowseArgs*
								  args);
static guint mafw_upnp_source_browse(MafwSource *source,
				     const gchar *object_id,
				     gboolean recursive,
				     const MafwFilter *filter,
				     const gchar *sort_criteria,
				     const gchar *const *metadata_keys,
				     guint skip_count,
				     guint item_count,
				     MafwSourceBrowseResultCb browse_cb,
				     gpointer user_data);
static gboolean mafw_upnp_source_cancel_browse(MafwSource *source,
					       guint browse_id,
					       GError **error);

/* Metadata */
static void mafw_upnp_source_get_metadata(MafwSource *source,
					      const gchar *object_id,
					      const gchar *const *metadata_keys,
					      MafwSourceMetadataResultCb cb,
					      gpointer user_data);

/* Common utilities */
static GHashTable *mafw_upnp_source_compile_metadata(gchar** keys,
						     xmlNode* didl_node,
						     const gchar* didl);

/* Search criteria parsing */
static gboolean internal_filter_to_search_criteria(GString *upsc,
						   MafwFilter *maffin,
						   gboolean negate,
						   GError **error);
static gboolean internal_filter_to_search_criteria_complex(
	GString *upsc, MafwFilter *maffin, gboolean negate,
	GError **error);
static gboolean internal_filter_to_search_criteria_simple(
	GString *upsc, MafwFilter *maffin, gboolean negate,
	GError **error);

/*----------------------------------------------------------------------------
  MAFW Plugin construction
  ----------------------------------------------------------------------------*/

G_MODULE_EXPORT MafwPluginDescriptor mafw_upnp_source_plugin_description = {
	{ .name = MAFW_UPNP_SOURCE_PLUGIN_NAME },
	.initialize = mafw_upnp_source_initialize,
	.deinitialize = mafw_upnp_source_deinitialize,
};

static gboolean mafw_upnp_source_initialize(MafwRegistry *registry,
					     GError **error)
{
	mafw_upnp_source_plugin_initialize(registry);
	return TRUE;
}

static void mafw_upnp_source_deinitialize(GError **error)
{
	mafw_upnp_source_plugin_deinitialize();
}

/*----------------------------------------------------------------------------
  The plugin struct that holds all static globals (which are ugly)
  ----------------------------------------------------------------------------*/

typedef struct _MafwUPnPSourcePlugin {

#ifdef HAVE_CONIC /* MAEMO */
	ConIcConnection* conic;
	DBusConnection* dbus_system;
#endif /* MAEMO */

	MafwRegistry* registry;
	GUPnPContext* context;
	GUPnPControlPoint* cp;
	GUPnPDIDLLiteParser* parser;
	guint next_browse_id;
} MafwUPnPSourcePlugin;

/** THE mafw plugin */
static MafwUPnPSourcePlugin* _plugin = NULL;

/**
 * mafw_upnp_source_plugin_gupnp_up:
 *
 * Creates and binds the gupnp framework to currently existing interfaces.
 */
static void mafw_upnp_source_plugin_gupnp_up(void)
{
	GError* error = NULL;

	if (_plugin->context != NULL)
		return;

	_plugin->context = gupnp_context_new(NULL, NULL, 0, &error);
	if (error != NULL)
	{
		g_warning("Unable to create GUPnP context: %s. UPnP servers "
			  "will not be available.", error->message);
		g_error_free(error);
		return;
	}

	/* Create a control point object with MediaServer search target */
	_plugin->cp = gupnp_control_point_new(_plugin->context, "ssdp:all");
	g_assert(_plugin->cp != NULL);

	/* Listen to device alive/byebye messages */
	g_signal_connect(_plugin->cp, "device-proxy-available",
			 G_CALLBACK(mafw_upnp_source_device_proxy_available),
			 _plugin);
	g_signal_connect(_plugin->cp, "device-proxy-unavailable",
			 G_CALLBACK(mafw_upnp_source_device_proxy_unavailable),
			 _plugin);

	/* Create a DIDL-Lite parser object */
	_plugin->parser = gupnp_didl_lite_parser_new();
        g_assert(_plugin->parser != NULL);

	/* Switch the control point on */
        gssdp_resource_browser_set_active(
				GSSDP_RESOURCE_BROWSER(_plugin->cp), TRUE);
}

/**
 * mafw_upnp_source_plugin_gupnp_down:
 *
 * Destroys the gupnp framework.
 */
static void mafw_upnp_source_plugin_gupnp_down(void)
{
	if (_plugin->cp != NULL)
	{
	        gssdp_resource_browser_set_active(
				GSSDP_RESOURCE_BROWSER(_plugin->cp), FALSE);
		g_object_unref(_plugin->cp);
		_plugin->cp = NULL;
	}

	if (_plugin->parser != NULL)
		g_object_unref(_plugin->parser);
	_plugin->parser = NULL;

	/* Destroy context, because it is bound to a nonexisting network
	   interface if we came here from conic message handler. */
	if (_plugin->context != NULL)
		g_object_unref(_plugin->context);
	_plugin->context = NULL;
}

#ifdef HAVE_CONIC /* MAEMO */

static void mafw_upnp_source_plugin_conic_event(ConIcConnection* connection,
						 ConIcConnectionEvent* event,
						 gpointer user_data)
{
	ConIcConnectionStatus status;
	ConIcConnectionError error;
	const gchar* bearer;

	status = con_ic_connection_event_get_status(event);
	error = con_ic_connection_event_get_error(event);

	switch (status)
	{
	case CON_IC_STATUS_CONNECTED:
		/* Create GUPnP stuff only for WLAN connections. This prevents
		   UPnP traffic in a GSM network. */
		bearer = con_ic_event_get_bearer_type(CON_IC_EVENT(event));
		if (bearer != NULL &&
		    (strcmp(bearer, CON_IC_BEARER_WLAN_INFRA) == 0 ||
		     strcmp(bearer, CON_IC_BEARER_WLAN_ADHOC) == 0))
		{
			g_debug("WLAN connection is up.");
			mafw_upnp_source_plugin_gupnp_up();
		}
		else
		{
			g_debug("Non-WLAN connection is up. Ignoring event.");
		}
		break;

	case CON_IC_STATUS_DISCONNECTED:
		/* Since only one connection can be up at a time (thru conic)
		   it shouldn't be necessary to check the bearer type here.
		   If a GSM network was up, this does nothing. Otherwise all
		   GUPnP stuff is destroyed as it should be. */
		g_debug("Network connection is down.");
		mafw_upnp_source_plugin_gupnp_down();
		break;

	case CON_IC_STATUS_DISCONNECTING:
		/* NOP */
		break;

	default:
		g_warning("Unknown network status: %d", status);
		break;
	}
}

#endif /* MAEMO */

void mafw_upnp_source_plugin_initialize(MafwRegistry* registry)
{
	g_debug("Mafw UPnP plugin initializing");

	/* Construct the global plugin struct that governs all sources */
	_plugin = g_new0(MafwUPnPSourcePlugin, 1);
	g_assert(_plugin != NULL);

	/* Store the MafwRegistry so that we don't have to _get() it later */
	_plugin->registry = registry;
	g_object_ref(registry);

	/* Libsoup needs thread support */
	if (g_thread_supported() == FALSE)
		g_thread_init(NULL);

	/* Reset next browse id */
	_plugin->next_browse_id = 0;

#ifdef HAVE_CONIC /* MAEMO */

	/* Initialize the system bus so libconic can receive ICD messages. */
	_plugin->dbus_system = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	dbus_connection_setup_with_g_main(_plugin->dbus_system, NULL);

	/* Create an Internet Connectivity object and start listening
	   to connection events */
	_plugin->conic = con_ic_connection_new();
	g_assert(_plugin->conic != NULL);

	/* Set conic to send events from all events and start receiving them */
	g_signal_connect(_plugin->conic, "connection-event",
			 G_CALLBACK(mafw_upnp_source_plugin_conic_event),
			 NULL);
	g_object_set(_plugin->conic, "automatic-connection-events", TRUE, NULL);

	g_debug("Waiting for ConIC to tell, whether network is up.");

#else

	/* We're operating on a non-armel (non-maemo) environment. Assume
	   network is up and running. */
	mafw_upnp_source_plugin_gupnp_up();

#endif /* MAEMO */
}

void mafw_upnp_source_plugin_deinitialize(void)
{
	g_assert(_plugin != NULL);

	mafw_upnp_source_plugin_gupnp_down();

#ifdef HAVE_CONIC /* MAEMO */
	if (_plugin->conic != NULL)
		g_object_unref(_plugin->conic);
	_plugin->conic = NULL;

	dbus_connection_unref(_plugin->dbus_system);
#endif /* MAEMO */

	g_object_unref(_plugin->registry);
	_plugin->registry = NULL;

	g_free(_plugin);
	_plugin = NULL;

	g_debug("Mafw UPnP plugin deinitialized");
}

/*----------------------------------------------------------------------------
  UPnP source object definition
  ----------------------------------------------------------------------------*/

G_DEFINE_TYPE(MafwUPnPSource, mafw_upnp_source, MAFW_TYPE_SOURCE);

#define MAFW_UPNP_SOURCE_GET_PRIVATE(object)				\
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), MAFW_TYPE_UPNP_SOURCE,	\
				      MafwUPnPSourcePrivate))

struct _MafwUPnPSourcePrivate {
	/* The UPnP device providing a CDS service */
	GUPnPDeviceProxy* device;

	/* The CDS (ContentDirectoryService) provided by this device */
	GUPnPServiceProxy* service;

	/* browse_id => GUPnPServiceProxyAction associations for ->cancel(). */
	GTree *browses;
};

static void mafw_upnp_source_init(MafwUPnPSource *self)
{
	MafwUPnPSourcePrivate *priv = NULL;

	g_return_if_fail(MAFW_IS_UPNP_SOURCE(self));
	priv = self->priv = MAFW_UPNP_SOURCE_GET_PRIVATE(self);
	priv->browses = g_tree_new_full(
		(GCompareDataFunc) util_compare_uint,
		NULL, NULL, NULL);
}

static void mafw_upnp_source_class_init(MafwUPnPSourceClass *klass)
{
	GObjectClass *gobject_class;
	MafwSourceClass *source_class;

	g_return_if_fail(klass != NULL);

	gobject_class = G_OBJECT_CLASS(klass);
	source_class = MAFW_SOURCE_CLASS(klass);

	gobject_class->dispose = mafw_upnp_source_dispose;

	g_type_class_add_private(gobject_class, sizeof(MafwUPnPSourcePrivate));

	source_class->browse = mafw_upnp_source_browse;
	source_class->cancel_browse = mafw_upnp_source_cancel_browse;
	source_class->get_metadata = mafw_upnp_source_get_metadata;
}

static void mafw_upnp_source_dispose(GObject *object)
{
	MafwUPnPSource *self = MAFW_UPNP_SOURCE(object);
	MafwUPnPSourcePrivate *priv = NULL;
	MafwSourceClass *parent_class;
	MafwUPnPSourceClass *klass;

	klass = MAFW_UPNP_SOURCE_GET_CLASS(object);

	parent_class = g_type_class_peek_parent(klass);
	priv = MAFW_UPNP_SOURCE_GET_PRIVATE(self);

	g_assert(self != NULL);
	g_assert(priv != NULL);

	/* Get rid of browse IDs. No need to cancel the actions since
	   GUPnP does it for us. */
	g_tree_destroy(priv->browses);

	if (priv->device != NULL) {
		g_object_unref(priv->device);
		priv->device = NULL;
	}

	if (priv->service != NULL) {
		g_object_unref(priv->service);
		priv->service = NULL;
	}

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/*----------------------------------------------------------------------------
  Public API
  ----------------------------------------------------------------------------*/

GObject *mafw_upnp_source_new(const gchar *name, const gchar *uuid)
{
	return g_object_new(MAFW_TYPE_UPNP_SOURCE,
			    "plugin", MAFW_UPNP_SOURCE_PLUGIN_NAME,
			    "name", name,
			    "uuid", uuid,
			    NULL);
}

/*----------------------------------------------------------------------------
  UPnP Proxy listeners
  ----------------------------------------------------------------------------*/

void mafw_upnp_source_notify_callback(GUPnPServiceProxy* service,
				       const gchar* variable,
				       GValue* value,
				       gpointer user_data)
{
	MafwExtension* self;

	self = MAFW_EXTENSION(user_data);
	g_assert(self != NULL);

	g_assert(service != NULL);
	g_assert(variable != NULL);

	g_debug("CDS [%s] notification for [%s]:",
		mafw_extension_get_name(MAFW_EXTENSION(self)), variable);

	if (strcmp(variable, CONTAINER_UPDATE_IDS) == 0)
	{
		gchar** ids;
		gchar* oid;
		int i;

		/* Send a signal for each changed container object ID */
		ids = g_strsplit(g_value_get_string(value), ",", 0);
		for (i = 0; i < g_strv_length(ids); i++)
		{
			oid = g_strdup_printf("%s::%s",
				mafw_extension_get_uuid(self), ids[i]);
			g_signal_emit_by_name(self, "container-changed", oid);
			g_free(oid);
		}

		g_strfreev(ids);
	}
}

/**
 * mafw_upnp_source_attach_proxy:
 *
 * Store the UPnP device & CDS service to the object's private structure.
 **/
static void mafw_upnp_source_attach_proxy(MafwUPnPSource* self,
					  GUPnPDeviceProxy* device,
					  GUPnPServiceProxy* service)
{
	MafwUPnPSourcePrivate* priv;

	g_assert(MAFW_IS_UPNP_SOURCE(self));
	priv = MAFW_UPNP_SOURCE_GET_PRIVATE(self);
	g_assert(priv != NULL);

	/* Keep a reference to the CDS service */
	priv->service = service;
	g_object_ref(priv->service);

	/* Keep a reference to the CDS server device */
	priv->device = device;
	g_object_ref(priv->device);

	/* Subscribe to service events */
	if (gupnp_service_proxy_get_subscribed(service) == FALSE)
		gupnp_service_proxy_set_subscribed(service, TRUE);
	if (gupnp_service_proxy_add_notify(service,
					   CONTAINER_UPDATE_IDS,
					   G_TYPE_STRING,
					   mafw_upnp_source_notify_callback,
					   self) == FALSE)
	{
		g_warning("Subscription of %s for CDS [%s] failed",
			CONTAINER_UPDATE_IDS,
			mafw_extension_get_name(MAFW_EXTENSION(self)));
	}
}

static void mafw_upnp_source_device_proxy_available(GUPnPControlPoint* cp,
						     GUPnPDeviceProxy* device,
						     gpointer user_data)
{
	GUPnPServiceProxy* service;
	MafwExtension* extension;
	gchar* uuid;
	gchar* name;
	const char *type;

	g_assert(device != NULL);

	type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (device));
	if (!g_pattern_match_simple (
			"urn:schemas-upnp-org:device:MediaServer:*", type)) {
		return;
	}

	/* Get the device UDN and strip the "uuid:" part away because it
	   confuses DBus. */
	uuid = util_udn_to_uuid(gupnp_device_info_get_udn(
					GUPNP_DEVICE_INFO(device)));
	g_assert(uuid != NULL);

	extension = mafw_registry_get_extension_by_uuid(_plugin->registry,
							uuid);
	if (extension != NULL) {
		/* We already have a proxy of this device, ignore */
		return;
	}

	name = gupnp_device_info_get_friendly_name(GUPNP_DEVICE_INFO(device));
	g_assert(name != NULL);

	/* Try to find a Content Directory Service from the device */
	service = GUPNP_SERVICE_PROXY(
		gupnp_device_info_get_service(GUPNP_DEVICE_INFO(device),
					      CONTENT_DIR_NO_VERSION));
	if (service != NULL)
	{
		MafwSource* source;

		g_assert(MAFW_IS_REGISTRY(_plugin->registry));

		/* New source can be created */
		g_debug("UPnP CDS available."
				"\n\tName:[%s]\n\tUUID:[%s]", name, uuid);

		source = MAFW_SOURCE(mafw_upnp_source_new(name,
								    uuid));
		mafw_upnp_source_attach_proxy(
				MAFW_UPNP_SOURCE(source), device, service);

		mafw_registry_add_extension(_plugin->registry,
					       MAFW_EXTENSION(source));

		g_object_unref(service);
	}

	g_free(name);
	g_free(uuid);
}

static void mafw_upnp_source_device_proxy_unavailable(
	GUPnPControlPoint* cp, GUPnPDeviceProxy* device, gpointer user_data)
{
	gchar* uuid;
	MafwSource* source;

	g_assert(device != NULL);
	g_assert(MAFW_IS_REGISTRY(_plugin->registry));

	uuid = util_udn_to_uuid(gupnp_device_info_get_udn(
					GUPNP_DEVICE_INFO(device)));
	g_assert(uuid != NULL);

	/* Attempt to find a source by the proxy's UUID */
	source = MAFW_SOURCE(mafw_registry_get_extension_by_uuid(
						_plugin->registry, uuid));
	if (source != NULL)
	{
		/* Source found. Remove it. */
		g_debug("UPnP CDS service no longer available."
			"\n\tName:[%s]\n\tUUID:[%s]",
			 mafw_extension_get_name(MAFW_EXTENSION(source)),
			 mafw_extension_get_uuid(MAFW_EXTENSION(source)));

		mafw_registry_remove_extension(_plugin->registry,
					   MAFW_EXTENSION(source));
	}

	g_free(uuid);
}

/*----------------------------------------------------------------------------
  Common utilities
  ----------------------------------------------------------------------------*/

/**
 * mafw_upnp_source_compile_metadata:
 * @keys:      A list of requested metadata keys (originating from a UI or renderer)
 * @didl_node: Parsed xmlNode structure from a successful browse action,
 *             containing a number of DIDL-Lite item/container nodes.
 * @didl:      Non-parsed raw string-form DIDL-Lite XML document
 *
 * Compiles requested metadata keys and their values into a #GHashTable that
 * can be sent back to the requesting UI/Renderer.
 *
 * Returns: A #GHashTable containing key-value pairs. Must be freed after use.
 */
static GHashTable *mafw_upnp_source_compile_metadata(gchar** keys,
						     xmlNode *didl_node,
						     const gchar* didl)
{
	GHashTable* metadata;
	int i;

	/* Requested metadata keys */
	metadata = mafw_metadata_new();

	for (i = 0; keys != NULL && keys[i] != NULL; i++)
	{
		gchar* name;
		gchar* value;
		gint number;

		name = keys[i];

		if (strcmp(name, MAFW_METADATA_KEY_URI) == 0)
		{
			value = didl_get_http_res_uri(didl_node);
			if (value != NULL && strlen(value) > 0)
				mafw_metadata_add_str(metadata, name, value);
			g_free(value);
		}
		else if (strcmp(name, MAFW_METADATA_KEY_CHILDCOUNT_1) == 0)
		{
			number = didl_get_childcount(didl_node);
			if (number >= 0)
				mafw_metadata_add_int(metadata, name, number);
		}
		else if (strcmp(name, MAFW_METADATA_KEY_MIME) == 0)
		{
			value = didl_get_mimetype(didl_node);
			if (value != NULL && strlen(value) > 0)
				mafw_metadata_add_str(metadata, name, value);
			g_free(value);
		}
		else if (strcmp(name, MAFW_METADATA_KEY_DURATION) == 0)
		{
			number = didl_get_duration(didl_node);
			if (number >= 0)
				mafw_metadata_add_int(metadata, name, number);
		}
		else if (strcmp(name, MAFW_METADATA_KEY_THUMBNAIL_URI) == 0)
		{
			value = didl_get_thumbnail_uri(didl_node);
			if (value != NULL && strlen(value) > 0)
				mafw_metadata_add_str(metadata, name, value);
			g_free(value);
		}
		else if (strcmp(name, MAFW_METADATA_KEY_DIDL) == 0)
		{
			if (didl != NULL && strlen(didl) > 0)
				mafw_metadata_add_str(metadata, name, didl);
		}
		else if (strcmp(name, MAFW_METADATA_KEY_IS_SEEKABLE) == 0)
		{
			gint8 seekability;

			seekability = didl_get_seekability(didl_node);
			if (seekability != -1) {
				mafw_metadata_add_boolean(metadata, name,
							  seekability);
			}
		}
		else
		{
			gint type = G_TYPE_INVALID;
			value = didl_fallback(didl_node, name, &type);
			if (value != NULL && strlen(value) > 0)
			{
				if (type == G_TYPE_INT)
					mafw_metadata_add_int(metadata, name,
							      atoi(value));
				else if (type == G_TYPE_STRING)
					mafw_metadata_add_str(metadata, name,
							      value);
				g_free(value);
			}
		}
	}

	return metadata;
}

/*----------------------------------------------------------------------------
  Search criteria parsing
  ----------------------------------------------------------------------------*/

static gboolean internal_filter_to_search_criteria_simple(
	GString *upsc, MafwFilter *maffin, gboolean negate,
	GError **error)
{
	const gchar* didl_key;

	g_assert(upsc != NULL);

	didl_key = didl_mafwkey_to_upnp_filter(maffin->key);
	g_string_append(upsc, didl_key);

	/* Since protocolInfo contains sub-strings like MIME type and protocol,
	   we must change the exact match to approximate match. UPnP doesn't
	   seem to support wildcards in protocolInfo fields during searching. */
	if (strcmp(didl_key, DIDL_RES "@" DIDL_RES_PROTOCOL_INFO) == 0)
	{
		if (maffin->type == mafw_f_eq)
			maffin->type = mafw_f_approx;
	}

	/* Convert the MAFW filter type into a UPnP type. */
	if (maffin->type == mafw_f_eq)
	{
		if (negate == TRUE)
			g_string_append(upsc, " != ");
		else
			g_string_append(upsc, " = ");
	}
	else if (maffin->type == mafw_f_lt)
	{
		if (negate == TRUE)
			g_string_append(upsc, " >= ");
		else
			g_string_append(upsc, " < ");
	}
	else if (maffin->type == mafw_f_gt)
	{
		if (negate == TRUE)
			g_string_append(upsc, " <= ");
		else
			g_string_append(upsc, " > ");
	}
	else if (maffin->type == mafw_f_approx)
	{
		if (negate == TRUE)
			g_string_append(upsc, " doesNotContain ");
		else
			g_string_append(upsc, " contains ");
	}
	else if (maffin->type == mafw_f_exists)
	{
		if (negate == TRUE)
			g_string_append(upsc, " exists false");
		else
			g_string_append(upsc, " exists true");
	}
	else
	{
		g_assert_not_reached();
	}

	/* If this is binary operation then append the right value */
	if (maffin->type != mafw_f_exists)
	{
		const gchar *str;

		g_assert(maffin->value != NULL);

		g_string_append_c(upsc, '"');
		str = maffin->value;
		while (*str)
		{
			gchar c;

			/*
			 * UPnP can search for a substring, but not
			 * for multiple substrings like "alpha*beta"
			 * in a property value.  Therefore reject the
			 * wildcard in the middle if we're matching
			 * approximately.
			 */
			if (maffin->type == mafw_f_approx && *str == '*')
			{
				if (str == maffin->value || *(str+1) == '\0')
				{
					str++;
					continue;
				}
				else
				{
					g_set_error(error,
						    MAFW_SOURCE_ERROR,
						    MAFW_SOURCE_ERROR_INVALID_SEARCH_STRING,
						    "Wildcards in the middle "
						    "of approximated property "
						    "values are not supported");
					return FALSE;
				}
			}

			/* mafw_filter_unquote_char() only returns NULL
			 * if $maffin->value is syntactically incorrect. */
			str = mafw_filter_unquote_char(str, &c);
			if (str == NULL)
			{
				g_set_error(error,
					    MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_INVALID_SEARCH_STRING,
					    "Invalid escape sequence");
				return FALSE;
			}

			/* We can't do anything about NILs for they
			 * are interpreted as terminators by gupnp
			 * and there is no way to quote them. */
			if (c == '\0')
			{
				g_set_error(error,
					    MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_INVALID_SEARCH_STRING,
					    "NIL in property value");
				return FALSE;
			}

			if (c == '\\' || c == '"')
				g_string_append_c(upsc, '\\');
			g_string_append_c(upsc, c);
		}

		g_string_append_c(upsc, '"');
	}

	return TRUE;
}

static gboolean internal_filter_to_search_criteria_complex(
	GString *upsc, MafwFilter *maffin, gboolean negate,
	GError **error)
{
	const gchar *op;
	MafwFilter *const *sexp;

	g_assert(maffin != NULL);

	if (maffin->type == mafw_f_not)
	{
		/* There is no NOT operator in UPnP search strings,
		 * so we need to negate all individual terms. */
		negate = !negate;
		op = NULL;
	}
	else if (maffin->type == mafw_f_and)
	{
		if (negate == TRUE)
			op = " or ";
		else
			op = " and ";
	}
	else if (maffin->type == mafw_f_or)
	{
		if (negate == TRUE)
			op = " and ";
		else
			op = " or ";
	}
	else
	{
		g_assert_not_reached();
	}

	g_assert(maffin->parts != NULL && maffin->parts[0] != NULL);

	if (maffin->parts[1] != NULL)
	{
		/* Has more than one subexpressions. */
		for (sexp = maffin->parts; *sexp; sexp++)
		{
			g_string_append_c(upsc, '(');

			if (internal_filter_to_search_criteria(
				    upsc, *sexp, negate, error) == FALSE)
			{
				return FALSE;
			}

			g_string_append_c(upsc, ')');

			if (*(sexp + 1))
			{
				/* mafw_f_not expressions are not
				 * supposed to have multiple parts. */
				g_assert(op != NULL);
				g_string_append(upsc, op);
			}
		}
	}
	else if (internal_filter_to_search_criteria(upsc, maffin->parts[0],
						    negate, error) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * mafw_upnp_source_filter_to_search_criteria:
 * @upsc:   UPnP-style search criteria
 * @maffin: MAFW-style browse filter string
 * @negate: Switch the whole expression into a NOT operation
 * @error:  An error pointer that is set if errors occur
 *
 * Parses a MafwFilter and writes the translated expressions to @upsc.
 * @negate tells it to negate all (sub)expressions, which is used to
 * emulate the NOT operator of LDAP that is not present in UPnP.
 *
 * NOTE: Remember to free @upsc.
 *
 * Returns: %TRUE if successful; otherwise %FALSE
 */
static gboolean internal_filter_to_search_criteria(GString *upsc,
						   MafwFilter *maffin,
						   gboolean negate,
						   GError **error)
{
	if (MAFW_FILTER_IS_SIMPLE(maffin))
	{
		return internal_filter_to_search_criteria_simple(upsc,
								 maffin,
								 negate,
								 error);
	}
	else
	{
		return internal_filter_to_search_criteria_complex(upsc,
								  maffin,
								  negate,
								  error);
	}
}

/**
 * mafw_upnp_source_filter_to_search_criteria:
 * @mafw_filter: The MAFW-style filter string to convert
 * @error:       An error pointer
 *
 * Converts a MAFW browse filter string to a UPnP SearchCriteriaString.
 *
 * Returns: A converted UPnP-style search criteria string or NULL if
 *          parsing fails.
 */
static gchar *mafw_upnp_source_filter_to_search_criteria(
	const MafwFilter *filter, GError **error)
{
	GString *search_criteria;
	MafwFilter *maffin;
	gchar* str;

	if (filter == NULL)
	{
		return NULL;
	}

	/* Convert the internal filter representation to UPnP. */
	search_criteria = g_string_new(NULL);
	maffin = mafw_filter_copy(filter);
	if (internal_filter_to_search_criteria(search_criteria,
					       maffin,
					       FALSE,
					       error) == TRUE)
	{
		str = search_criteria->str;

		/* Don't free the GString's gchar* array -> FALSE */
		g_string_free(search_criteria, FALSE);
	}
	else
	{
		str = NULL;

		/* Free also the GString's gchar* array, if any -> TRUE */
		g_string_free(search_criteria, TRUE);
	}

	mafw_filter_free(maffin);

	return str;
}

/*---------------------------------------------------------------------------
  Browse Arguments
  ---------------------------------------------------------------------------*/

/** Struct that holds all necessary information for a single browse action */
struct _BrowseArgs
{
	/*-------------------------------------------------------------------
	  Static parameters passed in mafw_source_browse() call
	  -------------------------------------------------------------------*/

	/** The particular UPnP server instance that is being browsed */
	MafwUPnPSource* source;

	/** The UPnP CDS Item ID that is being browsed */
	gchar* itemid;

	/** Filter string converted to a UPnP search criteria */
	gchar* search_criteria;

	/** Copied sort criteria string */
	gchar* sort_criteria;

	/** Requested metadata keys (copied) */
	gchar** meta_keys;

	/** Requested metadata keys in a comma-separated string */
	gchar* meta_keys_csv;

	/** Original skip count */
	guint skip_count;

	/** Original item count (total number of items the user wants) */
	guint item_count;

	/** Original requested count (number of items requested at a time) */
	guint requested_count;

	/** User callback function & its user data */
	MafwSourceBrowseResultCb callback;
	gpointer user_data;

	/*-------------------------------------------------------------------
	  Run-time parameters
	  -------------------------------------------------------------------*/

	/** The browse/search action associated with these args */
	GUPnPServiceProxyAction* action;

	/** ID of the current browse operation */
	guint browse_id;

	/** Number of items remaining to be fetched. */
	guint remaining_count;

	/** Number of items returned by the CDS in response to the request. */
	guint number_returned;

	/** Total number of items in the container currently browsed. */
	guint total_matches;

	/** Index of the next emitted item */
	guint current;

	/** Reference count */
	guint refcount;
};

/**
 * Increase BrowseArgs* reference count. Reference counting is needed because
 * this source sends results back to the user in multiple idle callbacks.
 */
static BrowseArgs* browse_args_ref(BrowseArgs* args)
{
	g_assert(args != NULL);

	if (++args->refcount == 1)
	{
		/* Take only one reference to the source object */
		g_object_ref(args->source);
	}

	return args;
}

/**
 * Decrease BrowseArgs* reference count. See browse_args_ref() for reasons.
 */
static void browse_args_unref(BrowseArgs* args)
{
	g_assert(args != NULL && args->refcount > 0);

	if (--args->refcount == 0)
	{
		/* Remove the browse ID and this args struct from our list
		   of cancellable browse operations */
		if (g_tree_remove(args->source->priv->browses,
				  GUINT_TO_POINTER(args->browse_id)) == FALSE)
		{
			g_assert_not_reached();
		}

		/* If remaining count > 0, then the action was probably
		   cancelled, so we must send the final result indicating EOF.
		*/
		if (args->remaining_count > 0)
		{
			args->callback(MAFW_SOURCE(args->source),
				       args->browse_id, 0, 0, NULL, NULL,
				       args->user_data, NULL);
		}

		g_object_unref(args->source);
		g_free(args->itemid);
		g_free(args->search_criteria);
		g_free(args->sort_criteria);
		g_strfreev(args->meta_keys);
		g_free(args->meta_keys_csv);
		g_free(args);
	}
}

/*----------------------------------------------------------------------------
  Browse
  ----------------------------------------------------------------------------*/

/**
 * mafw_upnp_source_browse_result:
 * @parser:    The DIDL-Lite parser object that is parsing browse results
 * @didl_node: A single DIDL-Lite object (item/container) node
 * @user_data: #BrowseArgs*
 *
 * Parses each item/container from a successful browse action, one by one
 * (decided by the #GUPnPDIDLLiteParser). Then, returns the results for the
 * whole set in one go using the user-given callback function.
 */
static void mafw_upnp_source_browse_result(GUPnPDIDLLiteParser* parser,
					    xmlNode* didl_node,
					    gpointer user_data)
{
	GHashTable* metadata;
	BrowseArgs* args;
	gchar* objectid;
	gint current;

	args = (BrowseArgs*) user_data;
	g_assert(args != NULL);
	g_assert(args->callback != NULL);

	/* Create a MAFW-style object ID for this item node. If an
	   ID cannot be found, this node might be a <desc> node, which
	   can be skipped with good conscience. */
	objectid = util_create_objectid(args->source, didl_node);

	/* If there was no object ID, this node might be a <desc> node
	   which must not be exposed to the user and thus not counted
	   in skip_count, either. */
	if (objectid == NULL)
		return;

	/* Gather requested metadata information from DIDL-Lite */
	metadata = mafw_upnp_source_compile_metadata(args->meta_keys,
						      didl_node, NULL);

	/* Calculate remaining count and current item's index. */
	current = args->current++;
	if (args->item_count == 0) {
		/* All items were requested. */
		args->remaining_count = args->total_matches - args->current;
	} else if (args->number_returned < args->item_count) {
		args->remaining_count = args->number_returned - args->current;
	} else {
		args->remaining_count =	args->item_count - args->current;
	}

	/* Emit results */
	args->callback(MAFW_SOURCE(args->source),
		       args->browse_id,
		       args->remaining_count,
		       current,
		       objectid,
		       metadata,
		       args->user_data,
		       NULL);
	/* Free the compiled metadata and MAFW-style object ID */
	g_hash_table_unref(metadata);
	g_free(objectid);
}

/**
 * mafw_upnp_source_browse_cb:
 * @service:   A CDS Service proxy that completed a browse action
 * @action:    The completed browse action
 * @user_data: #BrowseArgs*
 *
 * Callback that is called when results from a browse action invocation are
 * received. Parses the resulting DIDL-Lite node tree and sends the requested
 * information (if found) back to the requester.
 */
static void mafw_upnp_source_browse_cb(GUPnPServiceProxy* service,
					GUPnPServiceProxyAction* action,
					gpointer user_data)
{
	BrowseArgs* args = (BrowseArgs*) user_data;
	GError* gupnp_error = NULL;
	gboolean result = FALSE;
	gchar* didl = NULL;

	g_assert(args != NULL);

	/* This action was completed, remove it from args because it cannot be
	   cancelled anymore, since this function runs all the way thru the
	   returned set of results and returns to main loop after it's done. */
	args->action = NULL;

	/* Parse the action result and number of items returned in this set */
	result = gupnp_service_proxy_end_action(
		service, action, &gupnp_error,
		"Result",         G_TYPE_STRING, &didl,
		"NumberReturned", G_TYPE_UINT,   &args->number_returned,
		"TotalMatches",   G_TYPE_UINT,   &args->total_matches,
		NULL);

	g_debug("CDS server with UUID [%s] browse result consists of:"
		"\tNumberReturned: %d\n"
		"\tTotalMatches: %d\n",
		mafw_extension_get_uuid(MAFW_EXTENSION(args->source)),
		args->number_returned, args->total_matches);

	if (result == FALSE || didl == NULL || args->total_matches == 0)
	{
		/* Action failed completely, no results. */
		GError* error = NULL;
		if (gupnp_error != NULL)
		{
			g_warning("GUPnP error: %s", gupnp_error->message);

			/* g_set_error() takes its message argument as a
			 * printf() format string.  gupnp_error->message
			 * may contain format specifiers (XML fragments). */
			g_set_error(&error,
				    MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_BROWSE_RESULT_FAILED,
				    "GUPnP: %s", gupnp_error->message);
			g_error_free(gupnp_error);
		}

		/* Call the callback function with invalid values and an error.
		 * Zero out remaining_count, otherwise browse_args_unref()
		 * will try to terminate the session again. */
		if (args->remaining_count > 0)
		{
			args->callback(MAFW_SOURCE(args->source),
				       args->browse_id, 0, 0, NULL, NULL,
				       args->user_data, error);
			args->remaining_count = 0;
		}

		if (error) {
			g_error_free(error);
		}
	}
	else
	{
		/* Parse the DIDL-Lite into an xmlNode tree and parse them
		   one by one, using mafw_upnp_source_browse_result() */
		gupnp_didl_lite_parser_parse_didl(
			_plugin->parser,
			didl,
			mafw_upnp_source_browse_result,
			args,
			&gupnp_error);

		if (gupnp_error != NULL)
		{
			/* DIDL-Lite parsing failed */

			GError* error = NULL;
			g_set_error(&error,
				    MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_BROWSE_RESULT_FAILED,
				    "GUPnP: %s", gupnp_error->message);

			/* Call the callback function with invalid values and
			   an error. */
			if (args->remaining_count > 0)
			{
				g_warning("GUPnP error: %s."
					  "Terminating browse session.",
					  gupnp_error->message);

				args->callback(MAFW_SOURCE(args->source),
					       args->browse_id, 0, 0, NULL, NULL,
					       args->user_data, error);
				args->remaining_count = 0;
			}

			g_error_free(error);
			g_error_free(gupnp_error);
		}
		/* Continue incremental browse only, if:
		 * 1. There are items left in the server to browse
		 * (2. The server returned at least requested_count items)
		 * 3. All items were requested, or
		 * 4. the next skip_count won't go beyond the requested count
		 */
		else if (args->remaining_count <= 0)
		{
			/* There are no more items left to browse. Stop. */
		}
		/* This happens when no result was obtained in the browse operation.
		   In  this case, mafw_upnp_source_browse_result is not invoked,
		   so the remaining_count has not been modified (should be
		   INT_MAX) and the user callback was never invoked */
		else if (args->number_returned == 0)
		{
			args->callback(MAFW_SOURCE(args->source),
				       args->browse_id, 0, 0, NULL, NULL,
				       args->user_data, NULL);
		}
#if 0
		/* Uncommenting this makes DLNA CTT 7.3.64.10 fail, but this
		   commented version might produce other problems... */
		else if (args->number_returned < args->requested_count)
		{
			/* The latest browse returned less items than what
			   was requested. It is quite likely that we got all
			   of them. Stop. */
		}
#endif
		else if (args->item_count != 0 &&
			 args->current >= (args->item_count - 1))
		{
			/* All items were not requested (0 == all) and we got
			   already all that we wanted. Stop */
		}
		else
		{
			/* Browse the next increment. */
			args->action = mafw_upnp_source_browse_internal(args);
		}
	}

	g_free(didl);
	browse_args_unref(args);
}

static GUPnPServiceProxyAction* mafw_upnp_source_browse_internal(
	BrowseArgs* args)
{
	GUPnPServiceProxyAction *action;
	gint skip_count;

	g_assert(args != NULL);

	browse_args_ref(args);

	skip_count = args->skip_count + args->current;

	if (args->item_count == 0)
	{
		/* All items were requested by the user. Get default amount. */
		args->requested_count = DEFAULT_REQUESTED_COUNT;
	}
	else
	{
		/* A specific number of items was requested by the user.
		   Choose the smaller value between DEFAULT and items left. */
		args->requested_count = MIN(DEFAULT_REQUESTED_COUNT,
					    args->item_count);
	}

	g_debug("Browse increment: %s\n\tSkip: %d -- Count: %d\n",
		args->itemid, skip_count, args->requested_count);

	if (args->search_criteria == NULL)
	{
		action = gupnp_service_proxy_begin_action(
			args->source->priv->service,
			"Browse",         mafw_upnp_source_browse_cb, args,
			"ObjectID",       G_TYPE_STRING, args->itemid,
			"BrowseFlag",     G_TYPE_STRING, "BrowseDirectChildren",
			"Filter",         G_TYPE_STRING, args->meta_keys_csv,
			"StartingIndex",  G_TYPE_UINT,   skip_count,
			"RequestedCount", G_TYPE_UINT,   args->requested_count,
			"SortCriteria",   G_TYPE_STRING, args->sort_criteria,
			NULL);
	}
	else
	{
		action = gupnp_service_proxy_begin_action(
			args->source->priv->service,
			"Search",         mafw_upnp_source_browse_cb, args,
			"ContainerID",    G_TYPE_STRING, args->itemid,
			"SearchCriteria", G_TYPE_STRING, args->search_criteria,
			"Filter",         G_TYPE_STRING, args->meta_keys_csv,
			"StartingIndex",  G_TYPE_UINT,   skip_count,
			"RequestedCount", G_TYPE_UINT,   args->requested_count,
			"SortCriteria",   G_TYPE_STRING, args->sort_criteria,
			NULL);
	}

	return action;
}

/**
 * Convert a MAFW-style sort criteria string to contain UPnP-style keys.
 */
static gchar* mafw_sort_criteria_to_upnp(const gchar* mafw_sc)
{
	const gchar* key;
	gchar* upnp_sc;
	gchar** crit_array;
	const gchar* order;
	gchar *temp;
	int i;

	/* If there is no sort criteria, don't try to convert it */
	if (mafw_sc == NULL)
		return NULL;

	upnp_sc = g_strdup("");

	crit_array = g_strsplit(mafw_sc, ",", 0);
	for (i = 0; crit_array[i] != NULL; i++)
	{
		/* Check, which order to use. Fail if there isn't one. */
		if (crit_array[i][0] == '+')
		{
			order = "+";
		}
		else if (crit_array[i][0] == '-')
		{
			order = "-";
		}
		else
		{
			g_free(upnp_sc);
			upnp_sc = NULL;
			break;
		}

		/* Skip the order marker with + 1 */
		key = didl_mafwkey_to_upnp_filter(crit_array[i] + 1);

		/* Catenate the upnp-ified keys to a single CSV */
		temp = g_strconcat(upnp_sc, order, key, NULL);
		g_free(upnp_sc);
		upnp_sc = temp;

		/* Don't append a comma unless there are still keys left */
		if (crit_array[i + 1] != NULL)
			upnp_sc = g_strconcat(upnp_sc, ",", NULL);
	}

	g_strfreev(crit_array);

	return upnp_sc;
}

/**
 * See mafw_source_browse() for more information.
 */
static guint mafw_upnp_source_browse(MafwSource *source,
				      const gchar *object_id,
				      gboolean recursive,
				      const MafwFilter *filter,
				      const gchar *sort_criteria,
				      const gchar *const *metadata_keys,
				      guint skip_count,
				      guint item_count,
				      MafwSourceBrowseResultCb browse_cb,
				      gpointer user_data)
{
	GUPnPServiceProxyAction* action;
	MafwUPnPSource* self;
	BrowseArgs* args;
	gchar* upsc;
	gchar* upnp_sort_criteria;
	const gchar* const* meta_keys;
	gchar* itemid;
	GError *error = NULL;

	self = MAFW_UPNP_SOURCE(source);
	g_assert(self != NULL);
	g_assert(browse_cb != NULL);

	/* If metadata_keys is empty (but not NULL), or it contains an asterisk,
	   it means that ALL metadata keys are being requested */
	if (metadata_keys != NULL && metadata_keys[0] != NULL &&
	    strcmp(MAFW_SOURCE_ALL_KEYS[0], metadata_keys[0]) == 0)
	{
		meta_keys = MAFW_SOURCE_LIST(KNOWN_METADATA_KEYS);
	}
	else
	{
		meta_keys = metadata_keys;
	}

	/* Split the object ID to get the item part, after "::" */
	itemid = NULL;
	mafw_source_split_objectid(object_id, NULL, &itemid);
	if (itemid == NULL || strlen(itemid) == 0)
		itemid = g_strdup("0");

	/* Construct the UPnP SearchCriteria if $filter is specified. */
	if (filter == NULL)
	{
		upsc = NULL;
	}
	else
	{
		upsc = mafw_upnp_source_filter_to_search_criteria(filter,
								   &error);
		if (upsc == NULL)
		{
			g_debug("Wrong filter");
			if (browse_cb)
			{
				browse_cb(source, MAFW_SOURCE_INVALID_BROWSE_ID,
						0, 0, NULL,
						NULL, user_data, error);
			}
			g_error_free(error);
			g_free(itemid);
			return MAFW_SOURCE_INVALID_BROWSE_ID;
		}
	}

	/* Convert Mafw sort criteria to UPnP style. If there is no sort
	   criteria, use an empty string. Some servers don't support sort
	   criteria setting and fail completely if it is given. */
	upnp_sort_criteria = mafw_sort_criteria_to_upnp(sort_criteria);
	if (upnp_sort_criteria == NULL)
		upnp_sort_criteria = g_strdup("");

	/*
	 * Register the current browseid now.  This is necessary because
	 * gupnp_service_proxy_begin_action() may smartly call the callback
	 * (which removes the entry) before it returns.  To avoid state
	 * entries in ->browses we need to add it before beginning the
	 * action.
	 */
	g_assert(!g_tree_lookup_extended(self->priv->browses,
				 GUINT_TO_POINTER(_plugin->next_browse_id),
				 NULL, NULL));
	g_tree_insert(self->priv->browses,
		      GUINT_TO_POINTER(_plugin->next_browse_id), NULL);

	/* Some parameters we need to pass to the browse callbacks */
	args = g_new0(BrowseArgs, 1);
	args->source = self;
	args->itemid = itemid; /* Already strdupped */
	args->search_criteria = upsc;
	args->sort_criteria = upnp_sort_criteria;
	args->meta_keys = util_strvdup(meta_keys);
	args->meta_keys_csv = didl_mafwkey_array_to_upnp_filter(
				    (const gchar *const *)args->meta_keys);
	args->skip_count = skip_count;
	args->item_count = item_count;
	args->callback = browse_cb;
	args->user_data = user_data;
	args->browse_id = _plugin->next_browse_id;
	args->remaining_count = INT_MAX;

	g_debug("Browse: %s\n"
		"\tID: %u\n"
		"\tKeys: %s\n"
		"\tSort: %s\n"
		"\tSearch: %s",
		object_id, args->browse_id, args->meta_keys_csv,
		args->sort_criteria, args->search_criteria);

	/* Invoke the browse action on the given object (container) id */
	action = mafw_upnp_source_browse_internal(args);
	if (action == NULL)
	{
		g_warning("Unable to initiate browse. Terminating session.");
		if (browse_cb)
		{
			g_set_error(&error, MAFW_SOURCE_ERROR,
				MAFW_SOURCE_ERROR_PEER,
				"Unable to initiate browse.");
			browse_cb(source, MAFW_SOURCE_INVALID_BROWSE_ID,
					0, 0, NULL,
					NULL, user_data, error);
			g_error_free(error);
		}

		/* Action invocation failed before it even begun */
		browse_args_unref(args);
		return MAFW_SOURCE_INVALID_BROWSE_ID;
	}
	else
	{
		/* Set the new action as the current browse operation's action
		   so that we can cancel it. */
		args->action = action;

		/* Save args struct so we can cancel its action later */
		g_tree_insert(self->priv->browses,
			      GUINT_TO_POINTER(_plugin->next_browse_id), args);

		return _plugin->next_browse_id++;
	}
}

/**
 * See mafw_source_cancel_browse() for more information
 */
static gboolean mafw_upnp_source_cancel_browse(MafwSource *source,
					       guint browse_id,
					       GError **error)
{
	MafwUPnPSourcePrivate *priv = MAFW_UPNP_SOURCE(source)->priv;
	BrowseArgs* args = NULL;

	g_assert(priv != NULL);
	g_assert(priv->service != NULL);

	if (g_tree_lookup_extended(priv->browses, GUINT_TO_POINTER(browse_id),
				   NULL, (gpointer) &args) == FALSE)
	{
		g_warning("Unable to cancel browse with ID: %u", browse_id);
		g_set_error(error,
			    MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_INVALID_BROWSE_ID,
			    "Browse ID not found");
		return FALSE;
	}
	else
	{
		/* Since g_tree_lookup_extended() returned TRUE, the args
		   struct should not be NULL. */
		g_assert(args != NULL);

		if (args->action != NULL)
		{
			/* Cancel the action related to the given browse ID */
			gupnp_service_proxy_cancel_action(priv->service,
							  args->action);

			/* Unref args, since the UPnP action handler callback
			   won't be called anymore. This will also take care of
			   removing the browse id from the hash table, as well
			   as sending the last EOF msg to the user callback. */
			browse_args_unref(args);
		}
		else
		{
			/* The UPnP action was completed and it cannot be
			   cancelled anymore. */
		}

		return TRUE;
	}
}

/*----------------------------------------------------------------------------
  Metadata
  ----------------------------------------------------------------------------*/

typedef struct _MetadataArgs
{
	/** The particular UPnP server instance that is being browsed */
	MafwUPnPSource* source;

	/** Requested metadata keys */
	gchar** meta_keys;

	/** Metadata browse result as a DIDL-Lite-form XML string */
	gchar* didl;

	/** User callback function & userdata to receive metadata results */
	MafwSourceMetadataResultCb callback;
	gpointer user_data;
} MetadataArgs;

/**
 * mafw_upnp_source_metadata_result:
 * @parser:    The DIDL-Lite parser object that is parsing browse results
 * @didl_node: A single DIDL-Lite object (item/container) node
 * @user_data: #MetadataArgs*
 *
 * Parses each item/container from a successful metadata action, one by one
 * (decided by the #GUPnPDIDLLiteParser). Then, returns the results for the
 * whole set in one go using the user-given callback function. Metadata results
 * practically always contain just one item (also rarely a container).
 */
static void mafw_upnp_source_metadata_result(GUPnPDIDLLiteParser* parser,
					      xmlNode* didl_node,
					      gpointer user_data)
{
	MafwUPnPSourcePrivate* priv = NULL;
	MetadataArgs* args = NULL;

	args = (MetadataArgs*) user_data;
	g_assert(args != NULL);
	g_assert(args->callback != NULL);

	priv = MAFW_UPNP_SOURCE_GET_PRIVATE(args->source);
	g_assert(priv != NULL);

	/* If the XML node is not a DIDL item or container, skip it */
	if (gupnp_didl_lite_object_is_item(didl_node) == TRUE ||
	    gupnp_didl_lite_object_is_container(didl_node) == TRUE)
	{
		GHashTable* metadata;
		gchar* objectid;

		objectid = util_create_objectid(args->source, didl_node);
		metadata = mafw_upnp_source_compile_metadata(args->meta_keys,
							      didl_node,
							      args->didl);

		args->callback(MAFW_SOURCE(args->source), objectid, metadata,
			       args->user_data, NULL);

		g_hash_table_unref(metadata);
		g_free(objectid);
	}
}

/**
 * mafw_upnp_source_metadata_cb:
 * @service:   A CDS Service proxy that completed a browse action
 * @action:    The completed browse action
 * @user_data: #MetadataArgs*
 *
 * Callback that is called when the result from a browse action invocation
 * (with BrowseMetadata specifier) is received. Parses the resulting DIDL-Lite
 * node tree and sends the requested information (if found) back to the
 * requester.
 */
static void mafw_upnp_source_metadata_cb(GUPnPServiceProxy* service,
					 GUPnPServiceProxyAction* action,
					 gpointer user_data)
{
	MetadataArgs* args = (MetadataArgs*) user_data;
	GError* gupnp_error = NULL;

	g_assert(args != NULL);

	gupnp_service_proxy_end_action(service, action, &gupnp_error,
				       "Result", G_TYPE_STRING, &args->didl,
				       NULL);

	g_debug("CDS server with UUID [%s] gave metadata DIDL result: [%s]",
		mafw_extension_get_uuid(MAFW_EXTENSION(args->source)), args->didl);

	if (gupnp_error != NULL)
	{
		GError* error = NULL;

		/* Wrong result or no result at all */
		g_warning("GUPnP error: %s", gupnp_error->message);
		g_set_error(&error,
			    MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_GET_METADATA_RESULT_FAILED,
			    "GUPnP: %s", gupnp_error->message);

		/* Call the callback with invalid values and an error */
		args->callback(MAFW_SOURCE(args->source),
			       NULL, NULL, args->user_data, error);

		g_error_free(error);
		g_error_free(gupnp_error);
	}
	else
	{
		gupnp_didl_lite_parser_parse_didl(
			_plugin->parser,
			args->didl,
			mafw_upnp_source_metadata_result,
			args,
			&gupnp_error);

		if (gupnp_error != NULL)
		{
			GError* error = NULL;

			/* DIDL-Lite parsing failed */
			g_warning("GUPnP error: %s", gupnp_error->message);
			g_set_error(&error,
				    MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_GET_METADATA_RESULT_FAILED,
				    "GUPnP: %s", gupnp_error->message);

			/* Call the callback with invalid values and an error */
			args->callback(MAFW_SOURCE(args->source),
				       NULL, NULL, args->user_data,
				       error);

			g_error_free(error);
			g_error_free(gupnp_error);
		}
	}

	g_strfreev(args->meta_keys);
        g_free(args->didl);
	g_free(args);
}

/**
 * See mafw_source_get_metadata() for more information.
 */
static void mafw_upnp_source_get_metadata(MafwSource *source,
				      const gchar *object_id,
				      const gchar *const *metadata_keys,
				      MafwSourceMetadataResultCb metadata_cb,
				      gpointer user_data)
{
	MafwUPnPSource *self = MAFW_UPNP_SOURCE(source);
	MafwUPnPSourcePrivate *priv = MAFW_UPNP_SOURCE_GET_PRIVATE(self);
	gchar* itemid = NULL;
	MetadataArgs* args = NULL;
	const gchar *const *meta_keys;
	gchar* mdkeys_csv;
	GError *error = NULL;

	g_assert(self != NULL);
	g_assert(priv != NULL);
	g_return_if_fail(object_id != NULL);
	g_return_if_fail(metadata_keys != NULL);
	g_return_if_fail(metadata_cb != NULL);

	if (metadata_keys[0] == NULL)
	{
		/* No metadata keys requested. Call metadata_cb and bail out */
		metadata_cb(source, object_id, NULL, user_data, NULL);
		return;
	}

	/* Get the item ID part from the object ID */
	mafw_source_split_objectid(object_id, NULL, &itemid);
	if (itemid == NULL)
	{
		g_set_error(&error,
			    MAFW_SOURCE_ERROR,
			    MAFW_SOURCE_ERROR_INVALID_OBJECT_ID,
			    "Malformed object ID");
		metadata_cb(source, object_id, NULL, user_data, error);
		g_error_free(error);
		return;
	}

	/* If the ALL_KEYS macro is used, we need to cat all known keys into
	   a big string because otherwise UPnP servers won't do a thing. */
	if (strcmp(MAFW_SOURCE_ALL_KEYS[0], metadata_keys[0]) == 0)
		meta_keys = MAFW_SOURCE_LIST(KNOWN_METADATA_KEYS);
	else
		meta_keys = metadata_keys;

	/* Some parameters we need to pass to the browse metadata return
	 * callback */
	args = g_new0(MetadataArgs, 1);
	args->source = self;
	args->callback = metadata_cb;
	args->user_data = user_data;
	args->meta_keys = util_strvdup(meta_keys);

	/* Convert the given metadata key array into a UPnP browse filter */
	mdkeys_csv = didl_mafwkey_array_to_upnp_filter(meta_keys);

	g_debug("Get metadata: %s\n\tKeys: %s\n", object_id, mdkeys_csv);

	/* Invoke the browse metadata action */
	gupnp_service_proxy_begin_action(
		priv->service, "Browse", mafw_upnp_source_metadata_cb, args,
		"ObjectID",       G_TYPE_STRING, itemid,
		"BrowseFlag",     G_TYPE_STRING, "BrowseMetadata",
		"Filter",         G_TYPE_STRING, mdkeys_csv,
		"StartingIndex",  G_TYPE_UINT,   0,
		"RequestedCount", G_TYPE_UINT,   0,
		"SortCriteria",   G_TYPE_STRING, "",
		NULL);

	g_free(mdkeys_csv);
	g_free(itemid);

	return;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
