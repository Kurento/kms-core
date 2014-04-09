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

GMainLoop *loop;

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
}

GST_START_TEST (push_buffers)
{
  GstBus *bus;
  GstElement *pipe = gst_parse_launch ("videotestsrc num-buffers=5 ! vp8enc ! "
      "vp8parse deadline=200000 threads=1 cpu-used=16 resize-allowed=TRUE "
      "target-bitrate=300000 end-usage=cbr ! fakesink", NULL);

  loop = g_main_loop_new (NULL, TRUE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipe);

  gst_element_set_state (pipe, GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (pipe, GST_STATE_NULL);
  g_object_unref (bus);
  g_object_unref (pipe);
}

GST_END_TEST
GST_START_TEST (create_element)
{
  GstElement *vp8parse;

  vp8parse = gst_element_factory_make ("vp8parse", NULL);

  fail_unless (vp8parse != NULL);

  g_object_unref (vp8parse);
}

GST_END_TEST static Suite *
vp8parse_suite (void)
{
  Suite *s = suite_create ("vp8parse");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_element);
  tcase_add_test (tc_chain, push_buffers);

  return s;
}

GST_CHECK_MAIN (vp8parse);
