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

#include "kmsremb.h"
#include "kmsrtcp.h"
#include "constants.h"

#define GST_CAT_DEFAULT kmsutils
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "kmsremb"

#define REMB_MIN 30000          /* bps */
#define REMB_MAX 2000000        /* bps */

#define KMS_REMB_REMOTE "kms-remb-remote"

/* KmsRembLocal begin */

#define KMS_REMB_LOCAL "kms-remb-local"

#define DEFAULT_REMB_PACKETS_RECV_INTERVAL_TOP 100
#define DEFAULT_REMB_EXPONENTIAL_FACTOR 0.04
#define DEFAULT_REMB_LINEAL_FACTOR_MIN 50       /* bps */
#define DEFAULT_REMB_LINEAL_FACTOR_GRADE ((60 * RTCP_MIN_INTERVAL)/ 1000)       /* Reach last top bitrate in 60secs aprox. */
#define DEFAULT_REMB_DECREMENT_FACTOR 0.5
#define DEFAULT_REMB_THRESHOLD_FACTOR 0.8
#define DEFAULT_REMB_UP_LOSSES 12       /* 4% losses */

static void
kms_remb_base_destroy (KmsRembBase * rb)
{
  g_signal_handler_disconnect (rb->rtpsess, rb->signal_id);
  rb->signal_id = 0;
  g_object_set_data (rb->rtpsess, KMS_REMB_LOCAL, NULL);
  g_object_set_data (rb->rtpsess, KMS_REMB_REMOTE, NULL);
  g_clear_object (&rb->rtpsess);
  g_rec_mutex_clear (&rb->mutex);
  g_hash_table_unref (rb->remb_stats);
}

static void
kms_remb_base_create (KmsRembBase * rb, GObject * rtpsess)
{
  rb->rtpsess = g_object_ref (rtpsess);
  g_rec_mutex_init (&rb->mutex);
  rb->remb_stats = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      (GDestroyNotify) kms_utils_destroy_guint);
}

static void
kms_remb_base_update_stats (KmsRembBase * rb, guint ssrc, guint bitrate)
{
  guint *value;

  KMS_REMB_BASE_LOCK (rb);

  if (g_hash_table_contains (rb->remb_stats, GUINT_TO_POINTER (ssrc))) {
    value =
        (guint *) g_hash_table_lookup (rb->remb_stats, GUINT_TO_POINTER (ssrc));
  } else {
    value = g_slice_new0 (guint);
    g_hash_table_insert (rb->remb_stats, GUINT_TO_POINTER (ssrc), value);
  }

  *value = bitrate;

  KMS_REMB_BASE_UNLOCK (rb);
}

typedef struct _KmsRlRemoteSession
{
  GObject *rtpsess;
  guint ssrc;

  guint64 last_packets_received_expected;
} KmsRlRemoteSession;

static KmsRlRemoteSession *
kms_rl_remote_session_create (GObject * rtpsess, guint ssrc)
{
  KmsRlRemoteSession *rlrs = g_slice_new0 (KmsRlRemoteSession);

  rlrs->rtpsess = g_object_ref (rtpsess);
  rlrs->ssrc = ssrc;

  return rlrs;
}

static void
kms_rl_remote_session_create_destroy (KmsRlRemoteSession * rlrs)
{
  g_clear_object (&rlrs->rtpsess);
  g_slice_free (KmsRlRemoteSession, rlrs);
}

typedef struct _GetRtpSessionsInfo
{
  guint count;
  guint64 bitrate;
  guint fraction_lost_accumulative;     /* the sum of all sessions, it should be normalized */
  guint64 packets_received_expected_interval_accumulative;
  guint64 octets_received;
  guint64 packets_received;
} GetRtpSessionsInfo;

static void
get_sessions_info (KmsRlRemoteSession * rlrs, GetRtpSessionsInfo * data)
{
  GValueArray *arr = NULL;
  GValue *val;
  guint i;

  g_object_get (rlrs->rtpsess, "sources", &arr, NULL);
  if (arr == NULL) {
    GST_WARNING_OBJECT (rlrs->rtpsess, "Sources array not found");
    return;
  }

  for (i = 0; i < arr->n_values; i++) {
    GObject *source;
    guint ssrc;

    val = g_value_array_get_nth (arr, i);
    source = g_value_get_object (val);
    g_object_get (source, "ssrc", &ssrc, NULL);
    GST_TRACE_OBJECT (source, "source ssrc: %u", ssrc);

    if (ssrc == rlrs->ssrc) {
      GstStructure *s;
      guint64 bitrate, octets_received, packets_received;
      gint packets_lost;
      guint fraction_lost;
      guint64 packets_received_expected, packets_received_expected_interval;

      g_object_get (source, "stats", &s, NULL);
      GST_TRACE_OBJECT (source, "stats: %" GST_PTR_FORMAT, s);

      if (!gst_structure_get_uint64 (s, "bitrate", &bitrate) ||
          !gst_structure_get_uint64 (s, "octets-received", &octets_received) ||
          !gst_structure_get_uint (s, "sent-rb-fractionlost", &fraction_lost) ||
          !gst_structure_get_uint64 (s, "packets-received", &packets_received)
          || !gst_structure_get_int (s, "packets-lost", &packets_lost)) {
        gst_structure_free (s);
        break;
      }
      gst_structure_free (s);

      packets_received_expected = packets_received + packets_lost;
      packets_received_expected_interval =
          packets_received_expected - rlrs->last_packets_received_expected;
      rlrs->last_packets_received_expected = packets_received_expected;

      data->bitrate += bitrate;
      data->fraction_lost_accumulative +=
          fraction_lost * packets_received_expected_interval;
      data->packets_received_expected_interval_accumulative +=
          packets_received_expected_interval;
      data->octets_received += octets_received;
      data->packets_received += packets_received;
      data->count++;

      GST_TRACE_OBJECT (source,
          "packets_received: %" G_GUINT64_FORMAT ", packets_lost: %"
          G_GUINT32_FORMAT ", packets_received_expected_interval: %"
          G_GUINT64_FORMAT
          ", packets_received_expected_interval_accumulative: %"
          G_GUINT64_FORMAT, packets_received, packets_lost,
          packets_received_expected_interval,
          data->packets_received_expected_interval_accumulative);

      break;
    }
  }

  g_value_array_free (arr);
}

static gboolean
get_video_recv_info (KmsRembLocal * rl,
    guint64 * bitrate, guint * fraction_lost, guint64 * packets_rcv_interval)
{
  GetRtpSessionsInfo data;
  GstClockTime current_time;

  if (!KMS_REMB_BASE (rl)->rtpsess) {
    GST_WARNING ("Session object does not exist");
    return FALSE;
  }

  data.count = 0;
  data.bitrate = 0;
  data.fraction_lost_accumulative = 0;
  data.packets_received_expected_interval_accumulative = 0;
  data.octets_received = 0;
  data.packets_received = 0;
  g_slist_foreach (rl->remote_sessions, (GFunc) get_sessions_info, &data);

  if (data.count == 0
      || data.packets_received_expected_interval_accumulative == 0) {
    GST_DEBUG ("Any data updated");
    return FALSE;
  }

  current_time = kms_utils_get_time_nsecs ();

  /* Normalize fraction_lost */
  *fraction_lost =
      data.fraction_lost_accumulative /
      data.packets_received_expected_interval_accumulative;

  *bitrate = data.bitrate;
  if (rl->last_time != 0) {
    GstClockTime elapsed = current_time - rl->last_time;
    guint64 bytes_handled = data.octets_received - rl->last_octets_received;

    *bitrate = gst_util_uint64_scale (bytes_handled, 8 * GST_SECOND, elapsed);
    GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess,
        "Elapsed %" G_GUINT64_FORMAT " bytes %" G_GUINT64_FORMAT ", rate %"
        G_GUINT64_FORMAT, elapsed, bytes_handled, *bitrate);
  }

  rl->last_time = current_time;
  rl->last_octets_received = data.octets_received;

  *packets_rcv_interval = data.packets_received - rl->last_packets_received;
  rl->last_packets_received = data.packets_received;

  return TRUE;
}

static gboolean
kms_remb_local_update (KmsRembLocal * rl)
{
  guint64 bitrate, packets_rcv_interval;
  guint fraction_lost, packets_rcv_interval_top;

  if (!get_video_recv_info (rl, &bitrate, &fraction_lost,
          &packets_rcv_interval)) {
    return FALSE;
  }

  if (!rl->probed) {
    if (bitrate == 0) {
      return FALSE;
    }

    rl->remb = bitrate;
    rl->probed = TRUE;
  }

  packets_rcv_interval_top =
      MAX (rl->packets_recv_interval_top, packets_rcv_interval);
  rl->fraction_lost_record =
      (rl->fraction_lost_record * (packets_rcv_interval_top -
          packets_rcv_interval) +
      fraction_lost * packets_rcv_interval) / packets_rcv_interval_top;
  rl->max_br = MAX (rl->max_br, bitrate);

  if (rl->avg_br == 0) {
    rl->avg_br = bitrate;
  } else {
    rl->avg_br = (rl->avg_br * 7 + bitrate) / 8;
  }

  GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess,
      "packets_rcv_interval: %" G_GUINT64_FORMAT ", fraction_lost: %"
      G_GUINT32_FORMAT ", fraction_lost_record: %" G_GUINT64_FORMAT,
      packets_rcv_interval, fraction_lost, rl->fraction_lost_record);

  if (rl->fraction_lost_record == 0) {
    gint remb_base, remb_new;

    remb_base = MIN (rl->remb, rl->max_br);

    if (remb_base < rl->threshold) {
      GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess, "A.1) Exponential (%f)",
          rl->exponential_factor);
      remb_new = remb_base * (1 + rl->exponential_factor);
    } else {
      GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess,
          "A.2) Lineal (%" G_GUINT32_FORMAT ")", rl->lineal_factor);
      remb_new = remb_base + rl->lineal_factor;
    }

    rl->remb = MAX (rl->remb, remb_new);
  } else {
    gint remb_base, lineal_factor_new;

    remb_base = MAX (rl->remb, rl->avg_br);
    rl->threshold = remb_base * rl->threshold_factor;
    lineal_factor_new = (remb_base - rl->threshold) / rl->lineal_factor_grade;
    rl->lineal_factor = MAX (rl->lineal_factor_min, lineal_factor_new);

    if (rl->fraction_lost_record < rl->up_losses) {
      GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess, "B) Assumable losses");

      rl->remb = MIN (rl->remb, rl->max_br);
    } else {
      GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess, "C) Too losses");

      rl->remb = remb_base * rl->decrement_factor;
      rl->fraction_lost_record = 0;
      rl->max_br = 0;
      rl->avg_br = 0;
    }
  }

  if (rl->max_bw > 0) {
    rl->remb = MIN (rl->remb, rl->max_bw * 1000);
  }

  GST_TRACE_OBJECT (KMS_REMB_BASE (rl)->rtpsess,
      "REMB: %" G_GUINT32_FORMAT ", TH: %" G_GUINT32_FORMAT
      ", fraction_lost: %d, fraction_lost_record: %" G_GUINT64_FORMAT
      ", bitrate: %" G_GUINT64_FORMAT "," " max_br: %" G_GUINT32_FORMAT
      ", avg_br: %" G_GUINT32_FORMAT, rl->remb, rl->threshold, fraction_lost,
      rl->fraction_lost_record, bitrate, rl->max_br, rl->avg_br);

  return TRUE;
}

typedef struct _AddSsrcsData
{
  KmsRembLocal *rl;
  KmsRTCPPSFBAFBREMBPacket *remb_packet;
} AddSsrcsData;

static void
add_ssrcs (KmsRlRemoteSession * rlrs, AddSsrcsData * data)
{
  KmsRembBase *rb = KMS_REMB_BASE (data->rl);

  data->remb_packet->ssrcs[data->remb_packet->n_ssrcs] = rlrs->ssrc;
  data->remb_packet->n_ssrcs++;

  GST_TRACE_OBJECT (rb->rtpsess, "Sending REMB (bitrate: %" G_GUINT32_FORMAT
      ", ssrc: %" G_GUINT32_FORMAT ")", data->remb_packet->bitrate, rlrs->ssrc);

  kms_remb_base_update_stats (rb, rlrs->ssrc, data->remb_packet->bitrate);
}

static void
on_sending_rtcp (GObject * sess, GstBuffer * buffer, gboolean is_early,
    gboolean * do_not_supress)
{
  KmsRembLocal *rl;
  GstClockTime current_time, elapsed;
  KmsRTCPPSFBAFBREMBPacket remb_packet;
  GstRTCPBuffer rtcp = { NULL, };
  GstRTCPPacket packet;
  guint packet_ssrc;
  AddSsrcsData data;

  rl = g_object_get_data (sess, KMS_REMB_LOCAL);

  if (!rl) {
    GST_WARNING ("Invalid RembLocal");
    return;
  }

  current_time = kms_utils_get_time_nsecs ();
  elapsed = current_time - rl->last_sent_time;
  if (rl->last_sent_time != 0 && (elapsed < REMB_MAX_INTERVAL * GST_MSECOND)) {
    GST_TRACE_OBJECT (sess, "Not sending, interval < %u ms", REMB_MAX_INTERVAL);
    return;
  }

  if (!gst_rtcp_buffer_map (buffer, GST_MAP_READWRITE, &rtcp)) {
    GST_WARNING_OBJECT (sess, "Cannot map buffer to RTCP");
    return;
  }

  if (!gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_PSFB, &packet)) {
    GST_WARNING_OBJECT (sess, "Cannot add RTCP packet");
    goto end;
  }

  if (!kms_remb_local_update (rl)) {
    goto end;
  }

  remb_packet.bitrate = rl->remb;
  if (rl->event_manager != NULL) {
    guint remb_local_max;

    remb_local_max = kms_utils_remb_event_manager_get_min (rl->event_manager);
    if (remb_local_max > 0) {
      GST_TRACE_OBJECT (sess, "REMB local max: %" G_GUINT32_FORMAT,
          remb_local_max);
      remb_packet.bitrate = MIN (remb_local_max, rl->remb);
    }
  }

  if (rl->min_bw > 0) {
    remb_packet.bitrate = MAX (remb_packet.bitrate, rl->min_bw * 1000);
  } else {
    remb_packet.bitrate = MAX (remb_packet.bitrate, REMB_MIN);
  }

  remb_packet.n_ssrcs = 0;
  data.rl = rl;
  data.remb_packet = &remb_packet;
  g_slist_foreach (rl->remote_sessions, (GFunc) add_ssrcs, &data);

  g_object_get (sess, "internal-ssrc", &packet_ssrc, NULL);
  if (!kms_rtcp_psfb_afb_remb_marshall_packet (&packet, &remb_packet,
          packet_ssrc)) {
    gst_rtcp_packet_remove (&packet);
  }

  rl->last_sent_time = current_time;

end:
  gst_rtcp_buffer_unmap (&rtcp);
}

void
kms_remb_local_destroy (KmsRembLocal * rl)
{
  if (rl == NULL) {
    return;
  }

  if (rl->event_manager != NULL) {
    kms_utils_remb_event_manager_destroy (rl->event_manager);
  }

  g_slist_free_full (rl->remote_sessions,
      (GDestroyNotify) kms_rl_remote_session_create_destroy);
  kms_remb_base_destroy (KMS_REMB_BASE (rl));

  g_slice_free (KmsRembLocal, rl);
}

KmsRembLocal *
kms_remb_local_create (GObject * rtpsess, guint min_bw, guint max_bw)
{
  KmsRembLocal *rl = g_slice_new0 (KmsRembLocal);

  g_object_set_data (rtpsess, KMS_REMB_LOCAL, rl);
  rl->base.signal_id = g_signal_connect (rtpsess, "on-sending-rtcp",
      G_CALLBACK (on_sending_rtcp), NULL);

  kms_remb_base_create (KMS_REMB_BASE (rl), rtpsess);

  rl->min_bw = min_bw;
  rl->max_bw = max_bw;

  rl->probed = FALSE;
  rl->remb = REMB_MAX;
  rl->threshold = REMB_MAX;
  rl->lineal_factor = DEFAULT_REMB_LINEAL_FACTOR_MIN;

  rl->packets_recv_interval_top = DEFAULT_REMB_PACKETS_RECV_INTERVAL_TOP;
  rl->exponential_factor = DEFAULT_REMB_EXPONENTIAL_FACTOR;
  rl->lineal_factor_min = DEFAULT_REMB_LINEAL_FACTOR_MIN;
  rl->lineal_factor_grade = DEFAULT_REMB_LINEAL_FACTOR_GRADE;
  rl->decrement_factor = DEFAULT_REMB_DECREMENT_FACTOR;
  rl->threshold_factor = DEFAULT_REMB_THRESHOLD_FACTOR;
  rl->up_losses = DEFAULT_REMB_UP_LOSSES;

  return rl;
}

void
kms_remb_local_add_remote_session (KmsRembLocal * rl, GObject * rtpsess,
    guint ssrc)
{
  KmsRlRemoteSession *rlrs = kms_rl_remote_session_create (rtpsess, ssrc);

  rl->remote_sessions = g_slist_append (rl->remote_sessions, rlrs);
}

void
kms_remb_local_set_params (KmsRembLocal * rl, GstStructure * params)
{
  gfloat auxf;
  gint auxi;
  gboolean is_set;

  is_set =
      gst_structure_get (params, "packets-recv-interval-top", G_TYPE_INT,
      &auxi, NULL);
  if (is_set) {
    if (auxi <= 0) {
      GST_WARNING
          ("'packets-recv-interval-top' must be greater than 0. Setting to 1.");
      auxi = 1;
    }

    rl->packets_recv_interval_top = auxi;
  }

  is_set =
      gst_structure_get (params, "exponential-factor", G_TYPE_FLOAT,
      &auxf, NULL);
  if (is_set) {
    rl->exponential_factor = auxf;
  }

  is_set =
      gst_structure_get (params, "lineal-factor-min", G_TYPE_INT, &auxi, NULL);
  if (is_set) {
    rl->lineal_factor_min = auxi;
  }

  is_set =
      gst_structure_get (params, "lineal-factor-grade", G_TYPE_INT,
      &auxi, NULL);
  if (is_set) {
    rl->lineal_factor_grade = auxi;
  }

  is_set =
      gst_structure_get (params, "decrement-factor", G_TYPE_FLOAT, &auxf, NULL);
  if (is_set) {
    rl->decrement_factor = auxf;
  }

  is_set =
      gst_structure_get (params, "threshold-factor", G_TYPE_FLOAT, &auxf, NULL);
  if (is_set) {
    rl->threshold_factor = auxf;
  }

  is_set = gst_structure_get (params, "up-losses", G_TYPE_INT, &auxi, NULL);
  if (is_set) {
    rl->up_losses = auxi;
  }
}

void
kms_remb_local_get_params (KmsRembLocal * rl, GstStructure ** params)
{
  gst_structure_set (*params,
      "packets-recv-interval-top", G_TYPE_INT, rl->packets_recv_interval_top,
      "exponential-factor", G_TYPE_FLOAT, rl->exponential_factor,
      "lineal-factor-min", G_TYPE_INT, rl->lineal_factor_min,
      "lineal-factor-grade", G_TYPE_FLOAT, rl->lineal_factor_grade,
      "decrement-factor", G_TYPE_FLOAT, rl->decrement_factor,
      "threshold-factor", G_TYPE_FLOAT, rl->threshold_factor,
      "up-losses", G_TYPE_INT, rl->up_losses, NULL);
}

/* KmsRembLocal end */

/* KmsRembRemote begin */

#define DEFAULT_REMB_ON_CONNECT 300000  /* bps */

static void
send_remb_event (KmsRembRemote * rm, guint bitrate, guint ssrc)
{
  GstEvent *event;
  guint br, min = 0, max = 0;

  /* TODO: use g_atomic */
  if (rm->pad_event == NULL) {
    return;
  }

  br = bitrate;

  if (rm->min_bw > 0) {
    min = rm->min_bw * 1000;
    br = MAX (br, min);
  }

  if (rm->max_bw > 0) {
    max = rm->max_bw * 1000;
    br = MIN (br, max);
  }

  GST_TRACE_OBJECT (KMS_REMB_BASE (rm)->rtpsess,
      "bitrate: %" G_GUINT32_FORMAT ", ssrc: %" G_GUINT32_FORMAT
      ", range [%" G_GUINT32_FORMAT ", %" G_GUINT32_FORMAT
      "], event bitrate: %" G_GUINT32_FORMAT, bitrate, ssrc, min, max, br);

  event = kms_utils_remb_event_upstream_new (br, ssrc);
  gst_pad_push_event (rm->pad_event, event);
}

static GstPadProbeReturn
send_remb_event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  KmsRembRemote *rm = user_data;
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS) {
    return GST_PAD_PROBE_OK;
  };

  send_remb_event (rm, rm->remb_on_connect, rm->local_ssrc);

  return GST_PAD_PROBE_REMOVE;
}

static void
kms_remb_remote_update (KmsRembRemote * rm,
    KmsRTCPPSFBAFBREMBPacket * remb_packet)
{
  guint32 br_send;

  if (remb_packet->n_ssrcs == 0) {
    GST_WARNING_OBJECT (KMS_REMB_BASE (rm)->rtpsess,
        "REMB packet without any SSRC");
    return;
  } else if (remb_packet->n_ssrcs > 1) {
    GST_WARNING_OBJECT (KMS_REMB_BASE (rm)->rtpsess,
        "REMB packet with %" G_GUINT32_FORMAT " SSRCs."
        " A inconsistent management could take place", remb_packet->n_ssrcs);
  }

  br_send = remb_packet->bitrate;
  if (!rm->probed) {
    if ((remb_packet->bitrate < rm->remb_on_connect)
        && (remb_packet->bitrate >= rm->remb)) {
      GST_DEBUG_OBJECT (KMS_REMB_BASE (rm)->rtpsess,
          "Not probed: sending remb_on_connect value");
      br_send = rm->remb_on_connect;
      rm->remb = remb_packet->bitrate;
    } else {
      rm->probed = TRUE;
    }
  }

  send_remb_event (rm, br_send, remb_packet->ssrcs[0]);
  rm->remb = remb_packet->bitrate;
}

static void
kms_remb_remote_update_target_ssrcs_stats (KmsRembRemote * rm,
    KmsRTCPPSFBAFBREMBPacket * remb_packet)
{
  guint i;

  for (i = 0; i < remb_packet->n_ssrcs; i++) {
    kms_remb_base_update_stats (KMS_REMB_BASE (rm), remb_packet->ssrcs[i],
        remb_packet->bitrate);
  }
}

static void
process_psfb_afb (GObject * sess, guint ssrc, GstBuffer * fci_buffer)
{
  KmsRembRemote *rm;
  KmsRTCPPSFBAFBBuffer afb_buffer = { NULL, };
  KmsRTCPPSFBAFBPacket afb_packet;
  KmsRTCPPSFBAFBREMBPacket remb_packet;
  KmsRTCPPSFBAFBType type;

  if (!G_IS_OBJECT (sess)) {
    GST_WARNING ("Invalid session object");
    return;
  }

  rm = g_object_get_data (sess, KMS_REMB_REMOTE);

  if (!rm) {
    GST_WARNING ("Invalid RembRemote");
    return;
  }

  if (!kms_rtcp_psfb_afb_buffer_map (fci_buffer, GST_MAP_READ, &afb_buffer)) {
    GST_WARNING_OBJECT (fci_buffer, "Buffer cannot be mapped");
    return;
  }

  if (!kms_rtcp_psfb_afb_get_packet (&afb_buffer, &afb_packet)) {
    GST_WARNING_OBJECT (fci_buffer, "Cannot get RTCP PSFB AFB packet");
    goto end;
  }

  type = kms_rtcp_psfb_afb_packet_get_type (&afb_packet);
  switch (type) {
    case KMS_RTCP_PSFB_AFB_TYPE_REMB:
      kms_rtcp_psfb_afb_remb_get_packet (&afb_packet, &remb_packet);
      kms_remb_remote_update (rm, &remb_packet);
      kms_remb_remote_update_target_ssrcs_stats (rm, &remb_packet);
      break;
    default:
      break;
  }

end:
  kms_rtcp_psfb_afb_buffer_unmap (&afb_buffer);
}

static void
on_feedback_rtcp (GObject * sess, guint type, guint fbtype,
    guint sender_ssrc, guint media_ssrc, GstBuffer * fci)
{
  switch (type) {
    case GST_RTCP_TYPE_RTPFB:
      break;
    case GST_RTCP_TYPE_PSFB:
      switch (fbtype) {
        case GST_RTCP_PSFB_TYPE_AFB:
          process_psfb_afb (sess, sender_ssrc, fci);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void
kms_remb_remote_destroy (KmsRembRemote * rm)
{
  if (rm == NULL) {
    return;
  }

  if (rm->pad_event != NULL) {
    g_object_unref (rm->pad_event);
  }

  kms_remb_base_destroy (KMS_REMB_BASE (rm));

  g_slice_free (KmsRembRemote, rm);
}

KmsRembRemote *
kms_remb_remote_create (GObject * rtpsess, guint local_ssrc,
    guint min_bw, guint max_bw, GstPad * pad)
{
  KmsRembRemote *rm = g_slice_new0 (KmsRembRemote);

  g_object_set_data (rtpsess, KMS_REMB_REMOTE, rm);
  rm->base.signal_id = g_signal_connect (rtpsess, "on-feedback-rtcp",
      G_CALLBACK (on_feedback_rtcp), NULL);

  kms_remb_base_create (KMS_REMB_BASE (rm), rtpsess);

  rm->local_ssrc = local_ssrc;
  rm->min_bw = min_bw;
  rm->max_bw = max_bw;

  rm->remb_on_connect = DEFAULT_REMB_ON_CONNECT;

  rm->pad_event = g_object_ref (pad);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      send_remb_event_probe, rm, NULL);

  return rm;
}

void
kms_remb_remote_set_params (KmsRembRemote * rm, GstStructure * params)
{
  gint auxi;
  gboolean is_set;

  is_set =
      gst_structure_get (params, "remb-on-connect", G_TYPE_INT, &auxi, NULL);
  if (is_set) {
    rm->remb_on_connect = auxi;
  }
}

void
kms_remb_remote_get_params (KmsRembRemote * rm, GstStructure ** params)
{
  gst_structure_set (*params,
      "remb-on-connect", G_TYPE_INT, rm->remb_on_connect, NULL);
}

/* KmsRembRemote end */

static void init_debug (void) __attribute__ ((constructor));

static void
init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}
