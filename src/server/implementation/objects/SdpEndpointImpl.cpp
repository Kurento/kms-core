#include <gst/gst.h>
#include "SdpEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <boost/filesystem.hpp>
#include <fstream>

#define GST_CAT_DEFAULT kurento_sdp_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoSdpEndpointImpl"

namespace kurento
{

static GstSDPMessage *
str_to_sdp (const std::string &sdpStr)
{
  GstSDPResult result;
  GstSDPMessage *sdp = NULL;

  result = gst_sdp_message_new (&sdp);

  if (result != GST_SDP_OK) {
    throw KurentoException (SDP_CREATE_ERROR, "Error creating SDP message");
  }

  result = gst_sdp_message_parse_buffer ( (const guint8 *) sdpStr.c_str (), -1,
                                          sdp);

  if (result != GST_SDP_OK) {

    gst_sdp_message_free (sdp);
    throw KurentoException (SDP_PARSE_ERROR, "Error parsing SDP");
  }

  return sdp;
}

static void
sdp_to_str (std::string &_return, const GstSDPMessage *sdp)
{
  std::string sdpStr;
  gchar *sdpGchar;

  sdpGchar = gst_sdp_message_as_text (sdp);
  _return.clear ();
  _return.append (sdpGchar);
  free (sdpGchar);
}

static std::string
readEntireFile (const std::string &file_name)
{
  std::ifstream t (file_name);
  std::string ret ( (std::istreambuf_iterator<char> (t) ),
                    std::istreambuf_iterator<char>() );

  if (ret.empty() ) {
    GST_ERROR ("Cannot read sdp pattern from: %s", file_name.c_str() );
    throw kurento::KurentoException (SDP_CONFIGURATION_ERROR,
                                     "Error reading SDP pattern from configuration, please contact the administrator");
  }

  return ret;
}

static std::shared_ptr <GstSDPMessage> pattern;

std::mutex SdpEndpointImpl::sdpMutex;

GstSDPMessage *
SdpEndpointImpl::getSdpPattern ()
{
  GstSDPMessage *sdp;
  GstSDPResult result;
  boost::filesystem::path sdp_pattern_file;
  std::unique_lock<std::mutex> lock (sdpMutex);

  if (pattern) {
    return pattern.get();
  }

  try {
    sdp_pattern_file = boost::filesystem::path (
                         getConfigValue<std::string, SdpEndpoint> ("sdpPattern") );
  } catch (boost::property_tree::ptree_error &e) {
    throw kurento::KurentoException (SDP_CONFIGURATION_ERROR,
                                     "Error reading SDP pattern from configuration, please contact the administrator: "
                                     + std::string (e.what() ) );
  }

  if (!sdp_pattern_file.is_absolute() ) {
    try {
      sdp_pattern_file = boost::filesystem::path (
                           config.get<std::string> ("configPath") ) / sdp_pattern_file;
    } catch (boost::property_tree::ptree_error &e) {

    }
  }

  result = gst_sdp_message_new (&sdp);

  if (result != GST_SDP_OK) {
    GST_ERROR ("Error creating sdp message");
    throw kurento::KurentoException (SDP_CREATE_ERROR,
                                     "Error creating SDP pattern");
  }

  pattern = std::shared_ptr<GstSDPMessage> (sdp, gst_sdp_message_free);

  result = gst_sdp_message_parse_buffer ( (const guint8 *) readEntireFile (
      sdp_pattern_file.string() ).c_str(), -1, sdp);

  if (result != GST_SDP_OK) {
    GST_ERROR ("Error parsing SDP config pattern");
    pattern.reset();
    throw kurento::KurentoException (SDP_CONFIGURATION_ERROR,
                                     "Error reading SDP pattern from configuration, please contact the administrator");
  }

  return pattern.get();
}

SdpEndpointImpl::SdpEndpointImpl (const boost::property_tree::ptree &config,
                                  std::shared_ptr< MediaObjectImpl > parent,
                                  const std::string &factoryName) :
  SessionEndpointImpl (config, parent, factoryName)
{
  //   TODO: Add support for this events
  //   g_signal_connect (element, "media-start", G_CALLBACK (media_start_cb), this);
  //   g_signal_connect (element, "media-stop", G_CALLBACK (media_stop_cb), this);
  g_object_set (element, "pattern-sdp", getSdpPattern (), NULL);
  offerInProcess = false;
}


int SdpEndpointImpl::getMaxVideoRecvBandwidth ()
{
  int maxVideoRecvBandwidth;

  g_object_get (element, "max-video-recv-bandwidth", &maxVideoRecvBandwidth,
                NULL);

  return maxVideoRecvBandwidth;
}

void SdpEndpointImpl::setMaxVideoRecvBandwidth (int maxVideoRecvBandwidth)
{
  g_object_set (element, "max-video-recv-bandwidth", maxVideoRecvBandwidth, NULL);
}

std::string SdpEndpointImpl::generateOffer ()
{
  GstSDPMessage *offer = NULL;
  std::string offerStr;
  bool expected = false;

  if (!offerInProcess.compare_exchange_strong (expected, true) ) {
    //the endpoint is already negotiated
    throw KurentoException (SDP_END_POINT_ALREADY_NEGOTIATED,
                            "Endpoint already negotiated");
  }

  if (element == NULL) {
  }

  g_signal_emit_by_name (element, "generate-offer", &offer);

  if (offer == NULL) {
    offerInProcess = false;
    throw KurentoException (SDP_END_POINT_GENERATE_OFFER_ERROR,
                            "Error generating offer");
  }

  sdp_to_str (offerStr, offer);
  gst_sdp_message_free (offer);

  return offerStr;
}

std::string SdpEndpointImpl::processOffer (const std::string &offer)
{
  GstSDPMessage *offerSdp = NULL, *result = NULL;
  std::string offerSdpStr;
  bool expected = false;

  if (!offerInProcess.compare_exchange_strong (expected, true) ) {
    //the endpoint is already negotiated
    throw KurentoException (SDP_END_POINT_ALREADY_NEGOTIATED,
                            "Endpoint already negotiated");
  }

  offerSdp = str_to_sdp (offer);
  g_signal_emit_by_name (element, "process-offer", offerSdp, &result);
  gst_sdp_message_free (offerSdp);

  if (result == NULL) {
    offerInProcess = false;
    throw KurentoException (SDP_END_POINT_PROCESS_OFFER_ERROR,
                            "Error processing offer");
  }

  sdp_to_str (offerSdpStr, result);
  gst_sdp_message_free (result);

  MediaSessionStarted event (shared_from_this(), MediaSessionStarted::getName() );
  signalMediaSessionStarted (event);

  return offerSdpStr;
}

std::string SdpEndpointImpl::processAnswer (const std::string &answer)
{
  GstSDPMessage *answerSdp;
  std::string resultStr;

  answerSdp = str_to_sdp (answer);
  g_signal_emit_by_name (element, "process-answer", answerSdp, NULL);
  gst_sdp_message_free (answerSdp);

  MediaSessionStarted event (shared_from_this(), MediaSessionStarted::getName() );
  signalMediaSessionStarted (event);

  return getLocalSessionDescriptor ();
}

std::string SdpEndpointImpl::getLocalSessionDescriptor ()
{
  GstSDPMessage *localSdp = NULL;
  std::string localSdpStr;

  g_object_get (element, "local-answer-sdp", &localSdp, NULL);

  if (localSdp == NULL) {
    g_object_get (element, "local-offer-sdp", &localSdp, NULL);
  }

  if (localSdp == NULL) {
    throw KurentoException (SDP_END_POINT_NO_LOCAL_SDP_ERROR, "No local SDP");
  }

  sdp_to_str (localSdpStr, localSdp);
  gst_sdp_message_free (localSdp);

  return localSdpStr;
}

std::string SdpEndpointImpl::getRemoteSessionDescriptor ()
{
  GstSDPMessage *remoteSdp = NULL;
  std::string remoteSdpStr;

  g_object_get (element, "remote-answer-sdp", &remoteSdp, NULL);

  if (remoteSdp == NULL) {
    g_object_get (element, "remote-offer-sdp", &remoteSdp, NULL);
  }

  if (remoteSdp == NULL) {
    throw KurentoException (SDP_END_POINT_NO_REMOTE_SDP_ERROR, "No remote SDP");
  }

  sdp_to_str (remoteSdpStr, remoteSdp);;
  gst_sdp_message_free (remoteSdp);

  return remoteSdpStr;
}

SdpEndpointImpl::StaticConstructor SdpEndpointImpl::staticConstructor;

SdpEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
