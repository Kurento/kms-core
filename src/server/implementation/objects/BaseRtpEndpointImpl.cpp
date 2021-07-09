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

#include <gst/gst.h>
#include "BaseRtpEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <MediaState.hpp>
#include <ConnectionState.hpp>
#include <ctime>
#include <SignalHandler.hpp>
#include <MediaType.hpp>

#include "RembParams.hpp"

#include "StatsType.hpp"
#include "RTCInboundRTPStreamStats.hpp"
#include "RTCOutboundRTPStreamStats.hpp"
#include "EndpointStats.hpp"
#include "kmsstats.h"
#include "kmsutils.h"

#define GST_CAT_DEFAULT kurento_base_rtp_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoBaseRtpEndpointImpl"

#define KMS_MEDIA_DISCONNECTED 0
#define KMS_MEDIA_CONNECTED 1
#define KMS_CONNECTION_DISCONNECTED 0
#define KMS_CONNECTION_CONNECTED 1
#define REMB_PARAMS "remb-params"

#define KMS_STATISTIC_FIELD_PREFIX_SESSION "session-"
#define KMS_STATISTIC_FIELD_PREFIX_SSRC "ssrc-"

#define PARAM_MIN_PORT "minPort"
#define PARAM_MAX_PORT "maxPort"
#define PARAM_MTU "mtu"

#define PROP_MIN_PORT "min-port"
#define PROP_MAX_PORT "max-port"
#define PROP_MTU "mtu"

/* Fixed point conversion macros */
#define FRIC        65536.                  /* 2^16 as a double */
#define FP2D(r)     ((double)(r) / FRIC)

namespace kurento
{
void BaseRtpEndpointImpl::postConstructor ()
{
  SdpEndpointImpl::postConstructor ();

  mediaStateChangedHandlerId = register_signal_handler (G_OBJECT (element),
                               "media-state-changed",
                               std::function <void (GstElement *, guint) > (std::bind (
                                     &BaseRtpEndpointImpl::updateMediaState, this,
                                     std::placeholders::_2) ),
                               std::dynamic_pointer_cast<BaseRtpEndpointImpl>
                               (shared_from_this() ) );

  connStateChangedHandlerId = register_signal_handler (G_OBJECT (element),
                              "connection-state-changed",
                              std::function <void (GstElement *, gchar *, guint) > (std::bind (
                                    &BaseRtpEndpointImpl::updateConnectionState, this,
                                    std::placeholders::_2, std::placeholders::_3) ),
                              std::dynamic_pointer_cast<BaseRtpEndpointImpl>
                              (shared_from_this() ) );
}

BaseRtpEndpointImpl::BaseRtpEndpointImpl (const boost::property_tree::ptree
    &config,
    std::shared_ptr< MediaObjectImpl > parent,
    const std::string &factoryName, bool useIpv6) :
  SdpEndpointImpl (config, parent, factoryName, useIpv6)
{
  current_media_state = std::make_shared <MediaState>
                        (MediaState::DISCONNECTED);
  mediaStateChangedHandlerId = 0;

  current_conn_state = std::make_shared <ConnectionState>
                       (ConnectionState::DISCONNECTED);
  connStateChangedHandlerId = 0;

  guint minPort = 0;
  if (getConfigValue<guint, BaseRtpEndpoint> (&minPort, PARAM_MIN_PORT)) {
    g_object_set (getGstreamerElement (), PROP_MIN_PORT, minPort, NULL);
  }

  guint maxPort = 0;
  if (getConfigValue <guint, BaseRtpEndpoint> (&maxPort, PARAM_MAX_PORT)) {
    g_object_set (getGstreamerElement (), PROP_MAX_PORT, maxPort, NULL);
  }

  guint mtu;
  if (getConfigValue <guint, BaseRtpEndpoint> (&mtu, PARAM_MTU)) {
    GST_INFO ("Predefined RTP MTU: %u", mtu);
    g_object_set (G_OBJECT (element), PROP_MTU, mtu, NULL);
  } else {
    GST_DEBUG ("No predefined RTP MTU found in config; using default");
  }
}

BaseRtpEndpointImpl::~BaseRtpEndpointImpl ()
{
  if (mediaStateChangedHandlerId > 0) {
    unregister_signal_handler (element, mediaStateChangedHandlerId);
  }

  if (connStateChangedHandlerId > 0) {
    unregister_signal_handler (element, connStateChangedHandlerId);
  }
}

void
BaseRtpEndpointImpl::updateMediaState (guint new_state)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::shared_ptr<MediaState> old_state = current_media_state;

  switch (new_state) {
  case KMS_MEDIA_DISCONNECTED:
    current_media_state = std::make_shared <MediaState>
                          (MediaState::DISCONNECTED);
    break;

  case KMS_MEDIA_CONNECTED:
    current_media_state = std::make_shared <MediaState>
                          (MediaState::CONNECTED);
    break;

  default:
    GST_ERROR ("Invalid media state %u", new_state);
    return;
  }

  if (old_state->getValue() != current_media_state->getValue() ) {
    GST_DEBUG_OBJECT (element, "MediaState changed to '%s'",
        current_media_state->getString().c_str());
    try {
      MediaStateChanged event (shared_from_this (),
          MediaStateChanged::getName (), old_state, current_media_state);
      sigcSignalEmit(signalMediaStateChanged, event);
    } catch (const std::bad_weak_ptr &e) {
      // shared_from_this()
      GST_ERROR ("BUG creating %s: %s", MediaStateChanged::getName ().c_str (),
          e.what ());
    }
  }
}

void
BaseRtpEndpointImpl::updateConnectionState (gchar *sessId, guint new_state)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::shared_ptr<ConnectionState> old_state = current_conn_state;

  switch (new_state) {
  case KMS_CONNECTION_DISCONNECTED:
    current_conn_state = std::make_shared <ConnectionState>
                         (ConnectionState::DISCONNECTED);
    break;

  case KMS_CONNECTION_CONNECTED:
    current_conn_state = std::make_shared <ConnectionState>
                         (ConnectionState::CONNECTED);
    break;

  default:
    GST_ERROR ("Invalid connection state %u", new_state);
    return;
  }

  if (old_state->getValue() != current_conn_state->getValue() ) {
    GST_DEBUG_OBJECT (element, "ConnectionState changed to '%s'",
        current_conn_state->getString().c_str());
    try {
      ConnectionStateChanged event (shared_from_this(),
          ConnectionStateChanged::getName (), old_state, current_conn_state);
      sigcSignalEmit(signalConnectionStateChanged, event);
    } catch (const std::bad_weak_ptr &e) {
      // shared_from_this()
      GST_ERROR ("BUG creating %s: %s",
          ConnectionStateChanged::getName ().c_str (), e.what ());
    }
  }
}

int BaseRtpEndpointImpl::getMinVideoRecvBandwidth ()
{
  int minVideoRecvBandwidth;

  g_object_get (element, "min-video-recv-bandwidth", &minVideoRecvBandwidth,
                NULL);

  return minVideoRecvBandwidth;
}

void BaseRtpEndpointImpl::setMinVideoRecvBandwidth (int minVideoRecvBandwidth)
{
  g_object_set (element, "min-video-recv-bandwidth", minVideoRecvBandwidth, NULL);
}

int BaseRtpEndpointImpl::getMinVideoSendBandwidth ()
{
  int minVideoSendBandwidth;

  g_object_get (element, "min-video-send-bandwidth", &minVideoSendBandwidth,
                NULL);

  return minVideoSendBandwidth;
}

void BaseRtpEndpointImpl::setMinVideoSendBandwidth (int minVideoSendBandwidth)
{
  g_object_set (element, "min-video-send-bandwidth", minVideoSendBandwidth, NULL);
}

int BaseRtpEndpointImpl::getMaxVideoSendBandwidth ()
{
  int maxVideoSendBandwidth;

  g_object_get (element, "max-video-send-bandwidth", &maxVideoSendBandwidth,
                NULL);

  return maxVideoSendBandwidth;
}

void BaseRtpEndpointImpl::setMaxVideoSendBandwidth (int maxVideoSendBandwidth)
{
  g_object_set (element, "max-video-send-bandwidth", maxVideoSendBandwidth, NULL);
}

std::shared_ptr<MediaState>
BaseRtpEndpointImpl::getMediaState ()
{
  return current_media_state;
}

std::shared_ptr<ConnectionState>
BaseRtpEndpointImpl::getConnectionState ()
{
  return current_conn_state;
}

std::shared_ptr<RembParams>
BaseRtpEndpointImpl::getRembParams ()
{
  std::shared_ptr<RembParams> ret (new RembParams() );
  GstStructure *params;
  gint auxi;
  gfloat auxf;

  g_object_get (G_OBJECT (element), REMB_PARAMS, &params, NULL);

  if (params == nullptr) {
    return ret;
  }

  /* REMB local begin */
  gst_structure_get (params, "packets-recv-interval-top", G_TYPE_INT, &auxi,
                     NULL);
  ret->setPacketsRecvIntervalTop (auxi);

  gst_structure_get (params, "exponential-factor", G_TYPE_FLOAT, &auxf, NULL);
  ret->setExponentialFactor (auxf);

  gst_structure_get (params, "lineal-factor-min", G_TYPE_INT, &auxi, NULL);
  ret->setLinealFactorMin (auxi);

  gst_structure_get (params, "lineal-factor-grade", G_TYPE_FLOAT, &auxf, NULL);
  ret->setLinealFactorGrade (auxf);

  gst_structure_get (params, "decrement-factor", G_TYPE_FLOAT, &auxf, NULL);
  ret->setDecrementFactor (auxf);

  gst_structure_get (params, "threshold-factor", G_TYPE_FLOAT, &auxf, NULL);
  ret->setThresholdFactor (auxf);

  gst_structure_get (params, "up-losses", G_TYPE_INT, &auxi, NULL);
  ret->setUpLosses (auxi);
  /* REMB local end */

  /* REMB remote begin */
  gst_structure_get (params, "remb-on-connect", G_TYPE_INT, &auxi, NULL);
  ret->setRembOnConnect (auxi);
  /* REMB remote end */

  gst_structure_free (params);

  return ret;
}

void
BaseRtpEndpointImpl::setRembParams (std::shared_ptr<RembParams> rembParams)
{
  GstStructure *params = gst_structure_new_empty (REMB_PARAMS);

  /* REMB local begin */
  if (rembParams->isSetPacketsRecvIntervalTop () ) {
    gst_structure_set (params, "packets-recv-interval-top", G_TYPE_INT,
                       rembParams->getPacketsRecvIntervalTop(), NULL);
    GST_DEBUG_OBJECT (element, "New 'packets-recv-interval-top' value %d",
                      rembParams->getPacketsRecvIntervalTop() );
  }

  if (rembParams->isSetExponentialFactor () ) {
    gst_structure_set (params, "exponential-factor", G_TYPE_FLOAT,
                       rembParams->getExponentialFactor(), NULL);
    GST_DEBUG_OBJECT (element, "New 'exponential-factor' value %g",
                      rembParams->getExponentialFactor() );
  }

  if (rembParams->isSetLinealFactorMin () ) {
    gst_structure_set (params, "lineal-factor-min", G_TYPE_INT,
                       rembParams->getLinealFactorMin(), NULL);
    GST_DEBUG_OBJECT (element, "New 'lineal-factor-min' value %d",
                      rembParams->getLinealFactorMin() );
  }

  if (rembParams->isSetLinealFactorGrade () ) {
    gst_structure_set (params, "lineal-factor-grade", G_TYPE_FLOAT,
                       rembParams->getLinealFactorGrade(), NULL);
    GST_DEBUG_OBJECT (element, "New 'lineal-factor-grade' value %g",
                      rembParams->getLinealFactorGrade() );
  }

  if (rembParams->isSetDecrementFactor () ) {
    gst_structure_set (params, "decrement-factor", G_TYPE_FLOAT,
                       rembParams->getDecrementFactor(), NULL);
    GST_DEBUG_OBJECT (element, "New 'decrement-factor' value %g",
                      rembParams->getDecrementFactor() );
  }

  if (rembParams->isSetThresholdFactor () ) {
    gst_structure_set (params, "threshold-factor", G_TYPE_FLOAT,
                       rembParams->getThresholdFactor(), NULL);
    GST_DEBUG_OBJECT (element, "New 'threshold-factor' value %g",
                      rembParams->getThresholdFactor() );
  }

  if (rembParams->isSetUpLosses () ) {
    gst_structure_set (params, "up-losses", G_TYPE_INT,
                       rembParams->getUpLosses(), NULL);
    GST_DEBUG_OBJECT (element, "New 'up-losses' value %d",
                      rembParams->getUpLosses() );
  }

  /* REMB local end */

  /* REMB remote begin */
  if (rembParams->isSetRembOnConnect () ) {
    gst_structure_set (params, "remb-on-connect", G_TYPE_INT,
                       rembParams->getRembOnConnect(), NULL);
    GST_DEBUG_OBJECT (element, "New 'remb-on-connect' value %d",
                      rembParams->getRembOnConnect() );
  }

  /* REMB remote end */

  g_object_set (G_OBJECT (element), REMB_PARAMS, params, NULL);
  gst_structure_free (params);
}

int
BaseRtpEndpointImpl::getMtu ()
{
  int mtu;

  g_object_get (G_OBJECT (element), PROP_MTU, &mtu, NULL);

  return mtu;
}

void
BaseRtpEndpointImpl::setMtu (int mtu)
{
  GST_INFO ("Set MTU for RTP: %d", mtu);
  g_object_set (G_OBJECT (element), PROP_MTU, mtu, NULL);
}

/******************/
/* RTC statistics */
/******************/
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
         std::make_shared <StatsType> (StatsType::inboundrtp), 0.0, 0, "",
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
         std::make_shared <StatsType> (StatsType::outboundrtp), 0.0, 0, "",
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
                          &statsReport, double timestamp,
                          int64_t timestampMillis, const GstStructure *stats)
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
    rtcStats->setTimestampMillis (timestampMillis);

    statsReport[rtcStats->getId ()] = rtcStats;
  }
}

static void
collectRTCStats (std::map <std::string, std::shared_ptr<Stats>>
                 &statsReport, double timestamp, int64_t timestampMillis,
                 const GstStructure *stats)
{
  gint i, n;

  n = gst_structure_n_fields (stats);

  for (i = 0; i < n; i++) {
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

    collectRTCRTPStreamStats (statsReport, timestamp, timestampMillis,
                              gst_value_get_structure (value) );
  }
}

static void
setDeprecatedProperties (std::shared_ptr<EndpointStats> eStats)
{
  std::vector<std::shared_ptr<MediaLatencyStat>> inStats =
        eStats->getE2ELatency();

  for (auto &inStat : inStats) {
    if (inStat->getName() == "sink_audio_default") {
      eStats->setAudioE2ELatency(inStat->getAvg());
    } else if (inStat->getName() == "sink_video_default") {
      eStats->setVideoE2ELatency(inStat->getAvg());
    }
  }
}

void
BaseRtpEndpointImpl::collectEndpointStats (std::map
    <std::string, std::shared_ptr<Stats>>
    &statsReport, std::string id, const GstStructure *stats,
    double timestamp, int64_t timestampMillis)
{
  std::shared_ptr<Stats> endpointStats;
  GstStructure *e2e_stats;

  std::vector<std::shared_ptr<MediaLatencyStat>> inputStats;
  std::vector<std::shared_ptr<MediaLatencyStat>> e2eStats;

  if (gst_structure_get (stats, "e2e-latencies", GST_TYPE_STRUCTURE,
                         &e2e_stats, NULL) ) {
    collectLatencyStats (e2eStats, e2e_stats);
    gst_structure_free (e2e_stats);
  }

  endpointStats = std::make_shared <EndpointStats> (id,
                  std::make_shared <StatsType> (StatsType::endpoint), timestamp,
                  timestampMillis, 0.0, 0.0, inputStats, 0.0, 0.0, e2eStats);

  setDeprecatedProperties (std::dynamic_pointer_cast <EndpointStats>
                           (endpointStats) );

  statsReport[id] = endpointStats;
}

void
BaseRtpEndpointImpl::fillStatsReport (std::map
                                      <std::string, std::shared_ptr<Stats>>
                                      &report, const GstStructure *stats,
                                      double timestamp, int64_t timestampMillis)
{
  const GstStructure *e_stats, *rtc_stats;

  e_stats = kms_utils_get_structure_by_name (stats, KMS_MEDIA_ELEMENT_FIELD);

  if (e_stats != nullptr) {
    collectEndpointStats (report, getId (), e_stats, timestamp, timestampMillis);
  }

  rtc_stats = kms_utils_get_structure_by_name (stats, KMS_RTC_STATISTICS_FIELD);

  if (rtc_stats != nullptr) {
    collectRTCStats (report, timestamp, timestampMillis, rtc_stats );
  }

  SdpEndpointImpl::fillStatsReport (report, stats, timestamp, timestampMillis);
}

BaseRtpEndpointImpl::StaticConstructor BaseRtpEndpointImpl::staticConstructor;

BaseRtpEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
