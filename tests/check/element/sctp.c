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

GST_START_TEST (sctpclientsink)
{
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *sctpclientsink = gst_element_factory_make ("sctpbasesink", NULL);

  g_object_set (sctpclientsink, "num-ostreams", 2, NULL);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, videotestsrc,
      sctpclientsink, NULL);

  if (!gst_element_link_pads (videotestsrc, "src", sctpclientsink, "sink_0"))
    fail ("videotestsrc could not be linked.");

  if (!gst_element_link_pads (audiotestsrc, "src", sctpclientsink, "sink_1"))
    fail ("audiotestsrc could not be linked.");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
sctp_suite (void)
{
  Suite *s = suite_create ("sctp");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, sctpclientsink);

  return s;
}

GST_CHECK_MAIN (sctp);
