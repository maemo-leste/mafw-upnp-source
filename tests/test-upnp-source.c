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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef __ARMEL__
# include <errno.h>
# include <asm/unistd.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <linux/version.h>
#endif

#include <check.h>
#include <checkmore.h>
#include <glib.h>
#include <libmafw/mafw.h>
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>

#include "../upnp-source/mafw-upnp-source.h"
#include "../upnp-source/mafw-upnp-source-didl.h"
#include "../upnp-source/mafw-upnp-source-util.h"

#ifdef __ARMEL__
/* qemu's setsockopt() doesn't support a few flags, which disturbs some
 * libraries.  Let's fake it. */
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 16)
/* Examine the setsockopt() arguments, filter out what can't be emulated
 * then redirect the rest to the real (emulated) setsockopt().  This is
 * not available in newer linux-kernel-headers. */
#  define __NR_real_setsockopt __NR_setsockopt
static _syscall5(int,		real_setsockopt,
		 int,		fd,
		 int,		level,
		 int,		optname,
		 const void *,	optval,
		 socklen_t,	optlen);

int setsockopt(int fd, int level, int optname,
	       const void *optval, socklen_t optlen)
{
	if (level == SOL_SOCKET) {
		if (optlen == sizeof(int) && *(int *)optval) {
			if (optname == SO_BROADCAST)
				return 0;
			else if (optname == SO_REUSEADDR)
				return 0;
		}
	} else if (level == SOL_IP) {
		if (optname == IP_ADD_MEMBERSHIP
		    && optlen == sizeof(struct ip_mreq))
			return 0;
	}

	return real_setsockopt(fd, level, optname, optval, optlen);
}
# else /* LINUX_VERSION_CODE > 2.6.16 */
/* Newer linux-kernel-headers (2.6.22 at least) doesn't define
 * _syscall5() anymore.  Just ignore setsockopt(). */
int setsockopt(int fd, int level, int optname,
	       const void *optval, socklen_t optlen)
{
	return 0;
}
# endif /* LINUX_VERSION_CODE */
#endif /* __ARMEL__ */

START_TEST(test_plugin)
{
	MafwRegistry *reg = mafw_registry_get_instance();
	gboolean retv;
	GError *err = NULL;
	GList *tlist;
	
	err = NULL;
	retv = mafw_registry_load_plugin(reg,
				"mafw-upnp-source", &err);
	fail_unless(retv && !err);
	
	tlist = mafw_registry_list_plugins(reg);
	fail_unless(g_list_length(tlist) == 1);
	g_list_free(tlist);
	tlist = mafw_registry_get_sources(MAFW_REGISTRY(reg)); /* Do not free this list */
	fail_unless(g_list_length(tlist) == 0);
	tlist = mafw_registry_get_renderers(MAFW_REGISTRY(reg)); /* Do not free this list */
	fail_unless(g_list_length(tlist) == 0);
	
	mafw_registry_unload_plugin(reg, "mafw-upnp-source", &err);
	fail_unless(retv && !err);
	
	tlist = mafw_registry_list_plugins(reg);
	fail_unless(g_list_length(tlist) == 0);
	g_list_free(tlist);
	tlist = mafw_registry_get_sources(MAFW_REGISTRY(reg)); /* Do not free this list */
	fail_unless(g_list_length(tlist) == 0);
	tlist = mafw_registry_get_renderers(MAFW_REGISTRY(reg)); /* Do not free this list */
	fail_unless(g_list_length(tlist) == 0);
}
END_TEST

struct expected_results {
	GUPnPServiceProxy *proxy;
	const gchar *action;
	GUPnPServiceProxyActionCallback cb;
	gpointer *args;
	const gchar *const *names;
	guint types[6];
	const gchar *values[6];
	guint skip_count;
	guint item_count;
};

static struct expected_results results;

void verify_results(struct expected_results *expected)
{
	int i;

	fail_if((expected->action != NULL) ^ (results.action != NULL));
	if (!expected->action)
		return;

	fail_if(results.proxy != expected->proxy, "Wrong proxy (%p, %p)",
		results.proxy, expected->proxy);
	fail_if(strcmp(results.action, expected->action), "Wrong action");
	fail_if(results.cb == NULL, "Wrong browse cb(%p, %p)",
		results.cb, expected->cb);
	/* TODO: maybe check args more carefully */
	fail_if(results.args == NULL, "Wrong args");

	for (i=0; i<6; i++) {
		fail_if(strcmp(results.names[i],
			       expected->names[i]), "Wrong name (%s, %s)",
			results.names[i], expected->names[i]);
		fail_if(results.types[i] != expected->types[i]);
		if ( NULL != expected->values[i] )
			fail_if(strcmp(results.values[i],
				       expected->values[i]),
				"Wrong value: `%s' vs. `%s'",
				results.values[i], expected->values[i]);
		g_free((gpointer)results.values[i]);
	}

	fail_if(results.skip_count != expected->skip_count,
		"Wrong skip count");
/*
	fail_if(results.item_count != expected->item_count,
		"Wrong item count");
*/
	g_free((gchar **)results.names);
}

static gboolean need_browse_results;
static gboolean end_action_return_false;
static gboolean with_wrong_didl;
static gint browse_called;

void browse_cb(MafwSource *source, guint browse_id, gint remaining,
	       guint index, const gchar *objectid, GHashTable *metadata,
	       gpointer user_data, const GError *error)
{
	/* Dummy browse result callback. */
	browse_called++;
	if (end_action_return_false || with_wrong_didl)
	{
		fail_if(error == NULL);
		fail_if(metadata != NULL);
		return;
	}
	
	if (need_browse_results)
	{
		fail_if(metadata == NULL);
		fail_if(error != NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_TITLE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_ALBUM_ART_LARGE_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_ALBUM_ART_MEDIUM_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_LYRICS_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_BPP) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_BITRATE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_FILESIZE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_DURATION) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_IS_SEEKABLE) == NULL);
	}
	else
	{
		fail_if(metadata != NULL);
	}
}

static gint mdata_called;

static void mdata_result(MafwSource *self, const gchar *object_id,
			GHashTable *metadata, gpointer user_data,
			const GError *error)
{
	mdata_called++;
	if (end_action_return_false || with_wrong_didl)
	{
		fail_if(error == NULL);
		fail_if(metadata != NULL);
	}
	else
	{
		fail_if(error != NULL);
		fail_if(metadata == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_TITLE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_ALBUM_ART_LARGE_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_ALBUM_ART_MEDIUM_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_ALBUM_ART_SMALL_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_LYRICS_URI) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_BPP) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_BITRATE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_FILESIZE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_DURATION) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_IS_SEEKABLE) == NULL);
		fail_if(mafw_metadata_first(metadata, MAFW_METADATA_KEY_DIDL) == NULL);
	}
}

START_TEST(test_basic_get_metadata)
{
	struct expected_results expected = {
		NULL,
		"Browse",
		(GUPnPServiceProxyActionCallback)0xAAAAAAAA,
		(gpointer)0xBBBBBBBB,
		MAFW_SOURCE_LIST("ObjectID", "BrowseFlag", "Filter",
				 "StartingIndex", "RequestedCount",
				 "SortCriteria" ),
		{ G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING },
		{ "whatever", "BrowseMetadata", NULL, NULL, NULL, "" },
		0,
		0 };
	MafwSource *source = NULL;
	gpointer user_data = (gpointer)0xCCCCCCCC;

	mafw_upnp_source_plugin_initialize(
		MAFW_REGISTRY(mafw_registry_get_instance()));
	source = MAFW_SOURCE(mafw_upnp_source_new("name", "uuid"));

	fail_if(NULL == source, "Could not create source");

	memset((void*)&results, '\0', sizeof (struct expected_results));

	mafw_source_get_metadata(source,
				 "w::whatever", 
				 MAFW_SOURCE_ALL_KEYS,
				 (MafwSourceMetadataResultCb)expected.cb,
				 user_data);

	verify_results(&expected);

	need_browse_results = TRUE;
	mdata_called = 0;
	mafw_source_get_metadata(source,
				 "w::whatever", 
				 MAFW_SOURCE_ALL_KEYS,
				 mdata_result,
				 user_data);
	fail_if(mdata_called != 1);
	need_browse_results = FALSE;
	mafw_upnp_source_plugin_deinitialize();
	
	g_object_unref(source);
}
END_TEST

/*
 * @sexp: a MAFW filter expression
 * @exsc: expected translation of @sexp to upnp terms
 */
static void test_browse(gchar const *action,
		       	const gchar *const *args,
			const gchar *exsc,
			const gchar *sexp)
{
	struct expected_results expected = {
		NULL,
		action,
		(GUPnPServiceProxyActionCallback)0xAAAAAAAA,
		(gpointer)0xBBBBBBBB,
		args,
		{ G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
		       	G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING },
		{ "whatever", exsc, NULL, NULL, NULL, "+interesting ones first" },
		9,
		8 };
	MafwSource *source = NULL;
	gpointer user_data = (gpointer)0xCCCCCCCC;
	MafwFilter *filter = NULL;

	mafw_upnp_source_plugin_initialize(
		MAFW_REGISTRY(mafw_registry_get_instance()));

	source = MAFW_SOURCE(mafw_upnp_source_new("name", "uuid"));

	fail_if(NULL == source, "Could not create source");

	memset((void*)&results, '\0', sizeof (struct expected_results));

	filter = mafw_filter_parse(sexp);
	fail_if(filter == NULL, "Could not parse filter: %s", sexp);
	if (filter != NULL) {
		if (mafw_source_browse(source,
				   "w::whatever", FALSE,
				   filter, expected.values[5],
				   MAFW_SOURCE_NO_KEYS,
				   expected.skip_count, expected.item_count,
				   browse_cb, user_data) !=
					MAFW_SOURCE_INVALID_BROWSE_ID)
		{
			verify_results(&expected);
		}
		else
			fail_if(exsc != NULL);

		mafw_filter_free(filter);
	}

	mafw_upnp_source_plugin_deinitialize();
	
	g_object_unref(source);
}

START_TEST(test_basic_browse_null_metadata)
{
	gchar const *action = "Browse";
	const gchar *const *args =
		MAFW_SOURCE_LIST("ObjectID", "BrowseFlag", "Filter",
				 "StartingIndex", "RequestedCount",
				 "SortCriteria");
	const gchar *exsc = NULL;
	struct expected_results expected = {
		NULL,
		action,
		(GUPnPServiceProxyActionCallback)0xAAAAAAAA,
		(gpointer)0xBBBBBBBB,
		args,
		{ G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
		       	G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING },
		{ "whatever", exsc, NULL, NULL, NULL, "interesting ones first" },
		9,
		8 };
	MafwSource *source = NULL;
	gpointer user_data = (gpointer)0xCCCCCCCC;
	guint browse_id = 0;

	mafw_upnp_source_plugin_initialize(
		MAFW_REGISTRY(mafw_registry_get_instance()));

	source = MAFW_SOURCE(mafw_upnp_source_new("name", "uuid"));

	fail_if(NULL == source, "Could not create source");

	memset((void*)&results, '\0', sizeof (struct expected_results));

	browse_called = 0;
	browse_id =
		mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, expected.values[5], NULL,
				   expected.skip_count, expected.item_count,
				   browse_cb, user_data);
	fail_if(browse_called != 1);
	fail_if(browse_id != MAFW_SOURCE_INVALID_BROWSE_ID);
	browse_called = 0;

	mafw_upnp_source_plugin_deinitialize();
	g_object_unref(source);
}
END_TEST

struct _MafwUPnPSourcePrivate {
	/* The UPnP device providing a CDS service */
	GUPnPDeviceProxy* device;

	/* The CDS (ContentDirectoryService) provided by this device */
	GUPnPServiceProxy* service;

	/* browse_id => GUPnPServiceProxyAction associations for ->cancel(). */
	GTree *browses;
};

static gboolean return_null_action;
START_TEST(test_errors)
{
	MafwSource *source = NULL;
	guint browse_id = 0;

	mafw_upnp_source_plugin_initialize(
		MAFW_REGISTRY(mafw_registry_get_instance()));

	source = MAFW_SOURCE(mafw_upnp_source_new("name", "uuid"));

	fail_if(NULL == source, "Could not create source");

	need_browse_results = TRUE;
	end_action_return_false = TRUE;
	browse_id = mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   0, 0,
				   browse_cb, NULL);
	fail_if(browse_id == MAFW_SOURCE_INVALID_BROWSE_ID);
	end_action_return_false = FALSE;

	with_wrong_didl = TRUE;
	browse_id = mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   0, 0,
				   browse_cb, NULL);
	fail_if(browse_id == MAFW_SOURCE_INVALID_BROWSE_ID);
	with_wrong_didl = FALSE;

	need_browse_results = FALSE;

	return_null_action = TRUE;
	browse_id = mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   0, 0,
				   browse_cb, NULL);
	fail_if(browse_id != MAFW_SOURCE_INVALID_BROWSE_ID);
	return_null_action = FALSE;

	need_browse_results = TRUE;
	end_action_return_false = TRUE;
	mafw_source_get_metadata(source,
				 "w::whatever", 
				 MAFW_SOURCE_ALL_KEYS,
				 mdata_result,
				 NULL);
	end_action_return_false = FALSE;

	with_wrong_didl = TRUE;
	mafw_source_get_metadata(source,
				 "w::whatever", 
				 MAFW_SOURCE_ALL_KEYS,
				 mdata_result,
				 NULL);
	with_wrong_didl = FALSE;

	need_browse_results = FALSE;

	mafw_upnp_source_plugin_deinitialize();
	g_object_unref(source);
}
END_TEST

START_TEST(test_basic_browse)
{
	MafwSource *source = NULL;
	guint browse_id = 0;

	mafw_upnp_source_plugin_initialize(
		MAFW_REGISTRY(mafw_registry_get_instance()));

	source = MAFW_SOURCE(mafw_upnp_source_new("name", "uuid"));

	fail_if(NULL == source, "Could not create source");

	need_browse_results = TRUE;
	browse_called = 0;
	browse_id =
		mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   1, 1,
				   browse_cb, NULL);
	fail_if(browse_id == MAFW_SOURCE_INVALID_BROWSE_ID);
	fail_if(browse_called != 1, "Called: %d", browse_called);

	browse_called = 0;
	browse_id =
		mafw_source_browse(source,
				   "w::", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   1, 4,
				   browse_cb, NULL);
	fail_if(browse_id == MAFW_SOURCE_INVALID_BROWSE_ID);
	fail_if(browse_called != 3, "Called: %d", browse_called);

	browse_called = 0;
	browse_id =
		mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   0, 0,
				   browse_cb, NULL);
	fail_if(browse_id == MAFW_SOURCE_INVALID_BROWSE_ID);
	fail_if(browse_called != 3, "Called: %d", browse_called);
	need_browse_results = FALSE;

	/* Test cancel */
	browse_id =
		mafw_source_browse(source,
				   "w::whatever", FALSE,
				   NULL, NULL, MAFW_SOURCE_ALL_KEYS,
				   0, 0,
				   browse_cb, NULL);
	MAFW_UPNP_SOURCE(source)->priv->service = (gpointer)mafw_upnp_source_new("name2", "uuid2");
	fail_unless(mafw_source_cancel_browse(source, browse_id, NULL));
	fail_if(mafw_source_cancel_browse(source, browse_id, NULL));

	mafw_upnp_source_plugin_deinitialize();
	g_object_unref(source);
}
END_TEST


START_TEST(test_browse_with_filter)
{
	const gchar *const fields[] = {
	       	"ContainerID", "SearchCriteria", "Filter",
	       	"StartingIndex", "RequestedCount", "SortCriteria",
	};

	/* Simple terms */
	test_browse("Search", fields,
		    "everwhat = \"hihi\"", "(everwhat=hihi)");
	test_browse("Search", fields,
		    "everwhat != \"hihi\"", "(!(everwhat=hihi))");
	test_browse("Search", fields,
		    "everwhat < \"hihi\"", "(everwhat<hihi)");
	test_browse("Search", fields,
		    "everwhat >= \"hihi\"", "(!(everwhat<hihi))");
	test_browse("Search", fields,
		    "everwhat > \"hihi\"", "(everwhat>hihi)");
	test_browse("Search", fields,
		    "everwhat <= \"hihi\"", "(!(everwhat>hihi))");
	test_browse("Search", fields,
		    "everwhat exists true", "(everwhat?)");
	test_browse("Search", fields,
		    "everwhat exists false", "(!(everwhat?))");
	test_browse("Search", fields,
		    NULL, "(everwhat~hih*i)");

	/* Complex expressions */
	test_browse("Search", fields,
		    "(a < \"b\") and (c > \"d\")", "(&(a<b)(c>d))");
	test_browse("Search", fields,
		    "(a >= \"b\") or (c <= \"d\")", "(!(&(a<b)(c>d)))");
/* TODO: check what really is wrong here */
	test_browse("Search", fields,
		    "((a < \"b\") and (c exists true)) and "
		    	"((a doesNotContain \"b\") and (c = \"d\"))",
		    "(!(|(!(&(a<b)(c?)))(|(a~b)(!(c=d)))))");
	/* Escaping */
/* 	test_browse("Search", fields, */
/* 		    "a contains \"\\\\\"", "(a~\\5C)"); */
	test_browse("Search", fields,
		    "a = \"\\\"he,ha\\\"\"", "(a=\"he,ha\")");

	/* Search terms that get mapped to upnp domain */
	test_browse("Search", fields,
		    "res@protocolInfo contains \"hihi\"", "(mime-type=hihi)");
	test_browse("Search", fields,
		    "dc:title = \"hihi\"", "(title=hihi)");
	test_browse("Search", fields,
		    "upnp:album = \"hihi\"", "(album=hihi)");
	test_browse("Search", fields,
		    "upnp:genre = \"hihi\"", "(genre=hihi)");
	test_browse("Search", fields,
		    "upnp:artist = \"hihi\"", "(artist=hihi)");
}
END_TEST


/****************************************************************************
 * Container changed signal
 ****************************************************************************/

static MafwUPnPSource* ccsource;
static gboolean cc1 = FALSE;
static gboolean ccf = FALSE;
static gboolean cc2 = FALSE;
static gboolean cc3 = FALSE;

/* Prototype for a non-public API function */
void mafw_upnp_source_notify_callback(GUPnPServiceProxy* service,
				       const gchar* variable,
				       GValue* value,
				       gpointer user_data);

static void container_changed_cb(MafwUPnPSource* source, const gchar* oid)
{
	fail_unless(source == ccsource, "Wrong signaling source pointer");

	if (strcmp(oid, "uuid::1") == 0)
		cc1 = TRUE;
	else if (strcmp(oid, "uuid::foo") == 0)
		ccf = TRUE;
	else if (strcmp(oid, "uuid::2") == 0)
		cc2 = TRUE;
	else if (strcmp(oid, "uuid::3") == 0)
		cc3 = TRUE;
	else
		fail("Unjustified container changed signal for [%s]", oid);
}

START_TEST(test_container_changed)
{
	GValue value = { 0 };

	ccsource = MAFW_UPNP_SOURCE(mafw_upnp_source_new("name", "uuid"));
	g_signal_connect(ccsource, "container-changed",
			 G_CALLBACK(container_changed_cb), NULL);

	g_value_init(&value, G_TYPE_STRING);
	g_value_set_string(&value, "1,foo,2,3");

	mafw_upnp_source_notify_callback((GUPnPServiceProxy*) 0xEFFAFFAA,
					  "ContainerUpdateIDs", &value,
					  ccsource);

	g_object_unref(ccsource);

	fail_unless(cc1 == TRUE && ccf == TRUE && cc2 == TRUE && cc3 == TRUE,
		    "One or more container changed signals never came thru");
}
END_TEST

/****************************************************************************
 * Test suite creation & execution
 ****************************************************************************/

int main(void)
{
	TCase *tc;
	Suite *suite;

	checkmore_wants_dbus();
	g_type_init();
	g_thread_init(NULL);

	checkmore_wants_dbus();
	suite = suite_create("MafwUPnPSource");

	tc = tcase_create("Init");
	suite_add_tcase(suite, tc);
if (1)	tcase_add_test(tc, test_plugin);
	
	/* Browse tests */
	tc = tcase_create("Browse");
	suite_add_tcase(suite, tc);
if(1)	tcase_add_test(tc, test_browse_with_filter);
if(1)	tcase_add_test(tc, test_basic_browse_null_metadata);
if(1)	tcase_add_test(tc, test_basic_browse);

	/* Metadata tests */
	tc = tcase_create("Get metadata");
	suite_add_tcase(suite, tc);
if(1)	tcase_add_test(tc, test_basic_get_metadata);

	/* Other tests */
	tc = tcase_create("Other");
	suite_add_tcase(suite, tc);
if(1)	tcase_add_test(tc, test_container_changed);

if(1)	tcase_add_test(tc, test_errors);

        return checkmore_run(srunner_create(suite), FALSE);
}


/* GUPnP fake functions */

GUPnPServiceProxyAction *gupnp_service_proxy_begin_action(
                                GUPnPServiceProxy *proxy,
                                const char *action,
                                GUPnPServiceProxyActionCallback callback,
                                gpointer user_data,
                                ...)
{
	va_list list;
	gchar *next;
	gint i;
	GPtrArray *names;
	
	if (return_null_action)
		return NULL;

	if (need_browse_results)
	{
		callback(proxy, (GUPnPServiceProxyAction*) 0x2345, user_data);
		return (GUPnPServiceProxyAction *) 0x1234; 
	}
        results.proxy = proxy;
        results.action = action;
        results.cb = callback;
        results.args = user_data;

	va_start(list, user_data);
	next = (gchar *)va_arg(list, gchar*);

	names  = g_ptr_array_new();
	for (i=0; next;i++) {
		g_ptr_array_add(names, next);
		results.types[i] = (gint)va_arg(list, gint);
		switch (i) {
			case 3:
				results.skip_count = (guint)va_arg(list, guint);
				results.values[i] = g_strdup("dummy");
				break;
			case 4:
				results.item_count = (guint)va_arg(list, guint);
				results.values[i] = g_strdup("dummy");
				break;
			default:
				results.values[i] = g_strdup(va_arg(list, gchar*));
				break;

		}
		next = (gchar *)va_arg(list, gchar*);
	}

	results.names  = (const gchar *const *)g_ptr_array_free(names,  FALSE);

	va_end(list);

        return (GUPnPServiceProxyAction *) 0x1234;
}

static gchar* DIDL_ITEM = \
	"<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">" \
	 "<item id=\"18132\" refID=\"18073\" parentID=\"18131\" restricted=\"1\">" \
	  "<dc:title>Test Animals</dc:title>" \
	  "<upnp:albumArtURI>http://foo.bar.com:31337/albumArt.png</upnp:albumArtURI>" \
	  "<upnp:lyricsURI>http://foo.bar.com:31337/lyrics.txt</upnp:lyricsURI>" \
	  "<upnp:artistDiscographyURI>http://foo.bar.com:31337/disco.html</upnp:artistDiscographyURI>" \
	  "<res colorDepth=\"32\" bitrate=\"31337\" size=\"6548309\" duration=\"0:04:32.770\" protocolInfo=\"http-get:*:audio/mpeg:DLNA.ORG_OP=11\" >http://172.23.117.242:9000/disk/music/O18132.mp3</res>" \
	  "<upnp:class>object.item.audioItem.musicTrack</upnp:class>" \
	  "<foo>bar</foo>" \
	 "</item>" \
	"</DIDL-Lite>";
static gchar* FAKE_DIDL_ITEM = \
	"<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">" \
	 "And here comes the problem.....";

gboolean gupnp_service_proxy_end_action(GUPnPServiceProxy *proxy,
					GUPnPServiceProxyAction *action,
					GError **error, ...)
{
	va_list list;
	gchar *next;
	gpointer *data;

	if (end_action_return_false)
	{
		g_set_error(error, MAFW_SOURCE_ERROR,
				    MAFW_SOURCE_ERROR_BROWSE_RESULT_FAILED,
				    "GUPnP: testerr"); 
		return FALSE;
	}

	va_start(list, error);
	(gchar *)va_arg(list, gchar*);
	(gint)va_arg(list, gint);
	data = va_arg(list, gpointer*);
	if (with_wrong_didl)
	{
		*data = g_strdup(FAKE_DIDL_ITEM);
	}
	else
	{
		*data = g_strdup(DIDL_ITEM);
	}
	
	if ((gchar *)va_arg(list, gchar*))
	{
		(gint)va_arg(list, gint);
		data = va_arg(list, gpointer*);
		*data = (gpointer)3;
		
		(gchar *)va_arg(list, gchar*);
		(gint)va_arg(list, gint);
		data = va_arg(list, gpointer*);
		*data = (gpointer)3;
	}

	return TRUE;
}

void gupnp_service_proxy_cancel_action (GUPnPServiceProxy *proxy,
					GUPnPServiceProxyAction *action)
{
	return;
}

/* vi: set noexpandtab ts=8 sw=8 cino=t0,(0: */
