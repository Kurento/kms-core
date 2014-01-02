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

#include <kmscheck.h>

#define ITERATIONS 1

static int iterations = ITERATIONS;

static void
create_element (const gchar * element_name)
{
  GstElement *element = gst_element_factory_make (element_name, NULL);

  g_object_unref (element);
}

static void
play_element (const gchar * element_name)
{
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *element = gst_element_factory_make (element_name, NULL);

  gst_bin_add (GST_BIN (pipeline), element);

  gst_element_set_state (pipeline, GST_STATE_READY);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_object_unref (pipeline);
}

GST_START_TEST (test_create_rtcpdemux)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("rtcpdemux");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_dtlssrtpdemux)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("dtlssrtpdemux");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_srtpdec)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("srtpdec");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_dtlsdec)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("dtlsdec");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_dtlssrtpdec)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("dtlssrtpdec");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_dtlsenc)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("dtlsenc");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_dtlssrtpenc)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("dtlssrtpenc");
  }
}

KMS_END_TEST
GST_START_TEST (test_create_webrtcendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("webrtcendpoint");
  }
}

KMS_END_TEST
GST_START_TEST (test_play_webrtcendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    play_element ("webrtcendpoint");
  }
}

KMS_END_TEST
/*
 * End of test cases
 */
static Suite *
webrtcendpoint_suite (void)
{
  char *it_str;

  it_str = getenv ("ITERATIONS");
  if (it_str != NULL) {
    iterations = atoi (it_str);
    if (iterations <= 0)
      iterations = ITERATIONS;
  }

  Suite *s = suite_create ("webrtcendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_create_rtcpdemux);
  tcase_add_test (tc_chain, test_create_dtlssrtpdemux);

  tcase_add_test (tc_chain, test_create_srtpdec);
  tcase_add_test (tc_chain, test_create_dtlsdec);
  tcase_add_test (tc_chain, test_create_dtlssrtpdec);

  tcase_add_test (tc_chain, test_create_dtlsenc);
  tcase_add_test (tc_chain, test_create_dtlssrtpenc);

  tcase_add_test (tc_chain, test_create_webrtcendpoint);
  tcase_add_test (tc_chain, test_play_webrtcendpoint);

  return s;
}

KMS_CHECK_MAIN (webrtcendpoint);
