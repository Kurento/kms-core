/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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

GST_START_TEST (create_test)
{
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin3", NULL);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, agnosticbin, NULL);

  if (!gst_element_link_pads (audiotestsrc, "src", agnosticbin, "sink_%u")) {
    fail ("Could not link elements");
    return;
  }

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, GST_ELEMENT_NAME (pipeline));

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
agnosticbin3_suite (void)
{
  Suite *s = suite_create ("agnosticbin3");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_test);

  return s;
}

GST_CHECK_MAIN (agnosticbin3);
