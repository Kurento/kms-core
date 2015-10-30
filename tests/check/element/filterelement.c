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
GST_START_TEST (check_invalid_factory)
{
  GstElement *filterelement, *filter;
  gchar *filter_factory;

  filterelement = gst_element_factory_make ("filterelement", NULL);

  /* Default value for filter factory must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter_factory", &filter_factory,
      NULL);
  GST_DEBUG ("Got filter_factory property value : %s", filter_factory);
  fail_unless (filter_factory == NULL);

  /* Default value for filter must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  GST_DEBUG ("Got filter property value : %" GST_PTR_FORMAT, filter);
  fail_unless (filter == NULL);

  /* Set factory */
  filter_factory = "invalid_factory";
  GST_DEBUG ("Setting property uri to : %s", filter_factory);
  g_object_set (G_OBJECT (filterelement), "filter_factory", filter_factory,
      NULL);

  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  fail_unless (filter == NULL);

  /* Reset factory */
  filter_factory = "videoflip";
  GST_DEBUG ("Setting property uri to : %s", filter_factory);
  g_object_set (G_OBJECT (filterelement), "filter_factory", filter_factory,
      NULL);

  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  fail_unless (filter == NULL);

  gst_object_unref (filterelement);
}

GST_END_TEST
GST_START_TEST (check_invalid_pads_factory)
{
  GstElement *filterelement, *filter;
  gchar *filter_factory;

  filterelement = gst_element_factory_make ("filterelement", NULL);

  /* Default value for filter factory must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter_factory", &filter_factory,
      NULL);
  GST_DEBUG ("Got filter_factory property value : %s", filter_factory);
  fail_unless (filter_factory == NULL);

  /* Default value for filter must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  GST_DEBUG ("Got filter property value : %" GST_PTR_FORMAT, filter);
  fail_unless (filter == NULL);

  /* Set factory */
  filter_factory = "decodebin";
  GST_DEBUG ("Setting property uri to : %s", filter_factory);
  g_object_set (G_OBJECT (filterelement), "filter_factory", filter_factory,
      NULL);

  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  fail_unless (filter == NULL);

  /* Reset factory */
  filter_factory = "videoflip";
  GST_DEBUG ("Setting property uri to : %s", filter_factory);
  g_object_set (G_OBJECT (filterelement), "filter_factory", filter_factory,
      NULL);

  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  fail_unless (filter == NULL);

  gst_object_unref (filterelement);
}

GST_END_TEST
GST_START_TEST (check_properties)
{
  GstElement *filterelement, *filter;
  GstElementFactory *factory;
  gchar *filter_factory;

  filterelement = gst_element_factory_make ("filterelement", NULL);

  /* Default value for filter factory must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter_factory", &filter_factory,
      NULL);
  GST_DEBUG ("Got filter_factory property value : %s", filter_factory);
  fail_unless (filter_factory == NULL);

  /* Default value for filter must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  GST_DEBUG ("Got filter property value : %" GST_PTR_FORMAT, filter);
  fail_unless (filter == NULL);

  /* Set factory */
  filter_factory = "videoflip";
  GST_DEBUG ("Setting property uri to : %s", filter_factory);
  g_object_set (G_OBJECT (filterelement), "filter_factory", filter_factory,
      NULL);

  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  fail_unless (filter != NULL);
  factory = gst_element_get_factory (filter);
  fail_unless (factory != NULL);
  GST_DEBUG ("Got factory: %s", GST_OBJECT_NAME (factory));
  fail_unless (g_strcmp0 (filter_factory, GST_OBJECT_NAME (factory)) == 0);
  g_object_unref (filter);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (filterelement),
      GST_DEBUG_GRAPH_SHOW_ALL, "filter");

  /* Reset factory */
  filter_factory = "videocrop";
  GST_DEBUG ("Setting property uri to : %s", filter_factory);
  g_object_set (G_OBJECT (filterelement), "filter_factory", filter_factory,
      NULL);

  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  fail_unless (filter != NULL);
  factory = gst_element_get_factory (filter);
  fail_unless (factory != NULL);
  GST_DEBUG ("Got factory: %s", GST_OBJECT_NAME (factory));
  g_object_unref (filter);
  /* Factory changes are not allowed */
  fail_unless (g_strcmp0 (filter_factory, GST_OBJECT_NAME (factory)));

  gst_object_unref (filterelement);
}

GST_END_TEST;

GST_START_TEST (provide_created_filter)
{
  GstElement *filterelement, *filter, *got_factory;
  gchar *filter_factory = NULL;

  filterelement = gst_element_factory_make ("filterelement", NULL);

  filter = gst_parse_launch ("capsfilter caps=video/x-raw", NULL);

  fail_unless (filter);

  /* Set filter */
  g_object_set (G_OBJECT (filterelement), "filter", filter, NULL);

  /* Check that filter is the same */
  g_object_get (G_OBJECT (filterelement), "filter", &got_factory, NULL);
  fail_unless (got_factory == filter);
  g_object_unref (got_factory);

  /* Get factory */
  g_object_get (filterelement, "filter_factory", &filter_factory, NULL);

  fail_unless (filter_factory);
  fail_if (g_strcmp0 ("capsfilter", filter_factory));
  g_free (filter_factory);

  /* Next set should not have effect */
  g_object_set (filterelement, "filter_factory", "autovideosink", NULL);
  g_object_get (filterelement, "filter_factory", &filter_factory, NULL);

  fail_unless (filter_factory);
  fail_if (g_strcmp0 ("capsfilter", filter_factory));
  g_free (filter_factory);

  g_object_unref (filter);
  gst_object_unref (filterelement);
}

GST_END_TEST;

/* Suite initialization */
static Suite *
filterelement_suite (void)
{
  Suite *s = suite_create ("filterelement");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_properties);
  tcase_add_test (tc_chain, check_invalid_pads_factory);
  tcase_add_test (tc_chain, check_invalid_factory);
  tcase_add_test (tc_chain, provide_created_filter);

  return s;
}

GST_CHECK_MAIN (filterelement);
