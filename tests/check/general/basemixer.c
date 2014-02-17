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
#include "kmsbasemixer.h"
#include "kmsmixerendpoint.h"

GST_START_TEST (link_port_after_internal_link)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  KmsBaseMixer *mixer = g_object_new (KMS_TYPE_BASE_MIXER, NULL);
  KmsMixerEndPoint *mixer_end_point =
      g_object_new (KMS_TYPE_MIXER_END_POINT, NULL);
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiosrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *videofakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *audiofakesink = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;
  gint id;

  gst_bin_add_many (GST_BIN (pipe), GST_ELEMENT (mixer), videosrc, audiosrc,
      GST_ELEMENT (mixer_end_point), NULL);

  g_signal_emit_by_name (mixer, "handle-port", mixer_end_point, &id);
  fail_unless (id >= 0);

  gst_bin_add (GST_BIN (mixer), videofakesink);
  ret =
      kms_base_mixer_link_video_sink (mixer, id, videofakesink, "sink", FALSE);
  fail_unless (ret == TRUE);

  {
    gchar *pad_name = g_strdup_printf ("video_src_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad == NULL);

    g_free (pad_name);
  }

  gst_element_link_pads (videosrc, "src", GST_ELEMENT (mixer_end_point),
      "video_sink");

  {
    gchar *pad_name = g_strdup_printf ("video_sink_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad != NULL);

    g_object_unref (pad);
    g_free (pad_name);
  }

  gst_bin_add (GST_BIN (mixer), audiofakesink);
  ret =
      kms_base_mixer_link_audio_sink (mixer, id, audiofakesink, "sink", FALSE);
  fail_unless (ret != FALSE);

  {
    gchar *pad_name = g_strdup_printf ("audio_src_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad == NULL);

    g_free (pad_name);
  }

  gst_element_link_pads (audiosrc, "src", GST_ELEMENT (mixer_end_point),
      "audio_sink");

  {
    gchar *pad_name = g_strdup_printf ("audio_sink_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad != NULL);

    g_object_unref (pad);
    g_free (pad_name);
  }

  g_signal_emit_by_name (mixer, "unhandle-port", id);

  {
    gchar *pad_name = g_strdup_printf ("audio_sink_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad == NULL);

    g_free (pad_name);
  }

  {
    gchar *pad_name = g_strdup_printf ("video_sink_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad == NULL);

    g_free (pad_name);
  }

  g_object_unref (pipe);
}

GST_END_TEST
GST_START_TEST (link_port_before_internal_link)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  KmsBaseMixer *mixer = g_object_new (KMS_TYPE_BASE_MIXER, NULL);
  KmsMixerEndPoint *mixer_end_point =
      g_object_new (KMS_TYPE_MIXER_END_POINT, NULL);
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiosrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *videofakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *audiofakesink = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;
  gint id;

  gst_bin_add_many (GST_BIN (pipe), GST_ELEMENT (mixer), videosrc, audiosrc,
      GST_ELEMENT (mixer_end_point), NULL);

  g_signal_emit_by_name (mixer, "handle-port", mixer_end_point, &id);
  fail_unless (id >= 0);

  gst_element_link_pads (videosrc, "src", GST_ELEMENT (mixer_end_point),
      "video_sink");
  gst_element_link_pads (audiosrc, "src", GST_ELEMENT (mixer_end_point),
      "audio_sink");

  ret =
      kms_base_mixer_link_video_sink (mixer, id, videofakesink, "sink", FALSE);
  fail_unless (ret == FALSE);

  {
    gchar *pad_name = g_strdup_printf ("video_src_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad == NULL);

    g_free (pad_name);
  }

  gst_bin_add (GST_BIN (mixer), videofakesink);
  ret =
      kms_base_mixer_link_video_sink (mixer, id, videofakesink, "sink", FALSE);
  fail_unless (ret != FALSE);

  {
    gchar *pad_name = g_strdup_printf ("video_sink_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad != NULL);

    g_object_unref (pad);
    g_free (pad_name);
  }

  ret =
      kms_base_mixer_link_audio_sink (mixer, id, audiofakesink, "sink", FALSE);
  fail_unless (ret == FALSE);

  {
    gchar *pad_name = g_strdup_printf ("audio_src_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad == NULL);

    g_free (pad_name);
  }

  gst_bin_add (GST_BIN (mixer), audiofakesink);
  ret =
      kms_base_mixer_link_audio_sink (mixer, id, audiofakesink, "sink", FALSE);
  fail_unless (ret != FALSE);

  {
    gchar *pad_name = g_strdup_printf ("audio_sink_%d", id);
    GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (mixer), pad_name);

    fail_unless (pad != NULL);

    g_object_unref (pad);
    g_free (pad_name);
  }

  g_object_unref (pipe);
}

END_TEST
GST_START_TEST (handle_port_action)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  KmsBaseMixer *mixer = g_object_new (KMS_TYPE_BASE_MIXER, NULL);
  KmsMixerEndPoint *mixer_end_point =
      g_object_new (KMS_TYPE_MIXER_END_POINT, NULL);
  gint id = -1;

  fail_unless (mixer != NULL);
  fail_unless (mixer_end_point != NULL);

  gst_bin_add_many (GST_BIN (pipe), GST_ELEMENT (mixer),
      GST_ELEMENT (mixer_end_point), NULL);

  g_signal_emit_by_name (mixer, "handle-port", mixer_end_point, &id);
  fail_unless (id >= 0);
  GST_DEBUG ("Got id: %d", id);
  fail_unless (G_OBJECT (mixer_end_point)->ref_count == 2);

  g_signal_emit_by_name (mixer, "unhandle-port", id);
  fail_unless (G_OBJECT (mixer_end_point)->ref_count == 1);

  g_signal_emit_by_name (mixer, "handle-port", mixer, &id);
  fail_unless (id < 0);

  g_object_unref (pipe);
}

GST_END_TEST
GST_START_TEST (create)
{
  KmsBaseMixer *mixer = g_object_new (KMS_TYPE_BASE_MIXER, NULL);

  fail_unless (mixer != NULL);

  g_object_unref (mixer);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
base_mixer_suite (void)
{
  Suite *s = suite_create ("basemixer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create);
  tcase_add_test (tc_chain, handle_port_action);
  tcase_add_test (tc_chain, link_port_before_internal_link);
  tcase_add_test (tc_chain, link_port_after_internal_link);

  return s;
}

GST_CHECK_MAIN (base_mixer);
