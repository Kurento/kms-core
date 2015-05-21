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
#include "RTCStatsType.hpp"
#include "RTCInboundRTPStreamStats.hpp"
#include "RTCOutboundRTPStreamStats.hpp"

#define GST_CAT_DEFAULT kurento_statistics
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoStatistics"

#define UUID_STR_SIZE 37 /* 36-byte string (plus tailing '\0') */
#define KMS_STATISTIC_FIELD_PREFIX_SESSION "session-"
#define KMS_STATISTIC_FIELD_PREFIX_SSRC "ssrc-"

namespace kurento
{

namespace stats
{

static std::shared_ptr<RTCInboundRTPStreamStats>
createRTCInboundRTPStreamStats (const GstStructure *stats)
{
  guint64 bytesReceived, packetsReceived;
  guint jitter, fractionLost, pliCount, firCount, remb;
  gint packetLost;

  packetLost = jitter = fractionLost = pliCount = firCount = remb = 0;
  bytesReceived = packetsReceived = G_GUINT64_CONSTANT (0);

  gst_structure_get (stats, "packets-received", G_TYPE_UINT64, &packetsReceived,
                     "octets-received", G_TYPE_UINT64, &bytesReceived,
                     "rb-packetslost", G_TYPE_INT, &packetLost,
                     "rb-fractionlost", G_TYPE_UINT, &fractionLost,
                     "rb-jitter", G_TYPE_UINT, &jitter, NULL);

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
         std::make_shared <RTCStatsType> (RTCStatsType::inboundrtp), 0.0, "",
         "", false, "", "", "", firCount, pliCount, 0, 0, remb,
         packetsReceived, bytesReceived, packetLost, (float) jitter,
         (float) fractionLost);
}

static std::shared_ptr<RTCOutboundRTPStreamStats>
createRTCOutboundRTPStreamStats (const GstStructure *stats)
{
  guint64 bytesSent, packetsSent, bitRate, roundTripTime;
  guint pliCount, firCount, remb;

  bytesSent = packetsSent = bitRate = roundTripTime = G_GUINT64_CONSTANT (0);
  pliCount = firCount = remb = 0;

  gst_structure_get (stats, "packets-sent", G_TYPE_UINT64, &packetsSent,
                     "octets-sent", G_TYPE_UINT64, &bytesSent, "bitrate",
                     G_TYPE_UINT64, &bitRate, "rb-round-trip", G_TYPE_UINT64,
                     &roundTripTime, NULL);

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
         std::make_shared <RTCStatsType> (RTCStatsType::outboundrtp), 0.0, "",
         "", false, "", "", "", firCount, pliCount, 0, 0, remb,
         packetsSent, bytesSent, (float) bitRate, (float) roundTripTime);
}

static std::shared_ptr<RTCRTPStreamStats>
createRTCRTPStreamStats (uint nackCount, const GstStructure *stats)
{
  std::shared_ptr<RTCRTPStreamStats> rtcStats;
  gboolean isInternal;
  gchar *ssrcStr, *id;
  uint ssrc;

  gst_structure_get (stats, "ssrc", G_TYPE_UINT, &ssrc, "internal",
                     G_TYPE_BOOLEAN, &isInternal, "id", G_TYPE_STRING, &id, NULL);

  ssrcStr = g_strdup_printf ("%u", ssrc);

  if (isInternal) {
    /* Local SSRC */
    rtcStats = createRTCOutboundRTPStreamStats (stats);
  } else {
    /* Remote SSRC */
    rtcStats = createRTCInboundRTPStreamStats (stats);
  }

  rtcStats->setNackCount (nackCount);
  rtcStats->setSsrc (ssrcStr);
  rtcStats->setId (id);

  g_free (ssrcStr);
  g_free (id);

  return rtcStats;
}

static void
collectRTCRTPStreamStats (std::map <std::string, std::shared_ptr<RTCStats>>
                          &rtcStatsReport, double timestamp, const GstStructure *stats)
{
  guint nackCount = 0;
  gint i, n;

  gst_structure_get (stats, "sent-nack-count", G_TYPE_UINT, &nackCount, NULL);

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

    rtcStats = createRTCRTPStreamStats (nackCount,
                                        gst_value_get_structure (value) );

    rtcStats->setTimestamp (timestamp);

    rtcStatsReport[rtcStats->getId ()] = rtcStats;
  }
}

std::map <std::string, std::shared_ptr<RTCStats>> createRTCStatsReport (
      double timestamp, const GstStructure *stats)
{
  std::map <std::string, std::shared_ptr<RTCStats>> rtcStatsReport;
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

    collectRTCRTPStreamStats (rtcStatsReport, timestamp,
                              gst_value_get_structure (value) );
  }

  return rtcStatsReport;
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
