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

#define AGNOSTIC_KEY "agnostic"
#define DECODER_KEY "decoder"

#define VALVE_KEY "valve"

typedef struct _ElementsData
{
  GstElement *agnosticbin;
  GstElement *fakesink;
} ElementsData;

#ifdef DEBUGGING_TESTS
static GMainLoop *loop;
#endif

static gboolean
quit_main_loop_idle (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);
  return FALSE;
}

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
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
      if (g_str_has_prefix (GST_OBJECT_NAME (msg->src), "agnosticbin2")) {
        GST_INFO ("Event: %" GST_PTR_FORMAT, msg);
      }
    }
      break;
    default:
      break;
  }
}

static void
fakesink_hand_off (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    gpointer data)
{
  static int count = 0;
  GMainLoop *loop = (GMainLoop *) data;

  if (count++ > 40) {
    g_object_set (G_OBJECT (fakesink), "signal-handoffs", FALSE, NULL);
    g_idle_add (quit_main_loop_idle, loop);
  }
}

static gboolean
reconnect_elements (gpointer data)
{
  ElementsData *elements = data;
  GstPad *pad = gst_element_get_request_pad (elements->agnosticbin, "src_%u");

  gst_element_link_pads (elements->agnosticbin, GST_OBJECT_NAME (pad),
      elements->fakesink, NULL);
  g_object_unref (pad);

  return FALSE;
}

static void
free_elements (gpointer data)
{
  ElementsData *elements = data;

  g_object_unref (elements->agnosticbin);
  g_object_unref (elements->fakesink);

  g_slice_free (ElementsData, elements);
}

static void
fakesink_hand_off_simple (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    gpointer data)
{
  static int count = 0;
  GMainLoop *loop = (GMainLoop *) data;

  count++;

  if (count == 40) {
    GstPad *peer = gst_pad_get_peer (pad);
    ElementsData *elements = g_slice_new (ElementsData);

    elements->agnosticbin = gst_pad_get_parent_element (peer);
    elements->fakesink = g_object_ref (fakesink);

    gst_pad_unlink (peer, pad);
    g_object_unref (peer);

    g_idle_add_full (G_PRIORITY_DEFAULT, reconnect_elements, elements,
        free_elements);
  } else if (count > 100) {
    g_object_set (G_OBJECT (fakesink), "signal-handoffs", FALSE, NULL);
    g_idle_add (quit_main_loop_idle, loop);
  }
}

#ifdef DEBUGGING_TESTS
static gboolean
link_again (gpointer data)
{
  GstElement *decoder = (GstElement *) data;
  GstElement *agnostic = g_object_get_data (G_OBJECT (data), AGNOSTIC_KEY);

  GST_DEBUG ("Linking again %" GST_PTR_FORMAT ", %" GST_PTR_FORMAT, agnostic,
      decoder);
  gst_element_link (agnostic, decoder);

  return FALSE;
}

static gboolean
idle_unlink (gpointer data)
{
  GstPad *sink, *src;

  GstElement *decoder = (GstElement *) data;
  GstElement *agnostic = g_object_get_data (G_OBJECT (decoder), AGNOSTIC_KEY);

  sink = gst_element_get_static_pad (decoder, "sink");
  src = gst_pad_get_peer (sink);

  gst_pad_unlink (src, sink);

  gst_element_release_request_pad (agnostic, src);
  g_object_unref (src);
  g_object_unref (sink);

  g_timeout_add (200, link_again, decoder);

  return FALSE;
}

static void
fakesink_hand_off2 (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    gpointer data)
{
  static int count = 0;
  static int cycles = 0;
  GMainLoop *loop = (GMainLoop *) data;

  if (count++ > 10) {
    count = 0;
    if (cycles++ > 10) {
      GST_DEBUG ("Quit loop");
      g_idle_add (quit_main_loop_idle, loop);
    } else {
      GstElement *decoder =
          g_object_get_data (G_OBJECT (fakesink), DECODER_KEY);

      mark_point ();
      g_idle_add (idle_unlink, decoder);
    }
  }
}
#endif

static gboolean
timeout_check (gpointer pipeline)
{
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "timeout_test_end");
  return FALSE;
}

#ifdef DEBUGGING_TESTS
static gpointer
toggle_thread (gpointer data)
{
  GstElement *pipeline = GST_ELEMENT (data);
  GstElement *valve = g_object_get_data (G_OBJECT (pipeline), VALVE_KEY);
  gint i;

  g_usleep (800000);

  for (i = 0; i < 50; i++) {
    if (g_main_loop_is_running (loop)) {
      g_object_set (valve, "drop", i % 2, NULL);
    } else {
      GST_DEBUG ("Main loop stopped");
      break;
    }
    g_usleep (10000);
  }

  GST_DEBUG ("Toggle thread finished");
  g_idle_add (quit_main_loop_idle, loop);

  return NULL;
}

static void
bus_msg_valve_test (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (msg->type) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      g_main_loop_quit (loop);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
      if (msg->src == pipe) {
        GstState new_state;

        gst_message_parse_state_changed (msg, NULL, &new_state, NULL);

        if (new_state == GST_STATE_PLAYING) {
          GThread *thread;

          GST_INFO ("Pipe started: %" GST_PTR_FORMAT, msg);

          thread = g_thread_new ("toggle", toggle_thread, pipe);
          g_thread_unref (thread);
        }
      }
    }
      break;
    default:
      break;
  }
}

GST_START_TEST (valve_test)
{
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin2", NULL);
  GstElement *valve = gst_element_factory_make ("valve", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set_data (G_OBJECT (pipeline), VALVE_KEY, valve);

  loop = g_main_loop_new (NULL, TRUE);
  g_object_set (G_OBJECT (pipeline), "async-handling", FALSE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg_valve_test), pipeline);

  g_object_set_data (G_OBJECT (fakesink2), DECODER_KEY, decoder);
  g_object_set_data (G_OBJECT (decoder), AGNOSTIC_KEY, agnosticbin);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin, fakesink,
      valve, decoder, fakesink2, NULL);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, agnosticbin, fakesink, NULL);
  fail_unless (ret);
  mark_point ();
  ret = gst_element_link_many (agnosticbin, valve, decoder, fakesink2, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "loopback_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (pipeline);
  g_object_unref (bus);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (reconnect_test)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin2", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink2), "sync", FALSE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (G_OBJECT (fakesink2), "handoff",
      G_CALLBACK (fakesink_hand_off2), loop);
  g_object_set_data (G_OBJECT (fakesink2), DECODER_KEY, decoder);
  g_object_set_data (G_OBJECT (decoder), AGNOSTIC_KEY, agnosticbin);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin, fakesink,
      decoder, fakesink2, NULL);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, agnosticbin, fakesink, NULL);
  fail_unless (ret);
  mark_point ();
  ret = gst_element_link_many (agnosticbin, decoder, fakesink2, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "loopback_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
#endif
GST_START_TEST (static_link)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin2", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink2), "sync", FALSE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (G_OBJECT (fakesink2), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin, fakesink,
      decoder, fakesink2, NULL);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, agnosticbin, fakesink, NULL);
  fail_unless (ret);
  mark_point ();
  ret = gst_element_link_many (agnosticbin, decoder, fakesink2, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "loopback_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (encoded_input_link)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *encoder = gst_element_factory_make ("vp8enc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *filter = gst_element_factory_make ("capsfilter", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin2", NULL);
  GstCaps *caps;
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink), "sync", FALSE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  caps = gst_caps_from_string ("video/x-raw");
  g_object_set (G_OBJECT (filter), "caps", caps, NULL);
  gst_caps_unref (caps);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, encoder, agnosticbin,
      filter, fakesink, NULL);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, encoder, agnosticbin, filter,
      fakesink, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "simple_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (simple_link)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin2", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink), "sync", FALSE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off_simple), loop);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin, fakesink,
      NULL);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, agnosticbin, fakesink, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "simple_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (create_test)
{
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin2", NULL);
  GstPad *pad;

  pad = gst_element_get_request_pad (agnosticbin, "src_%u");

  GST_DEBUG_OBJECT (pad, "Pad created");

  gst_element_release_request_pad (agnosticbin, pad);

  g_object_unref (agnosticbin);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
agnostic2_suite (void)
{
  Suite *s = suite_create ("agnosticbin2");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_test);
  tcase_add_test (tc_chain, simple_link);
  tcase_add_test (tc_chain, encoded_input_link);
  tcase_add_test (tc_chain, static_link);
#ifdef DEBUGGING_TESTS
  tcase_add_test (tc_chain, reconnect_test);
  tcase_add_test (tc_chain, valve_test);
#endif

  return s;
}

GST_CHECK_MAIN (agnostic2);
