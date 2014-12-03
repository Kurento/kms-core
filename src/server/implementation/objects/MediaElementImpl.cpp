#include <gst/gst.h>
#include "MediaType.hpp"
#include "AudioCaps.hpp"
#include "VideoCaps.hpp"
#include "AudioCodec.hpp"
#include "VideoCodec.hpp"
#include "Fraction.hpp"
#include "MediaElementImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <MediaPipelineImpl.hpp>
#include <MediaSet.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_media_element_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaElementImpl"

namespace kurento
{

void
_media_element_impl_bus_message (GstBus *bus, GstMessage *message,
                                 gpointer data)
{
  if (message->type == GST_MESSAGE_ERROR) {
    GError *err = NULL;
    gchar *debug = NULL;
    MediaElementImpl *elem = reinterpret_cast <MediaElementImpl *> (data);

    if (elem == NULL) {
      return;
    }

    if (message->src != GST_OBJECT (elem->element) ) {
      return;
    }

    GST_ERROR ("MediaElement error: %" GST_PTR_FORMAT, message);
    gst_message_parse_error (message, &err, &debug);
    std::string errorMessage (err->message);

    if (debug != NULL) {
      errorMessage += " -> " + std::string (debug);
    }

    try {
      Error error (elem->shared_from_this(), errorMessage , 0,
                   "UNEXPECTED_ELEMENT_ERROR");

      elem->signalError (error);
    } catch (std::bad_weak_ptr &e) {
    }

    g_error_free (err);
    g_free (debug);
  }
}

MediaElementImpl::MediaElementImpl (const boost::property_tree::ptree &config,
                                    std::shared_ptr<MediaObjectImpl> parent,
                                    const std::string &factoryName) : MediaObjectImpl (config, parent)
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  element = gst_element_factory_make (factoryName.c_str(), NULL);

  if (element == NULL) {
    throw KurentoException (MEDIA_OBJECT_NOT_AVAILABLE,
                            "Cannot create gstreamer element: " + factoryName);
  }


  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe->getPipeline () ) );
  handlerId = g_signal_connect (bus, "message",
                                G_CALLBACK (_media_element_impl_bus_message), this);

  g_object_ref (element);
  gst_bin_add (GST_BIN ( pipe->getPipeline() ), element);
  gst_element_sync_state_with_parent (element);
}

MediaElementImpl::~MediaElementImpl ()
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  gst_element_set_locked_state (element, TRUE);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_bin_remove (GST_BIN ( pipe->getPipeline() ), element);
  g_object_unref (element);

  g_signal_handler_disconnect (bus, handlerId);
  g_object_unref (bus);
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections ()
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections (
      std::shared_ptr<MediaType> mediaType)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections (
      std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSinkConnections ()
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSinkConnections (
      std::shared_ptr<MediaType> mediaType)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSinkConnections (
      std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType,
                                const std::string &sourceMediaDescription)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType,
                                const std::string &sourceMediaDescription,
                                const std::string &sinkMediaDescription)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType,
                                   const std::string &sourceMediaDescription)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType,
                                   const std::string &sourceMediaDescription,
                                   const std::string &sinkMediaDescription)
{
  throw KurentoException (NOT_IMPLEMENTED, "Not implemented yet");
}

void MediaElementImpl::setAudioFormat (std::shared_ptr<AudioCaps> caps)
{
  std::shared_ptr<AudioCodec> codec;
  std::stringstream sstm;
  std::string str_caps;
  GstCaps *c = NULL;

  codec = caps->getCodec();

  switch (codec->getValue() ) {
  case AudioCodec::OPUS:
    str_caps = "audio/x-opus";
    break;

  case AudioCodec::PCMU:
    str_caps = "audio/x-mulaw";
    break;

  case AudioCodec::RAW:
    str_caps = "audio/x-raw";
    break;

  default:
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Invalid parameter provided: " + codec->getString() );
  }

  sstm << str_caps << ", bitrate=(int)" << caps->getBitrate();
  str_caps = sstm.str ();

  c = gst_caps_from_string (str_caps.c_str() );
  g_object_set (element, "audio-caps", c, NULL);
}

void MediaElementImpl::setVideoFormat (std::shared_ptr<VideoCaps> caps)
{
  std::shared_ptr<VideoCodec> codec;
  std::shared_ptr<Fraction> fraction;
  std::stringstream sstm;
  std::string str_caps;
  GstCaps *c = NULL;

  codec = caps->getCodec();
  fraction = caps->getFramerate();

  switch (codec->getValue() ) {
  case VideoCodec::VP8:
    str_caps = "video/x-vp8";
    break;

  case VideoCodec::H264:
    str_caps = "video/x-h264";
    break;

  case VideoCodec::RAW:
    str_caps = "video/x-raw";
    break;

  default:
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Invalid parameter provided: " + codec->getString() );
  }

  sstm << str_caps << ", framerate=(fraction)" << fraction->getNumerator() <<
       "/" << fraction->getDenominator();
  str_caps = sstm.str ();

  c = gst_caps_from_string (str_caps.c_str() );
  g_object_set (element, "video-caps", c, NULL);
}

MediaElementImpl::StaticConstructor MediaElementImpl::staticConstructor;

MediaElementImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
