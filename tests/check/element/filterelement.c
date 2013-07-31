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
  GST_DEBUG ("Got uri property value : %s", filter_factory);
  fail_unless (filter_factory == NULL);

  /* Default value for filter must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  GST_DEBUG ("Got uri property value : %s", filter);
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
  GST_DEBUG ("Got uri property value : %s", filter_factory);
  fail_unless (filter_factory == NULL);

  /* Default value for filter must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  GST_DEBUG ("Got uri property value : %s", filter);
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
  GST_DEBUG ("Got uri property value : %s", filter_factory);
  fail_unless (filter_factory == NULL);

  /* Default value for filter must be NULL */
  g_object_get (G_OBJECT (filterelement), "filter", &filter, NULL);
  GST_DEBUG ("Got uri property value : %s", filter);
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
  /* Factory changes are not allowed */
  fail_unless (g_strcmp0 (filter_factory, GST_OBJECT_NAME (factory)));

  gst_object_unref (filterelement);
}

GST_END_TEST
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

  return s;
}

GST_CHECK_MAIN (filterelement);
