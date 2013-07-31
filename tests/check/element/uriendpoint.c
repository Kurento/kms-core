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

GST_END_TEST
GST_START_TEST (check_state_prop)
{
  GstElement *urielement;
  KmsUriEndPointState state;

  urielement = gst_element_factory_make ("uriendpoint", NULL);

  /* Default value for uri must be KMS_URI_END_POINT_STATE_STOP */
  g_object_get (G_OBJECT (urielement), "state", &state, NULL);
  GST_DEBUG ("Got state property value : %d", state);
  fail_unless (state == KMS_URI_END_POINT_STATE_STOP);

  /* Set value to KMS_URI_END_POINT_STATE_START */
  GST_DEBUG ("Setting property state to : %d", KMS_URI_END_POINT_STATE_START);
  g_object_set (G_OBJECT (urielement), "state", KMS_URI_END_POINT_STATE_START,
      NULL);
  g_object_get (G_OBJECT (urielement), "state", &state, NULL);
  GST_DEBUG ("Got state property value : %d", state);
  fail_unless (state == KMS_URI_END_POINT_STATE_START);

  /* Set value to KMS_URI_END_POINT_STATE_PAUSE */
  GST_DEBUG ("Setting property state to : %d", KMS_URI_END_POINT_STATE_PAUSE);
  g_object_set (G_OBJECT (urielement), "state", KMS_URI_END_POINT_STATE_PAUSE,
      NULL);
  g_object_get (G_OBJECT (urielement), "state", &state, NULL);
  GST_DEBUG ("Got state property value : %d", state);
  fail_unless (state == KMS_URI_END_POINT_STATE_PAUSE);

  gst_object_unref (urielement);
}

GST_END_TEST static Suite *
uriendpoint_suite (void)
{
  Suite *s = suite_create ("uriendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_uri_prop);
  tcase_add_test (tc_chain, check_state_prop);

  return s;
}

GST_CHECK_MAIN (uriendpoint);
