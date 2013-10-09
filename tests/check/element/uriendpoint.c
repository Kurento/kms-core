/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */
#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

#include "kmsuriendpointstate.h"

GST_START_TEST (check_uri_prop)
{
  GstElement *urielement;
  gchar *uri, *test;

  urielement = gst_element_factory_make ("uriendpoint", NULL);

  /* Default value for uri must be NULL */
  g_object_get (G_OBJECT (urielement), "uri", &uri, NULL);
  GST_DEBUG ("Got uri property value : %s", uri);
  fail_unless (uri == NULL);

  /* Set property */
  test = "test_1";
  GST_DEBUG ("Setting property uri to : %s", test);
  g_object_set (G_OBJECT (urielement), "uri", test, NULL);
  g_object_get (G_OBJECT (urielement), "uri", &uri, NULL);
  GST_DEBUG ("Got uri property value : %s", uri);
  fail_unless (g_strcmp0 (uri, test) == 0);
  g_free (uri);

  /* Re-set property */
  test = "test_2";
  GST_DEBUG ("Setting property uri to : %s", test);
  g_object_set (G_OBJECT (urielement), "uri", test, NULL);
  g_object_get (G_OBJECT (urielement), "uri", &uri, NULL);
  GST_DEBUG ("Got uri property value : %s", uri);
  fail_unless (g_strcmp0 (uri, test) == 0);
  g_free (uri);

  gst_object_unref (urielement);
}

GST_END_TEST static Suite *
uriendpoint_suite (void)
{
  Suite *s = suite_create ("uriendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_uri_prop);

  return s;
}

GST_CHECK_MAIN (uriendpoint);
