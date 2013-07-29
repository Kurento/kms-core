#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

GST_START_TEST (check_properties)
{
  GstElement *recorder;
  gchar *uri, *test;

  recorder = gst_element_factory_make ("recorderendpoint", NULL);

  /* Default value for uri must be NULL */
  g_object_get (G_OBJECT (recorder), "uri", &uri, NULL);
  GST_DEBUG ("Got uri property value : %s", uri);
  fail_unless (uri == NULL);

  /* Set property */
  test = "test_1";
  GST_DEBUG ("Setting property uri to : %s", test);
  g_object_set (G_OBJECT (recorder), "uri", test, NULL);
  g_object_get (G_OBJECT (recorder), "uri", &uri, NULL);
  GST_DEBUG ("Got uri property value : %s", uri);
  fail_unless (g_strcmp0 (uri, test) == 0);
  g_free (uri);

  /* Re-set property */
  test = "test_2";
  GST_DEBUG ("Setting property uri to : %s", test);
  g_object_set (G_OBJECT (recorder), "uri", test, NULL);
  g_object_get (G_OBJECT (recorder), "uri", &uri, NULL);
  GST_DEBUG ("Got uri property value : %s", uri);
  fail_unless (g_strcmp0 (uri, test) == 0);
  g_free (uri);
}

GST_END_TEST static Suite *
recorderendpoint_suite (void)
{
  Suite *s = suite_create ("recorderendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_properties);

  return s;
}

GST_CHECK_MAIN (recorderendpoint);
