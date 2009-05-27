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

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

#include <libmafw-shared/mafw-shared.h>
#include <libmafw/mafw.h>

#include "upnp-source/mafw-upnp-source.h"

/*****************************************************************************
 * Performance measurement
 *****************************************************************************/
static struct timeval start_time;
static struct timeval first_time;
static struct timeval end_time;
static guint num = 0;

/*****************************************************************************
 * Browse cancellation
 *****************************************************************************/
static gboolean _cancel_browse = FALSE;
static guint _cancel_timeout = 100;

/*****************************************************************************
 * Browse & metadata
 *****************************************************************************/
static gboolean _get_metadata = FALSE;
static gchar* _sort_criteria = NULL;
static gchar* _filter = NULL;
static gchar* _objectid = NULL;
static guint _browse_timeout = 5000;
static guint _skip_count = 0;
static guint _item_count = 0;

/*****************************************************************************
 * System
 *****************************************************************************/
static gboolean _dbus = FALSE;
static GMainLoop* _main_loop = NULL;

static gboolean quit_cb(gpointer data)
{
	g_print("\nItems: %d, First result in %lu.%lus, All: %lu.%lus\n",
		num,
		first_time.tv_sec - start_time.tv_sec,
		first_time.tv_usec - start_time.tv_usec,
		end_time.tv_sec - start_time.tv_sec,
		end_time.tv_usec - start_time.tv_usec);
	g_main_loop_quit(_main_loop);
	return FALSE;
}

typedef struct _CancelData
{
	guint browseid;
	MafwSource* source;
} CancelData;

static gboolean cancel_cb(gpointer data)
{
	CancelData* cd = (CancelData*) data;
	GError* error = NULL;

	g_assert(cd != NULL);
	g_assert(cd->source != NULL);

	mafw_source_cancel_browse(cd->source, cd->browseid, &error);
	if (error != NULL)
	{
		g_print("\n\n\nUnable to cancel: %s\n\n\n", error->message);
		g_error_free(error);
	}

	g_object_unref(cd->source);
	g_free(cd);

	return FALSE;
}

static void print_items_cb(const gchar *key, gpointer val)
{
	g_print("\t%s:", key);
	if (mafw_metadata_nvalues(val) > 1)
	{
		guint i;
		GValueArray *values = val;

		for (i = 0; i < values->n_values; i++)
		{
			GValue* value = g_value_array_get_nth(values, i);
			if (G_VALUE_HOLDS(value, G_TYPE_STRING))
				g_print(" %s", g_value_get_string(value));
			else if (G_VALUE_HOLDS(value, G_TYPE_INT))
				g_print(" %d", g_value_get_int(value));
		}
	}
	else
	{
		if (G_VALUE_HOLDS(val, G_TYPE_STRING))
			g_print(" %s (string)\n", g_value_get_string(val));
		else if (G_VALUE_HOLDS(val, G_TYPE_INT))
			g_print(" %d (int)\n", g_value_get_int(val));
	}
}

static void print_items(const gchar *object_id, GHashTable *metadata)
{
	g_print("ObjectID: [%s]\n", object_id);
	g_hash_table_foreach(metadata, (GHFunc)print_items_cb, NULL);
}

static void browse_result_cb(MafwSource* source, guint browse_id,
			     gint remaining_count, guint index,
			     const gchar* object_id, GHashTable* metadata,
			     gpointer user_data, const GError *error)
{
	g_print(">> Browse result\n");

	if (error != NULL)
	{
		g_print("\nError: %s\n", error->message);
	}
	else if (metadata != NULL)
	{
		g_print("\nBrowse ID:\t%d\n", browse_id);
		g_print("Remaining:\t%d\n", remaining_count);
		g_print("Current:\t%d\n", index);
		print_items(object_id, metadata);
	}

	g_print("<< Browse result\n");

	if (num == 0)
		gettimeofday(&first_time, NULL);

	num++;
	if (remaining_count == 0)
	{
		g_print("Termination\n");

		gettimeofday(&end_time, NULL);

		/* Quit after a few mseconds */
		g_timeout_add(100, quit_cb, _main_loop);
	}
}

static void metadata_result_cb(MafwSource* source, const gchar* object_id,
			       GHashTable *metadata,
			       gpointer user_data, const GError *error)
{
	g_print(">> Metadata result\n");
	if (error != NULL)
		g_print("Error: %s\n", error->message);
	else
		print_items(object_id, metadata);
	g_print("<< Metadata result\n");
}

static gboolean browse_cb(gpointer data)
{
	const gchar* const* meta_keys = NULL;
	MafwRegistry* registry = NULL;
	MafwSource* source = NULL;
	guint browseid;
        gchar *childcount;

	registry = MAFW_REGISTRY(mafw_registry_get_instance());
	g_assert(registry != NULL);

	/* Make a list of interesting metadata keys */
        childcount = MAFW_METADATA_KEY_CHILDCOUNT(1);
	meta_keys = MAFW_SOURCE_LIST(MAFW_METADATA_KEY_TITLE,
				     MAFW_METADATA_KEY_URI,
				     childcount,
				     MAFW_METADATA_KEY_MIME,
				     MAFW_METADATA_KEY_DURATION,
				     MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI,
				     MAFW_METADATA_KEY_PROTOCOL_INFO,
				     MAFW_METADATA_KEY_FILESIZE,
				     MAFW_METADATA_KEY_BPP);

	gettimeofday(&start_time, NULL);

	if (_objectid == NULL)
	{
		gchar* objectid = NULL;
		GList* sources = NULL;
		GList* node = NULL;

		/* Browse all available sources' root containers */
		sources = mafw_registry_get_sources(registry);
		for (node = sources; node != NULL; node = node->next)
		{
			const gchar* uuid;
			const gchar* name;

			source = MAFW_SOURCE(node->data);

			/* Generate root object id */
			uuid = mafw_extension_get_uuid(MAFW_EXTENSION(source));
			name = mafw_extension_get_name(MAFW_EXTENSION(source));
			objectid = g_strdup_printf("%s::0", uuid);

			g_print("Browse %s root: [%s]\n", name, objectid);
			
			/* Browse each source's root container */
			browseid = mafw_source_browse(
				source,           /* Source */
				objectid,         /* Object ID */
				FALSE,            /* Recursive */
				_filter,          /* Filter */
				_sort_criteria,   /* Sort criteria */
				meta_keys,        /* Metadata keys */
				_skip_count,      /* Skip count */
				_item_count,      /* Item count */
				browse_result_cb, /* Callback */
				NULL);            /* User data */

			if (_cancel_browse == TRUE)
			{
				CancelData* cd = g_new0(CancelData, 1);
				cd->source = g_object_ref(source);
				cd->browseid = browseid;
				g_timeout_add(_cancel_timeout, cancel_cb, cd);
			}

			/* Fetch metadata for root container */
			mafw_source_get_metadata(source,
						  objectid,
						  meta_keys,
						  metadata_result_cb,
						  NULL);

			g_free(objectid);
		}
	}
	else
	{
		gchar* uuid = NULL;
		gchar* itemid = NULL;

		g_print("Browse: [%s]\n", _objectid);

		mafw_source_split_objectid(_objectid, &uuid, &itemid);
		
		source = MAFW_SOURCE(mafw_registry_get_extension_by_uuid(
						registry, uuid));
		g_assert(source != NULL);

		/* Browse the given container */
		browseid = mafw_source_browse(
			source,           /* Source */
			_objectid,         /* Object ID */
			FALSE,            /* Recursive */
			_filter,          /* Filter */
			_sort_criteria,   /* Sort criteria */
			meta_keys,        /* Metadata keys */
			_skip_count,      /* Skip count */
			_item_count,      /* Item count */
			browse_result_cb, /* Callback */
			NULL);            /* User data */

		if (_cancel_browse == TRUE)
		{
			CancelData* cd = g_new0(CancelData, 1);
			cd->source = g_object_ref(source);
			cd->browseid = browseid;
			g_timeout_add(_cancel_timeout, cancel_cb, cd);
		}

		/* Fetch metadata for the given container */
		if (_get_metadata == TRUE)
		{
			mafw_source_get_metadata(source,
						  _objectid,
						  meta_keys,
						  metadata_result_cb,
						  NULL);
		}
		
		g_free(uuid);
		g_free(itemid);
	}

        g_free(childcount);

	return FALSE;
}

void print_usage()
{
	g_print("--------------------------------------------------------------"
		"------------------\n");
	g_print("Without options, this tool browses the root containers "
		"of all available sources.\n\n");
	g_print("  -c <n>\tCancel browse after <n> milliseconds\n");
	g_print("  -d\t\tInitialize DBus for out-of-process extensions\n");
	g_print("  -f <filter>\tSearch results matching given criteria\n");
	g_print("  -k <n>\tSkip <n> amount of results from the start\n");
	g_print("  -l <n>\tLimit the number of items returned to <n>\n");
	g_print("  -m\t\tBrowse the given object ID's metadata as well\n");
	g_print("  -o <object>\tBrowse under the given <object>\n");
	g_print("  -s <sort>\tSort results according to criteria\n");
	g_print("  -t <n>\tWait <n> ms before browsing (default 5000)\n");
	g_print("--------------------------------------------------------------"
		"------------------\n");
}

void parse_args(int argc, char** argv, MafwRegistry* registry)
{
	int i;
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-c") == 0)
		{
			_cancel_browse = TRUE;
			if ((i+1) <= argc)
				_cancel_timeout = atoi(argv[i+1]);
			else
				_cancel_timeout = 100;
		}
		else if (strcmp(argv[i], "-d") == 0)
		{
			GError* error = NULL;

			g_assert(registry != NULL);

			mafw_shared_init(registry, &error);
			if (error != NULL)
			{
				g_warning("Extensions in session bus won't be "\
					  "available: %s", error->message);
				g_error_free(error);
				error = NULL;

				_dbus = FALSE;				
			}
			else
			{
				_dbus = TRUE;
			}
		}
		else if (strcmp(argv[i], "-f") == 0)
		{
			if ((i+1) <= argc)
				_filter = argv[i+1];
			else
				_filter = NULL;
		}
		else if (strcmp(argv[i], "-k") == 0)
		{
			if ((i+1) <= argc)
				_skip_count = atoi(argv[i+1]);
			else
				_skip_count = 0;
		}
		else if (strcmp(argv[i], "-l") == 0)
		{
			if ((i+1) <= argc)
				_item_count = atoi(argv[i+1]);
			else
				_item_count = 0;
		}
		else if (strcmp(argv[i], "-m") == 0)
		{
			_get_metadata = TRUE;
		}
		else if (strcmp(argv[i], "-o") == 0 && (i+1) <= argc)
		{
			_objectid = g_strdup(argv[i+1]);
		}
		else if (strcmp(argv[i], "-s") == 0)
		{
			if ((i+1) <= argc)
				_sort_criteria = argv[i+1];
			else
				_sort_criteria = NULL;
		}
		else if (strcmp(argv[i], "-t") == 0)
		{
			if ((i+1) < argc)
				_browse_timeout = atoi(argv[i+1]);
			else
				_browse_timeout = 5000;
		}
	}
}

int main(int argc, char **argv)
{
	MafwRegistry* registry;

	g_type_init();
	g_thread_init(NULL);

	_main_loop = g_main_loop_new(NULL, FALSE);
	g_assert(_main_loop != NULL);

	registry = MAFW_REGISTRY(mafw_registry_get_instance());
	g_assert(registry != NULL);

	if (argc == 1)
		print_usage();

	parse_args(argc, argv, registry);

	/* If DBus has not been initialized, the user wants to use only the one
	   in-process upnp plugin. Load it. */
	if (_dbus == FALSE)
	{
		GError* error = NULL;
		mafw_registry_load_plugin(registry, "mafw-upnp-source",
					   &error);
		if (error != NULL)
		{
			g_critical("Unable to load mafw-upnp-source: %s",
				   error->message);
			g_error_free(error);
		}
	}

	/* Wait a few seconds for sources and then do what is wanted */
	g_timeout_add(_browse_timeout, browse_cb, NULL);
	
	g_main_loop_run(_main_loop);
	g_main_loop_unref(_main_loop);
	g_free(_objectid);

	mafw_upnp_source_plugin_deinitialize();

	return 0;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
