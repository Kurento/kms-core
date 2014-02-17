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
#include "kmsmainmixer.h"

GST_START_TEST (create)
{
  GstElement *pipe = gst_pipeline_new (__FUNCTION__);
  GstElement *mixer = gst_element_factory_make ("mainmixer", NULL);
  GstElement *mixerendpoint = gst_element_factory_make ("mixerendpoint", NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);

  gst_bin_add_many (GST_BIN (pipe), mixerendpoint, mixer, audiotestsrc,
      videotestsrc, NULL);
  gst_element_set_state (pipe, GST_STATE_NULL);

  if (!gst_element_link (videotestsrc, mixerendpoint))
    fail ("videotestsrc could not be linked.");

  gst_element_link (mixerendpoint, mixer);

  g_object_unref (pipe);
}

GST_END_TEST static void
cb_new_pad (GstElement * element, GstPad * pad, gpointer data)
{
  GstPad *sink_pad;
  GstElement *endpoint;

  endpoint = GST_ELEMENT (data);
  sink_pad = gst_element_get_request_pad (endpoint, "video_sink");

  gst_pad_link (pad, sink_pad);
  gst_object_unref (GST_OBJECT (sink_pad));

}

GST_START_TEST (manage_ports)
{
  GstElement *pipe = gst_pipeline_new (__FUNCTION__);
  GstElement *mixer = gst_element_factory_make ("mainmixer", NULL);
  GstElement *mixerendpoint1 = gst_element_factory_make ("mixerendpoint", NULL);
  GstElement *mixerendpoint2 = gst_element_factory_make ("mixerendpoint", NULL);
  GstElement *player1 = gst_element_factory_make ("uridecodebin", NULL);
  GstElement *player2 = gst_element_factory_make ("uridecodebin", NULL);
  gint handlerId1, handlerId2;

  g_signal_connect (player1, "pad-added", G_CALLBACK (cb_new_pad),
      mixerendpoint1);
  g_signal_connect (player2, "pad-added", G_CALLBACK (cb_new_pad),
      mixerendpoint2);

  gst_bin_add_many (GST_BIN (pipe), player1, player2, mixerendpoint1,
      mixerendpoint2, mixer, NULL);
  gst_element_set_state (pipe, GST_STATE_NULL);

  g_object_set (player1, "uri", "https://ci.kurento.com/video/sintel.webm",
      NULL);
  g_object_set (player2, "uri", "https://ci.kurento.com/video/sintel.webm",
      NULL);

  g_signal_emit_by_name (mixer, "handle-port", mixerendpoint1, &handlerId1);
  g_signal_emit_by_name (mixer, "handle-port", mixerendpoint2, &handlerId2);

  g_object_set (mixer, "main", handlerId1, NULL);
  g_object_set (mixer, "main", handlerId2, NULL);

  g_object_set (mixer, "main", -1, NULL);

  g_signal_emit_by_name (mixer, "unhandle-port", handlerId1);
  g_signal_emit_by_name (mixer, "unhandle-port", handlerId2);

  g_object_unref (pipe);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
main_mixer_suite (void)
{
  Suite *s = suite_create ("mainmixer");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create);
  tcase_add_test (tc_chain, manage_ports);

  return s;
}

GST_CHECK_MAIN (main_mixer);
