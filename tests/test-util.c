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

#include <glib.h>
#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libmafw/mafw.h>
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>

#include "../upnp-source/mafw-upnp-source.h"
#include "../upnp-source/mafw-upnp-source-util.h"

START_TEST(test_util_udn_to_uuid)
{
	gchar* str;

	str = util_udn_to_uuid("uuid:6afad861-2430-41fa-a179-faf475078494");
	fail_unless(strcmp(str,
		"_uuid_3A6afad861_2D2430_2D41fa_2Da179_2Dfaf475078494") == 0,
		"UDN to UUID conversion failure");
	g_free(str);
}
END_TEST

START_TEST(test_util_uuid_to_udn)
{
	gchar* str;

	str = util_uuid_to_udn(
			"_uuid_3A6afad861_2D2430_2D41fa_2Da179_2Dfaf475078494");
	fail_unless(strcmp(str,
			"uuid:6afad861-2430-41fa-a179-faf475078494") == 0,
			"UUID to UDN conversion failure");
	g_free(str);
}
END_TEST

START_TEST(test_util_compare_uint)
{
	fail_unless(util_compare_uint(5, 10000000) < 0);
	fail_unless(util_compare_uint(100, 99) > 0);
	fail_unless(util_compare_uint(1, 1) == 0);
}
END_TEST

START_TEST(test_util_create_objectid)
{
	MafwUPnPSource* source;
	xmlDocPtr doc;
	gchar* oid;
	const gchar* item =
		"<item id=\"18132\" refID=\"18073\" parentID=\"18131\"></item>";

	doc = xmlReadMemory(item, strlen(item), "none.xml", NULL, 0);
	g_assert(doc != NULL);

	source = MAFW_UPNP_SOURCE(mafw_upnp_source_new("name", "uuid"));
	g_assert(source != NULL);

	oid = util_create_objectid(source, xmlDocGetRootElement(doc));
	fail_unless(strcmp(oid, "uuid::18132") == 0, "Wrong object ID");
	g_free(oid);

	xmlFreeDoc(doc);
	g_object_unref(source);
}
END_TEST

int main(void)
{
	Suite *suite;
	SRunner* sr;
	TCase *tc;
	gint nf;

        g_type_init();

        suite = suite_create("MAFW UPnP Source Utilities");

	tc = tcase_create("Utilities");
	suite_add_tcase(suite, tc);
	tcase_add_test(tc, test_util_udn_to_uuid);
	tcase_add_test(tc, test_util_uuid_to_udn);
	tcase_add_test(tc, test_util_compare_uint);
	tcase_add_test(tc, test_util_create_objectid);

	sr = srunner_create(suite);
	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
