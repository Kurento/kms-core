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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

static gboolean
quit_main_loop (gpointer data)
{
  g_main_loop_quit (data);

  return G_SOURCE_REMOVE;
}

GST_START_TEST (test_buffer_injector)
{
  GstElement *pipeline, *videotestsrc, *bufferinjector, *fakesink;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("bufferinjector0-test");
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  bufferinjector = gst_element_factory_make ("bufferinjector", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, bufferinjector,
      fakesink, NULL);
  gst_element_link (videotestsrc, bufferinjector);
  gst_element_link (bufferinjector, fakesink);

  g_timeout_add (4000, quit_main_loop, loop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  GST_DEBUG ("Setting pipline to NULL state");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_DEBUG ("Releasing pipeline");
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipeline released");
  g_main_loop_unref (loop);
}

GST_END_TEST;

static Suite *
buffer_injector_suite (void)
{
  Suite *s = suite_create ("kmsbufferinjector");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_buffer_injector);
  return s;
}

GST_CHECK_MAIN (buffer_injector);
