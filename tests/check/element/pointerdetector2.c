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

GstElement *pointerdetector2 = NULL;
GstElement *point = NULL;
GMainLoop *loop;

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  const GstStructure *st;
  gchar *windowID;
  const gchar *type;

  st = gst_message_get_structure (msg);
  type = gst_structure_get_name (st);

  if ((g_strcmp0 (type, "window-out") != 0) &&
      (g_strcmp0 (type, "window-in") != 0)) {
    return;
  }

  if (!gst_structure_get (st, "window", G_TYPE_STRING, &windowID, NULL)) {
    return;
  }

  if (g_strcmp0 (windowID, "window_test") == 0) {
    GST_DEBUG ("event received");
    g_main_loop_quit (loop);
  }
  g_free (windowID);
}

/* set_encoded_media test */
GST_START_TEST (check_pointerdetector2)
{
  GstElement *pipeline;
  guint bus_watch_id;
  GstBus *bus;

  pointerdetector2 = gst_element_factory_make ("filterelement", NULL);
  point = gst_element_factory_make ("videotestsrc", NULL);

  g_object_set (pointerdetector2, "filter-factory", "pointerdetector2", NULL);

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (point), "pattern", 18, NULL);
  g_object_set (G_OBJECT (point), "num-buffers", 100000000, NULL);

  GstStructure *buttonsLayout;

  /* set the window layout list */
  buttonsLayout = gst_structure_new_empty ("windowsLayout");

  GstStructure *buttonsLayoutAux;

  buttonsLayoutAux = gst_structure_new ("window_test",
      "upRightCornerX", G_TYPE_INT, 1,
      "upRightCornerY", G_TYPE_INT, 1,
      "width", G_TYPE_INT, 300,
      "height", G_TYPE_INT, 230, "id", G_TYPE_STRING, "window_test", NULL);

  gst_structure_set (buttonsLayout,
      "window0", GST_TYPE_STRUCTURE, buttonsLayoutAux, NULL);
  gst_structure_free (buttonsLayoutAux);

  GstElement *pd;

  g_object_get (G_OBJECT (pointerdetector2), "filter", &pd, NULL);

  g_object_set (G_OBJECT (pd), "windows-layout", buttonsLayout, NULL);
  gst_structure_free (buttonsLayout);

  GstStructure *color;

  color = gst_structure_new ("color",
      "h_min", G_TYPE_INT, 0,
      "h_max", G_TYPE_INT, 1,
      "s_min", G_TYPE_INT, 0, "s_max", G_TYPE_INT, 1, NULL);
  g_object_set (G_OBJECT (pd), "color-target", color, NULL);
  gst_structure_free (color);

  g_object_unref (pd);

  gst_bin_add_many (GST_BIN (pipeline), point, pointerdetector2, NULL);

  gst_element_link_pads (point, "src", pointerdetector2, "video_sink");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

}

GST_END_TEST
/* Define test suite */
static Suite *
pointerdetector2_suite (void)
{
  Suite *s = suite_create ("pointerdetector2");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_pointerdetector2);
  return s;
}

GST_CHECK_MAIN (pointerdetector2);
