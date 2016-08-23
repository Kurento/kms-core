/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gst/check/gstcheck.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>

#include <kmsrtpsynchronizer.h>

/* based on rtpjitterbuffer.c */
static GstBuffer *
generate_rtp_buffer_full (GstClockTime gst_ts, guint32 ssrc, guint pt,
    guint seq_num, guint32 rtp_ts)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *buf;

  buf = gst_rtp_buffer_new_allocate (0, 0, 0);
  GST_BUFFER_DTS (buf) = gst_ts;
  GST_BUFFER_PTS (buf) = gst_ts;

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_payload_type (&rtp, pt);
  gst_rtp_buffer_set_ssrc (&rtp, ssrc);
  gst_rtp_buffer_set_seq (&rtp, seq_num);
  gst_rtp_buffer_set_timestamp (&rtp, rtp_ts);
  gst_rtp_buffer_unmap (&rtp);

  return buf;
}

static GstBuffer *
generate_rtcp_sr_buffer_full (guint32 ssrc, guint64 ntptime, guint32 rtptime)
{
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;
  GstBuffer *buf;

  buf = gst_rtcp_buffer_new (1400);
  fail_unless (buf != NULL);
  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);
  fail_unless (gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_SR,
          &packet) == TRUE);
  gst_rtcp_packet_sr_set_sender_info (&packet, ssrc, ntptime, rtptime, 0, 0);
  gst_rtcp_buffer_unmap (&rtcp);

  return buf;
}

GST_START_TEST (test_sync_context)
{
  KmsRtpSyncContext *ctx;
  GstClockTime ntp_ns_time_out, running_time_out;

  ctx = kms_rtp_sync_context_new ();

  kms_rtp_sync_context_get_time_matching (ctx, 0, 0, &ntp_ns_time_out,
      &running_time_out);
  fail_unless (ntp_ns_time_out == 0);
  fail_unless (running_time_out == 0);

  kms_rtp_sync_context_get_time_matching (ctx, 1, 1, &ntp_ns_time_out,
      &running_time_out);
  fail_unless (ntp_ns_time_out == 0);
  fail_unless (running_time_out == 0);

  g_object_unref (ctx);
}

GST_END_TEST;

GST_START_TEST (test_sync_add_clock_rate_for_pt)
{
  KmsRtpSynchronizer *sync;
  GstBuffer *buf;

  sync = kms_rtp_synchronizer_new (NULL);

  buf = generate_rtcp_sr_buffer_full (0x1, G_GUINT64_CONSTANT (0), 0);
  fail_unless (kms_rtp_synchronizer_process_rtcp_buffer (sync, buf, 0, NULL));
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (100, 0x1, 96, 0, 0);  /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_if (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 100);
  gst_buffer_unref (buf);

  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, -1, NULL));
  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 0, NULL));
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  buf = generate_rtp_buffer_full (200, 0x1, 96, 0, 0);  /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000, NULL));
  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 8000, NULL));

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_one_stream)
{
  KmsRtpSynchronizer *sync;
  GstBuffer *buf;

  sync = kms_rtp_synchronizer_new (NULL);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  /* Buffer before RTCP SR: it mantains the PTS */
  buf = generate_rtp_buffer_full (0, 0x1, 96, 0, 0);    /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  /* Buffer before RTCP SR: it mantains the PTS */
  buf = generate_rtp_buffer_full (100, 0x1, 96, 1, 0);  /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 100);
  gst_buffer_unref (buf);

  buf = generate_rtcp_sr_buffer_full (0x1, G_GUINT64_CONSTANT (0), 0);
  fail_unless (kms_rtp_synchronizer_process_rtcp_buffer (sync, buf, 0, NULL));
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (200, 0x1, 96, 2, 0);  /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (200, 0x1, 96, 3, 90000);      /* Video frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == GST_SECOND);
  gst_buffer_unref (buf);

  /* A huge gap in the gst ts */
  buf = generate_rtp_buffer_full (50000, 0x1, 96, 4, 90000);    /* Video frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == GST_SECOND);
  gst_buffer_unref (buf);

  /* Packet 5 missed */
  buf = generate_rtp_buffer_full (50100, 0x1, 96, 6, 180000);   /* Video frame 2 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 2 * GST_SECOND);
  gst_buffer_unref (buf);

  /* Packet 5 arrives */
  buf = generate_rtp_buffer_full (50100, 0x1, 96, 5, 90000);    /* Video frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == GST_SECOND);
  gst_buffer_unref (buf);

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_one_stream_rtptime_after_sr_rtptime)
{
  KmsRtpSynchronizer *sync;
  GstBuffer *buf;
  guint64 ntptime;

  sync = kms_rtp_synchronizer_new (NULL);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  /* Buffer before RTCP SR: it mantains the PTS */
  buf = generate_rtp_buffer_full (0, 0x1, 96, 0, 0);    /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  /* Buffer before RTCP SR: it mantains the PTS */
  buf = generate_rtp_buffer_full (100, 0x1, 96, 1, 0);  /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 100);
  gst_buffer_unref (buf);

  ntptime = gst_util_uint64_scale (GST_SECOND, (1LL << 32), GST_SECOND);
  buf = generate_rtcp_sr_buffer_full (0x1, ntptime, 90000);
  fail_unless (kms_rtp_synchronizer_process_rtcp_buffer (sync, buf, GST_SECOND,
          NULL));
  gst_buffer_unref (buf);

  /* An RTP packet with rtptime<last_sr_rtptime arrives after last SR */
  buf = generate_rtp_buffer_full (200, 0x1, 96, 2, 0);  /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (200, 0x1, 96, 3, 90000);      /* Video frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == GST_SECOND);
  gst_buffer_unref (buf);

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_two_streams)
{
  KmsRtpSyncContext *ctx;
  KmsRtpSynchronizer *sync_audio, *sync_video;
  GstBuffer *buf;
  guint64 ntptime;

  ctx = kms_rtp_sync_context_new ();
  sync_audio = kms_rtp_synchronizer_new (ctx);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync_audio, 0, 8000,
          NULL));
  sync_video = kms_rtp_synchronizer_new (ctx);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync_video, 96,
          90000, NULL));
  g_object_unref (ctx);

  /* Audio RTCP SR */
  ntptime = gst_util_uint64_scale (GST_SECOND, (1LL << 32), GST_SECOND);
  buf = generate_rtcp_sr_buffer_full (0x1, ntptime, 8000);
  fail_unless (kms_rtp_synchronizer_process_rtcp_buffer (sync_audio, buf,
          GST_SECOND, NULL));
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (0, 0x1, 0, 0, 0);     /* Audio frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync_audio, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (0, 0x1, 0, 1, 8000);  /* Audio frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync_audio, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == GST_SECOND);
  gst_buffer_unref (buf);

  /* Video RTCP SR */
  buf = generate_rtcp_sr_buffer_full (0x2, G_GUINT64_CONSTANT (0), 0);
  fail_unless (kms_rtp_synchronizer_process_rtcp_buffer (sync_video, buf,
          GST_SECOND, NULL));
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (0, 0x1, 96, 0, 0);    /* Video frame 0 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync_video, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 0);
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (0, 0x1, 96, 1, 90000);        /* Video frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync_video, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == GST_SECOND);
  gst_buffer_unref (buf);

  buf = generate_rtp_buffer_full (0, 0x1, 96, 1, 180000);       /* Video frame 1 */
  GST_DEBUG ("in PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (kms_rtp_synchronizer_process_rtp_buffer (sync_video, buf, NULL));
  GST_DEBUG ("out PTS: %lu", GST_BUFFER_PTS (buf));
  fail_unless (GST_BUFFER_PTS (buf) == 2 * GST_SECOND);
  gst_buffer_unref (buf);

  g_object_unref (sync_audio);
  g_object_unref (sync_video);
}

GST_END_TEST;

static Suite *
rtpsync_suite (void)
{
  Suite *s = suite_create ("rtpsync");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_sync_context);

  tcase_add_test (tc_chain, test_sync_add_clock_rate_for_pt);
  tcase_add_test (tc_chain, test_sync_one_stream);
  tcase_add_test (tc_chain, test_sync_one_stream_rtptime_after_sr_rtptime);
  tcase_add_test (tc_chain, test_sync_two_streams);

  return s;
}

GST_CHECK_MAIN (rtpsync);
