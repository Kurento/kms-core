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
#define FAKESINK_KEY "fakesink"

#define VALVE_KEY "valve"
#define COUNT_KEY "count"

/*
 * By now we only enable one output, otherwise the test will probably fail
 * once the bug is solved this value should be incremented
 */
#define N_ITERS 200

typedef struct _ElementsData
{
  GstElement *agnosticbin;
  GstElement *fakesink;
} ElementsData;

static GMainLoop *loop;

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
      gchar *error_file = g_strdup_printf ("error-%s", GST_OBJECT_NAME (pipe));

      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, error_file);
      g_free (error_file);
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      gchar *warn_file = g_strdup_printf ("warning-%s", GST_OBJECT_NAME (pipe));

      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, warn_file);
      g_free (warn_file);
      break;
    }
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

static gboolean
link_again (gpointer data)
{
  GstElement *decoder = (GstElement *) data;
  GstElement *agnostic = g_object_get_data (G_OBJECT (data), AGNOSTIC_KEY);
  GstElement *fakesink = g_object_get_data (G_OBJECT (decoder), FAKESINK_KEY);

  GST_DEBUG ("Linking again %" GST_PTR_FORMAT ", %" GST_PTR_FORMAT, agnostic,
      decoder);

  g_object_set (G_OBJECT (fakesink), "signal-handoffs", TRUE, NULL);
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
      g_object_set (G_OBJECT (fakesink), "signal-handoffs", FALSE, NULL);
      GST_DEBUG ("Quit loop");
      g_idle_add (quit_main_loop_idle, loop);
    } else {
      GstElement *decoder =
          g_object_get_data (G_OBJECT (fakesink), DECODER_KEY);

      g_object_set (G_OBJECT (fakesink), "signal-handoffs", FALSE, NULL);
      mark_point ();
      g_idle_add (idle_unlink, decoder);
    }
  }
}

static gboolean
timeout_check (gpointer pipeline)
{
  if (GST_IS_BIN (pipeline)) {
    gchar *timeout_file =
        g_strdup_printf ("timeout-%s", GST_OBJECT_NAME (pipeline));

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
        GST_DEBUG_GRAPH_SHOW_ALL, timeout_file);
    g_free (timeout_file);
  }

  return FALSE;
}

static gpointer
toggle_thread (gpointer data)
{
  GstElement *pipeline = GST_ELEMENT (data);
  GstElement *valve = g_object_get_data (G_OBJECT (pipeline), VALVE_KEY);
  GstPad *sink = gst_element_get_static_pad (valve, "sink");
  gint i;

  g_usleep (800000);

  for (i = 0; i < 15; i++) {
    if (g_main_loop_is_running (loop)) {
      g_object_set (valve, "drop", i % 2, NULL);
      g_usleep (20000);
    } else {
      GST_DEBUG ("Main loop stopped");
      break;
    }
  }

  g_usleep (100000);

  g_object_set (valve, "drop", FALSE, NULL);

  g_object_unref (sink);

  GST_DEBUG ("Toggle thread finished");
  {
    GstElement *fakesink = gst_bin_get_by_name (GST_BIN (pipeline), "fakesink");

    GST_DEBUG_OBJECT (fakesink, "Found");
    g_signal_connect (G_OBJECT (fakesink), "handoff",
        G_CALLBACK (fakesink_hand_off), loop);
    g_object_set (G_OBJECT (fakesink), "signal-handoffs", TRUE, NULL);
    g_object_unref (fakesink);
  }

  return NULL;
}

static gboolean
link_source (gpointer data)
{
  GstElement *pipeline = data;
  GstElement *agnosticbin =
      gst_bin_get_by_name (GST_BIN (pipeline), "agnosticbin");
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);

  g_object_set (G_OBJECT (videosrc), "is-live", TRUE, NULL);
  gst_bin_add_many (GST_BIN (pipeline), videosrc, NULL);
  gst_element_link (videosrc, agnosticbin);
  gst_element_sync_state_with_parent (videosrc);

  g_object_unref (agnosticbin);

  return FALSE;
}

static void
type_found (GstElement * typefind, guint prob, GstCaps * caps,
    GstElement * pipeline)
{
  GstElement *fakesink = gst_bin_get_by_name (GST_BIN (pipeline), "fakesink");
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);

  gst_bin_add (GST_BIN (pipeline), agnosticbin);
  gst_element_sync_state_with_parent (agnosticbin);

  gst_element_link (agnosticbin, fakesink);
  gst_element_link (typefind, agnosticbin);

  g_object_unref (fakesink);
}

static void
change_input (gpointer pipeline)
{
  GstBin *pipe = GST_BIN (pipeline);
  GstElement *agnosticbin;
  GstPad *peer, *sink;
  GstElement *agnosticbin2 = gst_bin_get_by_name (pipe, "agnosticbin_2");
  GstElement *enc = gst_element_factory_make ("vp8enc", NULL);
  GstElement *fakesink = gst_bin_get_by_name (pipe, "fakesink");

  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);
  g_object_unref (fakesink);

  gst_bin_add (pipe, enc);
  gst_element_sync_state_with_parent (enc);

  sink = gst_element_get_static_pad (agnosticbin2, "sink");
  peer = gst_pad_get_peer (sink);
  agnosticbin = gst_pad_get_parent_element (peer);
  gst_pad_unlink (peer, sink);

  GST_INFO ("Got peer: %" GST_PTR_FORMAT, peer);
  gst_element_release_request_pad (agnosticbin, peer);
  gst_element_link (enc, agnosticbin2);
  gst_element_link (agnosticbin, enc);

  g_object_unref (agnosticbin);
  g_object_unref (agnosticbin2);
  g_object_unref (sink);
  g_object_unref (peer);
}

static GstPadProbeReturn
block_agnostic_sink (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  static gboolean configuring = FALSE;

  /* HACK: Ignore caps event and stream start event that causes negotiation
   * failures.This is a workaround that should be removed
   */
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

    if (GST_EVENT_TYPE (event) == GST_EVENT_STREAM_START
        || GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
      return GST_PAD_PROBE_PASS;
    }
  }

  /* HACK: Ignore query accept caps that causes negotiation errors.
   * This is a workaround that should be removed
   */
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);

    if (GST_QUERY_TYPE (query) == GST_QUERY_ACCEPT_CAPS) {
      return GST_PAD_PROBE_PASS;
    }
  }

  if (!g_atomic_int_get (&configuring)) {
    g_atomic_int_set (&configuring, TRUE);
    change_input (data);
    g_atomic_int_set (&configuring, FALSE);
    return GST_PAD_PROBE_REMOVE;
  }

  return GST_PAD_PROBE_PASS;
}

static gboolean
change_input_cb (gpointer pipeline)
{
  GstBin *pipe = GST_BIN (pipeline);
  GstPad *sink;
  GstElement *agnosticbin2 = gst_bin_get_by_name (pipe, "agnosticbin_2");

  sink = gst_element_get_static_pad (agnosticbin2, "sink");
  gst_pad_add_probe (sink, GST_PAD_PROBE_TYPE_BLOCK, block_agnostic_sink,
      pipeline, NULL);
  // HACK: Sending a dummy event to ensure the block probe is called
  gst_pad_send_event (sink,
      gst_event_new_custom (GST_EVENT_TYPE_DOWNSTREAM,
          gst_structure_new_from_string ("dummy")));

  g_object_unref (agnosticbin2);
  g_object_unref (sink);
  return FALSE;
}

static GstFlowReturn
appsink_handle (GstElement * appsink, gpointer data)
{
  static int count = 0;
  GstSample *sample;

  count++;

  g_signal_emit_by_name (appsink, "pull-sample", &sample);
  gst_sample_unref (sample);

  if (count == 40) {
    GST_DEBUG ("Terminatig");
    g_idle_add (quit_main_loop_idle, loop);
  }

  return GST_FLOW_OK;
}

static gboolean
check_pipeline_termination (gpointer data)
{
  GstElement *pipeline = GST_ELEMENT (data);
  int *count = g_object_get_data (G_OBJECT (pipeline), COUNT_KEY);

  if (count != NULL && g_atomic_int_dec_and_test (count)) {
    GST_DEBUG ("Terminating main loop");
    quit_main_loop_idle (loop);
  }

  return FALSE;
}

static GstFlowReturn
appsink_handle_many (GstElement * appsink, gpointer data)
{
  int *count = g_object_get_data (G_OBJECT (appsink), COUNT_KEY);
  GstSample *sample;

  if (count == NULL) {
    count = g_malloc0 (sizeof (int));
    g_object_set_data_full (G_OBJECT (appsink), COUNT_KEY, count, g_free);
  }

  g_signal_emit_by_name (appsink, "pull-sample", &sample);
  gst_sample_unref (sample);

  if (g_atomic_int_add (count, 1) == 40) {
    GST_DEBUG_OBJECT (appsink, "Terminatig");
    g_idle_add (check_pipeline_termination, GST_OBJECT_PARENT (appsink));
  }

  return GST_FLOW_OK;
}

static gboolean
connect_appsink (gpointer pipeline)
{
  GstElement *appsink = gst_element_factory_make ("appsink", NULL);
  GstElement *valve = gst_bin_get_by_name (GST_BIN (pipeline), "valve");
  GstCaps *caps = gst_caps_from_string ("audio/x-vorbis");

  gst_bin_add (GST_BIN (pipeline), appsink);
  gst_element_sync_state_with_parent (appsink);
  g_object_set (G_OBJECT (appsink), "emit-signals", TRUE, "caps", caps, "sync",
      TRUE, "async", FALSE, NULL);
  gst_caps_unref (caps);

  g_signal_connect (G_OBJECT (appsink), "new-sample",
      G_CALLBACK (appsink_handle), NULL);

  gst_element_link (valve, appsink);

  g_object_set (G_OBJECT (valve), "drop", FALSE, NULL);

  g_object_unref (valve);
  return FALSE;
}

static gboolean
connect_output (gpointer pipeline)
{
  GstElement *agnosticbin =
      gst_bin_get_by_name (GST_BIN (pipeline), "agnosticbin");
  GstElement *appsink = gst_element_factory_make ("appsink", NULL);
  GstCaps *caps = gst_caps_from_string ("audio/x-vorbis");

  g_object_set (G_OBJECT (appsink), "emit-signals", TRUE, "caps", caps, "sync",
      TRUE, "async", FALSE, NULL);
  gst_caps_unref (caps);

  g_signal_connect (G_OBJECT (appsink), "new-sample",
      G_CALLBACK (appsink_handle_many), NULL);

  gst_bin_add (GST_BIN (pipeline), appsink);
  gst_element_sync_state_with_parent (appsink);

  if (!gst_element_link (agnosticbin, appsink)) {
    GST_ERROR ("Error linking elements");
  }

  g_object_unref (agnosticbin);

  return FALSE;
}

GST_START_TEST (input_reconfiguration)
{
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *agnosticbin =
      gst_element_factory_make ("agnosticbin", "agnosticbin_1");
  GstElement *agnosticbin2 =
      gst_element_factory_make ("agnosticbin", "agnosticbin_2");
  GstElement *fakesink = gst_element_factory_make ("fakesink", "fakesink");

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  loop = g_main_loop_new (NULL, TRUE);

  g_object_set (G_OBJECT (videosrc), "is-live", TRUE, NULL);
  g_object_set (G_OBJECT (fakesink), "sync", TRUE, "signal-handoffs", TRUE,
      "async", FALSE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  gst_bin_add_many (GST_BIN (pipeline), videosrc, agnosticbin, agnosticbin2,
      fakesink, NULL);
  gst_element_link_many (videosrc, agnosticbin, agnosticbin2, fakesink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_timeout_add_seconds (1, change_input_cb, pipeline);

  g_timeout_add_seconds (6, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (pipeline);
  g_object_unref (bus);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (add_later)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *typefind = gst_element_factory_make ("typefind", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", "fakesink");
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink), "sync", TRUE, "signal-handoffs", TRUE,
      "async", FALSE, NULL);
  g_object_set (G_OBJECT (videosrc), "is-live", TRUE, NULL);
  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  g_signal_connect (G_OBJECT (typefind), "have-type", G_CALLBACK (type_found),
      pipeline);

  gst_bin_add_many (GST_BIN (pipeline), videosrc, typefind, fakesink, NULL);
  gst_element_link (videosrc, typefind);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (pipeline);
  g_object_unref (bus);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (delay_stream)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *agnosticbin =
      gst_element_factory_make ("agnosticbin", "agnosticbin");
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink), "async", FALSE, "sync", TRUE,
      "signal-handoffs", TRUE, NULL);
  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  gst_bin_add_many (GST_BIN (pipeline), agnosticbin, fakesink, NULL);
  gst_element_link (agnosticbin, fakesink);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_timeout_add_seconds (1, link_source, pipeline);
  g_timeout_add_seconds (11, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (pipeline);
  g_object_unref (bus);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (valve_test)
{
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *valve = gst_element_factory_make ("valve", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", "fakesink");
  gboolean ret;
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  GThread *thread;

  g_object_set_data (G_OBJECT (pipeline), VALVE_KEY, valve);

  loop = g_main_loop_new (NULL, TRUE);
  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set_data (G_OBJECT (fakesink2), DECODER_KEY, decoder);
  g_object_set_data (G_OBJECT (decoder), AGNOSTIC_KEY, agnosticbin);

  g_object_set (G_OBJECT (fakesink2), "sync", TRUE, "async", FALSE, NULL);
  g_object_set (G_OBJECT (fakesink), "sync", TRUE, "async", FALSE, NULL);

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

  thread = g_thread_new ("toggle", toggle_thread, pipeline);
  g_thread_unref (thread);

  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

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
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink2), "sync", TRUE, "signal-handoffs", TRUE,
      "async", FALSE, NULL);
  g_signal_connect (G_OBJECT (fakesink2), "handoff",
      G_CALLBACK (fakesink_hand_off2), loop);
  g_object_set_data (G_OBJECT (fakesink2), DECODER_KEY, decoder);
  g_object_set_data (G_OBJECT (decoder), AGNOSTIC_KEY, agnosticbin);
  g_object_set_data (G_OBJECT (decoder), FAKESINK_KEY, fakesink2);

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

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (static_link)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink2), "sync", TRUE, "signal-handoffs", TRUE,
      "async", FALSE, NULL);
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

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (encoded_input_n_encoded_ouput)
{
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *encoder = gst_element_factory_make ("alawenc", NULL);
  GstElement *agnosticbin =
      gst_element_factory_make ("agnosticbin", "agnosticbin");
  gboolean ret;
  int *count, i;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  loop = g_main_loop_new (NULL, TRUE);

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (audiotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, encoder, agnosticbin,
      NULL);
  mark_point ();
  ret = gst_element_link_many (audiotestsrc, encoder, agnosticbin, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  mark_point ();

  count = g_malloc0 (sizeof (int));
  *count = N_ITERS;
  g_object_set_data_full (G_OBJECT (pipeline), COUNT_KEY, count, g_free);
  GST_INFO ("Connecting %d outputs", N_ITERS);

  g_timeout_add_seconds (6, timeout_check, pipeline);
  for (i = 0; i < N_ITERS; i++) {
    g_timeout_add (700, connect_output, pipeline);
  }

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (encoded_input_to_valve)
{
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *encoder = gst_element_factory_make ("alawenc", NULL);
  GstElement *valve = gst_element_factory_make ("valve", "valve");
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  loop = g_main_loop_new (NULL, TRUE);

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (audiotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (valve), "drop", TRUE, NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, encoder, agnosticbin,
      valve, NULL);
  mark_point ();
  ret = gst_element_link_many (audiotestsrc, encoder, agnosticbin, valve, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  mark_point ();
  g_timeout_add_seconds (6, timeout_check, pipeline);
  g_timeout_add (700, connect_appsink, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

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
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *encoder = gst_element_factory_make ("vp8enc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *filter = gst_element_factory_make ("capsfilter", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstCaps *caps;
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink), "sync", TRUE, "signal-handoffs", TRUE,
      "async", FALSE, NULL);
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

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

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
  GstElement *pipeline = gst_pipeline_new (__FUNCTION__);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (fakesink), "sync", TRUE, "signal-handoffs", TRUE,
      "async", FALSE, NULL);
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

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (create_test)
{
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstPad *pad;

  pad = gst_element_get_request_pad (agnosticbin, "src_%u");

  GST_DEBUG_OBJECT (pad, "Pad created");

  gst_element_release_request_pad (agnosticbin, pad);
  gst_object_unref (pad);

  g_object_unref (agnosticbin);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
agnostic2_suite (void)
{
  Suite *s = suite_create ("agnosticbin");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_test);
  tcase_add_test (tc_chain, simple_link);
  tcase_add_test (tc_chain, encoded_input_link);
  tcase_add_test (tc_chain, encoded_input_to_valve);
  tcase_add_test (tc_chain, static_link);
  tcase_add_test (tc_chain, reconnect_test);
  tcase_add_test (tc_chain, valve_test);
  tcase_add_test (tc_chain, delay_stream);
  tcase_add_test (tc_chain, add_later);
  tcase_add_test (tc_chain, input_reconfiguration);
  tcase_add_test (tc_chain, encoded_input_n_encoded_ouput);

  tcase_add_test (tc_chain, input_reconfiguration);

  return s;
}

GST_CHECK_MAIN (agnostic2);
