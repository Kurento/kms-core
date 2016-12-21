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

#define process_rtp(sync, gst_ts, ssrc, pt, seq_num, rtp_ts, expected_out_pts, fail_func) \
do {                                                                                      \
  GstBuffer *__buf;                                                                       \
                                                                                          \
  __buf = generate_rtp_buffer_full (gst_ts, ssrc, pt, seq_num, rtp_ts);                   \
  GST_DEBUG ("in PTS: %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS (__buf)));         \
  fail_func (kms_rtp_synchronizer_process_rtp_buffer (sync, __buf, NULL));                \
  GST_DEBUG ("out PTS: %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS (__buf)));        \
  fail_unless (GST_BUFFER_PTS (__buf) == expected_out_pts);                               \
  gst_buffer_unref (__buf);                                                               \
} while (0)

#define process_rtcp(sync, ssrc, ntptime, rtptime, current_time)                            \
do {                                                                                        \
  GstBuffer *__buf;                                                                         \
                                                                                            \
  __buf = generate_rtcp_sr_buffer_full (0x1, ntptime, rtptime);                             \
  fail_unless (kms_rtp_synchronizer_process_rtcp_buffer (sync, __buf, current_time, NULL)); \
  gst_buffer_unref (__buf);                                                                 \
} while (0)

GST_START_TEST (test_sync_context)
{
  KmsRtpSyncContext *ctx;
  GstClockTime ntp_ns_time_out, running_time_out;

  ctx = kms_rtp_sync_context_new (NULL);

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

  sync = kms_rtp_synchronizer_new (NULL, FALSE);

  process_rtcp (sync, 0x1, G_GUINT64_CONSTANT (0), 0, 0);

  process_rtp (sync, 100, 0x1, 96, 0, 0, 100, fail_if); /* Video frame 0 */

  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, -1, NULL));
  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 0, NULL));
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtp (sync, 200, 0x1, 96, 0, 0, 0, fail_unless);       /* Video frame 0 */

  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000, NULL));
  fail_if (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 8000, NULL));

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_one_stream)
{
  KmsRtpSynchronizer *sync;

  sync = kms_rtp_synchronizer_new (NULL, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtp (sync, 0, 0x1, 96, 0, 0, 0, fail_unless); /* Video frame 0 */
  process_rtp (sync, 100, 0x1, 96, 1, 0, 0, fail_unless);       /* Video frame 0 */

  process_rtcp (sync, 0x1, G_GUINT64_CONSTANT (0), 0, 0);

  process_rtp (sync, 200, 0x1, 96, 2, 0, 0, fail_unless);       /* Video frame 0 */
  process_rtp (sync, 200, 0x1, 96, 3, 90000, GST_SECOND, fail_unless);  /* Video frame 1 */

  /* A huge gap in the gst ts */
  process_rtp (sync, 50000, 0x1, 96, 4, 90000, GST_SECOND, fail_unless);        /* Video frame 1 */

  /* Packet 5 missed */
  process_rtp (sync, 50100, 0x1, 96, 6, 180000, 2 * GST_SECOND, fail_unless);   /* Video frame 2 */

  /* Packet 5 arrives */
  process_rtp (sync, 50100, 0x1, 96, 5, 90000, GST_SECOND, fail_unless);        /* Video frame 1 */

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_one_stream_rtptime_after_sr_rtptime)
{
  KmsRtpSynchronizer *sync;

  sync = kms_rtp_synchronizer_new (NULL, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtp (sync, 0, 0x1, 96, 0, 0, 0, fail_unless); /* Video frame 0 */
  process_rtp (sync, 100, 0x1, 96, 1, 0, 0, fail_unless);       /* Video frame 0 */

  process_rtcp (sync, 0x1, gst_util_uint64_scale (GST_SECOND, (1LL << 32),
          GST_SECOND), 90000, GST_SECOND);

  /* An RTP packet with rtptime<last_sr_rtptime arrives after last SR */
  process_rtp (sync, 200, 0x1, 96, 2, 0, 0, fail_unless);       /* Video frame 0 */

  process_rtp (sync, 200, 0x1, 96, 3, 90000, GST_SECOND, fail_unless);  /* Video frame 1 */

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_two_streams)
{
  KmsRtpSyncContext *ctx;
  KmsRtpSynchronizer *sync_audio, *sync_video;

  ctx = kms_rtp_sync_context_new (NULL);
  sync_audio = kms_rtp_synchronizer_new (ctx, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync_audio, 0, 8000,
          NULL));
  sync_video = kms_rtp_synchronizer_new (ctx, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync_video, 96,
          90000, NULL));
  g_object_unref (ctx);

  /* Audio RTCP SR */
  process_rtcp (sync_audio, 0x1, gst_util_uint64_scale (GST_SECOND, (1LL << 32),
          GST_SECOND), 8000, GST_SECOND);
  process_rtp (sync_audio, 0, 0x1, 0, 0, 0, 0, fail_unless);    /* Audio frame 0 */
  process_rtp (sync_audio, 0, 0x1, 0, 1, 8000, GST_SECOND, fail_unless);        /* Audio frame 1 */

  /* Video RTCP SR */
  process_rtcp (sync_video, 0x2, G_GUINT64_CONSTANT (0), 0, GST_SECOND);
  process_rtp (sync_video, 0, 0x1, 96, 0, 0, 0, fail_unless);   /* Video frame 0 */
  process_rtp (sync_video, 0, 0x1, 96, 1, 90000, GST_SECOND, fail_unless);      /* Video frame 1 */
  process_rtp (sync_video, 0, 0x1, 96, 2, 180000, 2 * GST_SECOND, fail_unless); /* Video frame 2 */

  g_object_unref (sync_audio);
  g_object_unref (sync_video);
}

GST_END_TEST;

GST_START_TEST (test_sync_avoid_negative_pts)
{
  KmsRtpSynchronizer *sync;

  sync = kms_rtp_synchronizer_new (NULL, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtcp (sync, 0x1, gst_util_uint64_scale (2 * GST_SECOND, (1LL << 32),
          GST_SECOND), 90000, GST_SECOND);
  process_rtcp (sync, 0x1, G_GUINT64_CONSTANT (0), 0, GST_SECOND);

  /* Force wrapped down with the same timestamp in RTP and RTCP packets */
  process_rtp (sync, 0, 0x1, 96, 0, 0, 0, fail_unless); /* Video frame 0 */

  process_rtcp (sync, 0x1, G_GUINT64_CONSTANT (0), 90000, GST_SECOND);

  /* Force wrapped down with RTP timestamp < RTCP timestamp */
  process_rtp (sync, 0, 0x1, 96, 0, 0, 0, fail_unless); /* Video frame 0 */

  /* Force wrapped down with RTP timestamp > RTCP timestamp, PTS resulting > 0 */
  process_rtp (sync, 0, 0x1, 96, 0, 270000, GST_SECOND, fail_unless);   /* Video frame 3 */

  /* Force wrapped down with RTP timestamp > RTCP timestamp, PTS resulting == 0 */
  process_rtp (sync, 0, 0x1, 96, 0, 180000, 0, fail_unless);    /* Video frame 2 */

  /* Force wrapped down with RTP timestamp > RTCP timestamp, PTS resulting < 0 */
  process_rtp (sync, 0, 0x1, 96, 0, 90000, 0, fail_unless);     /* Video frame 1 */

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_feeded_sorted_but_unsorted)
{
  KmsRtpSynchronizer *sync;

  sync = kms_rtp_synchronizer_new (NULL, TRUE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtp (sync, 100, 0x1, 96, 1, 90000, 100, fail_unless); /* Video frame 1 */
  process_rtp (sync, 0, 0x1, 96, 0, 0, 0, fail_if);     /* Video frame 0 */

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_sync_feeded_sorted_rtcp_beetween_same_ts)
{
  KmsRtpSynchronizer *sync_sorted, *sync_not_sorted;

  sync_sorted = kms_rtp_synchronizer_new (NULL, TRUE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync_sorted, 96,
          90000, NULL));

  sync_not_sorted = kms_rtp_synchronizer_new (NULL, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync_not_sorted, 96,
          90000, NULL));

  process_rtp (sync_sorted, 0, 0x1, 96, 0, 0, 0, fail_unless);  /* Video frame 0 */
  process_rtp (sync_not_sorted, 0, 0x1, 96, 0, 0, 0, fail_unless);      /* Video frame 0 */

  process_rtcp (sync_sorted, 0x1, gst_util_uint64_scale (GST_SECOND,
          (1LL << 32), GST_SECOND), 0, GST_SECOND);
  process_rtcp (sync_not_sorted, 0x1, gst_util_uint64_scale (GST_SECOND,
          (1LL << 32), GST_SECOND), 0, GST_SECOND);

  process_rtp (sync_sorted, 0, 0x1, 96, 1, 0, 0, fail_unless);  /* Video frame 0 */
  process_rtp (sync_not_sorted, 0, 0x1, 96, 1, 0, GST_SECOND, fail_unless);     /* Video frame 0 */

  g_object_unref (sync_sorted);
  g_object_unref (sync_not_sorted);
}

GST_END_TEST;

GST_START_TEST (test_interpolate)
{
  KmsRtpSynchronizer *sync;

  sync = kms_rtp_synchronizer_new (NULL, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtp (sync, 0, 0x1, 96, 0, 0, 0, fail_unless); /* Video frame 0 */
  process_rtp (sync, 0, 0x1, 96, 1, 0, 0, fail_unless); /* Video frame 0 */

  /* Packet 2 missed */
  process_rtp (sync, 0, 0x1, 96, 3, 180000, 2 * GST_SECOND, fail_unless);       /* Video frame 2 */

  /* Packet 2 arrives */
  process_rtp (sync, 0, 0x1, 96, 2, 90000, GST_SECOND, fail_unless);    /* Video frame 1 */

  process_rtp (sync, 50100, 0x1, 96, 4, 180000, 2 * GST_SECOND, fail_unless);   /* Video frame 2 */

  g_object_unref (sync);
}

GST_END_TEST;

GST_START_TEST (test_interpolate_avoid_negative_pts)
{
  KmsRtpSynchronizer *sync;

  sync = kms_rtp_synchronizer_new (NULL, FALSE);
  fail_unless (kms_rtp_synchronizer_add_clock_rate_for_pt (sync, 96, 90000,
          NULL));

  process_rtp (sync, 0, 0x1, 96, 1, 90000, 0, fail_unless);     /* Video frame 1 */

  /* Force PTS resulting < 0 */
  process_rtp (sync, 100, 0x1, 96, 0, 0, 0, fail_unless);       /* Video frame 0 */

  /* Packet 2 missed */
  process_rtp (sync, 50000, 0x1, 96, 3, 180000, GST_SECOND, fail_unless);       /* Video frame 2 */

  /* Packet 2 arrives */
  process_rtp (sync, 50100, 0x1, 96, 2, 90000, 0, fail_unless); /* Video frame 1 */

  process_rtp (sync, 50200, 0x1, 96, 4, 180000, GST_SECOND, fail_unless);       /* Video frame 2 */

  g_object_unref (sync);
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
  tcase_add_test (tc_chain, test_sync_avoid_negative_pts);

  tcase_add_test (tc_chain, test_sync_feeded_sorted_but_unsorted);
  tcase_add_test (tc_chain, test_sync_feeded_sorted_rtcp_beetween_same_ts);

  tcase_add_test (tc_chain, test_interpolate);
  tcase_add_test (tc_chain, test_interpolate_avoid_negative_pts);

  return s;
}

GST_CHECK_MAIN (rtpsync);
