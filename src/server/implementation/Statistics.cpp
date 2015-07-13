/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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

#include "Statistics.hpp"
#include "StatsType.hpp"
#include "RTCInboundRTPStreamStats.hpp"
#include "RTCOutboundRTPStreamStats.hpp"

#define GST_CAT_DEFAULT kurento_statistics
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoStatistics"

#define UUID_STR_SIZE 37 /* 36-byte string (plus tailing '\0') */
#define KMS_STATISTIC_FIELD_PREFIX_SESSION "session-"
#define KMS_STATISTIC_FIELD_PREFIX_SSRC "ssrc-"

/* Fixed point conversion macros */
#define FRIC        65536.                  /* 2^16 as a double */
#define FP2D(r)     ((double)(r) / FRIC)

namespace kurento
{

namespace stats
{

static std::shared_ptr<RTCInboundRTPStreamStats>
createRTCInboundRTPStreamStats (const GstStructure *stats)
{
  guint64 bytesReceived, packetsReceived;
  guint jitter, fractionLost, pliCount, firCount, remb;
  gint packetLost, clock_rate;
  float jitterSec;

  packetLost = jitter = fractionLost = pliCount = firCount = remb =
                                         clock_rate = 0;
  bytesReceived = packetsReceived = G_GUINT64_CONSTANT (0);
  jitterSec = 0.0;

  gst_structure_get (stats, "packets-received", G_TYPE_UINT64, &packetsReceived,
                     "octets-received", G_TYPE_UINT64, &bytesReceived,
                     "sent-rb-packetslost", G_TYPE_INT, &packetLost,
                     "sent-rb-fractionlost", G_TYPE_UINT, &fractionLost,
                     "clock-rate", G_TYPE_INT, &clock_rate,
                     "jitter", G_TYPE_UINT, &jitter, NULL);

  /* jitter is computed in timestamp units. Convert it to seconds */
  if (clock_rate > 0) {
    jitterSec = (float) jitter / clock_rate;
  }

  /* Next fields are only available with PLI and FIR statistics patches so */
  /* hey are prone to fail if these patches are not applied in Gstreamer */
  if (!gst_structure_get (stats, "sent-pli-count", G_TYPE_UINT, &pliCount,
                          "sent-fir-count", G_TYPE_UINT, &firCount, NULL) ) {
    GST_WARNING ("Current version of gstreamer has neither PLI nor FIR statistics patches applied.");
  }

  if (!gst_structure_get (stats, "remb", G_TYPE_UINT, &remb, NULL) ) {
    GST_TRACE ("No remb stats collected");
  }

  return std::make_shared <RTCInboundRTPStreamStats> ("",
         std::make_shared <StatsType> (StatsType::inboundrtp), 0.0, "",
         "", false, "", "", "", firCount, pliCount, 0, 0, remb,
         packetLost, (float) fractionLost, packetsReceived, bytesReceived,
         jitterSec);
}

static std::shared_ptr<RTCOutboundRTPStreamStats>
createRTCOutboundRTPStreamStats (const GstStructure *stats)
{
  guint64 bytesSent, packetsSent, bitRate;
  guint pliCount, firCount, remb, rtt, fractionLost;
  float roundTripTime;
  gint packetLost;

  bytesSent = packetsSent = bitRate = G_GUINT64_CONSTANT (0);
  pliCount = firCount = remb = rtt = 0;
  roundTripTime = 0.0;

  gst_structure_get (stats, "packets-sent", G_TYPE_UINT64, &packetsSent,
                     "octets-sent", G_TYPE_UINT64, &bytesSent, "bitrate",
                     G_TYPE_UINT64, &bitRate, "round-trip-time", G_TYPE_UINT,
                     &rtt, "outbound-fraction-lost", G_TYPE_UINT, &fractionLost,
                     "outbound-packet-lost", G_TYPE_INT, &packetLost, NULL);

  /* the round-trip time (in NTP Short Format, 16.16 fixed point) */
  roundTripTime = FP2D (rtt);

  /* Next fields are only available with PLI and FIR statistics patches so */
  /* hey are prone to fail if these patches are not applied in Gstreamer */
  if (!gst_structure_get (stats, "recv-pli-count", G_TYPE_UINT, &pliCount,
                          "recv-fir-count", G_TYPE_UINT, &firCount, NULL) ) {
    GST_WARNING ("Current version of gstreamer has neither PLI nor FIR statistics patches applied.");
  }

  if (!gst_structure_get (stats, "remb", G_TYPE_UINT, &remb, NULL) ) {
    GST_TRACE ("No remb stats collected");
  }

  return std::make_shared <RTCOutboundRTPStreamStats> ("",
         std::make_shared <StatsType> (StatsType::outboundrtp), 0.0, "",
         "", false, "", "", "", firCount, pliCount, 0, 0, remb, packetLost,
         (float) fractionLost, packetsSent, bytesSent, (float) bitRate,
         roundTripTime);
}

static std::shared_ptr<RTCRTPStreamStats>
createRTCRTPStreamStats (guint nackSent, guint nackRecv,
                         const GstStructure *stats)
{
  std::shared_ptr<RTCRTPStreamStats> rtcStats;
  gboolean isInternal;
  gchar *ssrcStr, *id;
  uint ssrc, nackCount;

  gst_structure_get (stats, "ssrc", G_TYPE_UINT, &ssrc, "internal",
                     G_TYPE_BOOLEAN, &isInternal, "id", G_TYPE_STRING, &id, NULL);

  ssrcStr = g_strdup_printf ("%u", ssrc);

  if (isInternal) {
    /* Local SSRC */
    rtcStats = createRTCOutboundRTPStreamStats (stats);
    nackCount = nackRecv;
  } else {
    /* Remote SSRC */
    rtcStats = createRTCInboundRTPStreamStats (stats);
    nackCount = nackSent;
  }

  rtcStats->setNackCount (nackCount);
  rtcStats->setSsrc (ssrcStr);
  rtcStats->setId (id);

  g_free (ssrcStr);
  g_free (id);

  return rtcStats;
}

static void
collectRTCRTPStreamStats (std::map <std::string, std::shared_ptr<Stats>>
                          &statsReport, double timestamp, const GstStructure *stats)
{
  guint nackSent, nackRecv;
  gint i, n;

  nackSent = nackRecv = 0;

  gst_structure_get (stats, "sent-nack-count", G_TYPE_UINT, &nackSent,
                     "recv-nack-count", G_TYPE_UINT, &nackRecv, NULL);

  n = gst_structure_n_fields (stats);

  for (i = 0; i < n; i++) {
    std::shared_ptr<RTCStats> rtcStats;
    const GValue *value;
    const gchar *name;

    name = gst_structure_nth_field_name (stats, i);

    if (!g_str_has_prefix (name, KMS_STATISTIC_FIELD_PREFIX_SSRC) ) {
      continue;
    }

    value = gst_structure_get_value (stats, name);

    if (!GST_VALUE_HOLDS_STRUCTURE (value) ) {
      gchar *str_val;

      str_val = g_strdup_value_contents (value);
      GST_WARNING ("Unexpected field type (%s) = %s", name, str_val);
      g_free (str_val);

      continue;
    }

    rtcStats = createRTCRTPStreamStats (nackSent, nackRecv,
                                        gst_value_get_structure (value) );

    rtcStats->setTimestamp (timestamp);

    statsReport[rtcStats->getId ()] = rtcStats;
  }
}

std::map <std::string, std::shared_ptr<Stats>> createStatsReport (
      double timestamp, const GstStructure *stats)
{
  std::map <std::string, std::shared_ptr<Stats>> statsReport;
  gint i, n;

  n = gst_structure_n_fields (stats);

  for (i = 0; i < n; i++) {
    std::shared_ptr<RTCStats> rtcStats;
    const GValue *value;
    const gchar *name;

    name = gst_structure_nth_field_name (stats, i);

    if (!g_str_has_prefix (name, KMS_STATISTIC_FIELD_PREFIX_SESSION) ) {
      GST_DEBUG ("Ignoring field %s", name);
      continue;
    }

    value = gst_structure_get_value (stats, name);

    if (!GST_VALUE_HOLDS_STRUCTURE (value) ) {
      gchar *str_val;

      str_val = g_strdup_value_contents (value);
      GST_WARNING ("Unexpected field type (%s) = %s", name, str_val);
      g_free (str_val);

      continue;
    }

    collectRTCRTPStreamStats (statsReport, timestamp,
                              gst_value_get_structure (value) );
  }

  return statsReport;
}

} /* statistics */

} /* kurento */

static void init_debug (void) __attribute__ ( (constructor) );

static void
init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}
