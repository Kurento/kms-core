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
#include <ElementConnectionData.hpp>
#include "kmselement.h"

#define GST_CAT_DEFAULT kurento_media_element_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaElementImpl"

namespace kurento
{

class ElementConnectionDataInternal
{
public:
  ElementConnectionDataInternal (std::shared_ptr<MediaElement> source,
                                 std::shared_ptr<MediaElement> sink,
                                 std::shared_ptr<MediaType> type,
                                 const std::string &sourceDescription,
                                 const std::string &sinkDescription)
  {
    this->source = source;
    this->sink = sink;
    this->type = type;
    this->sourceDescription = sourceDescription;
    this->sinkDescription = sinkDescription;
    this->sourcePadName = NULL;
    setSinkPadName ();
  }

  ~ElementConnectionDataInternal()
  {
    if (sourcePadName != NULL) {
      free (sourcePadName);
    }
  }

  ElementConnectionDataInternal (std::shared_ptr<ElementConnectionData> data)
  {
    this->source = source;
    this->sink = sink;
    this->type = type;
    this->sourceDescription = sourceDescription;
    this->sinkDescription = sinkDescription;
    this->sourcePadName = NULL;
    setSinkPadName ();
  }

  void setSinkPadName()
  {
    std::string desc = (sourceDescription.empty () ? "" : "_") + sourceDescription;

    switch (type->getValue () ) {
    case MediaType::AUDIO:
      sinkPadName = "sink_audio" + desc;
      break;

    case MediaType::VIDEO:
      sinkPadName = "sink_video" + desc;
      break;

    case MediaType::DATA:
      sinkPadName = "sink_data" + desc;
      break;
    }
  }

  void setSourcePadName (gchar *padName)
  {
    if (this->sourcePadName != NULL) {
      GST_WARNING ("Resetting padName for connection");

      if (this->sourcePadName != padName) {
        free (this->sourcePadName);
      }
    }

    this->sourcePadName = padName;
  }

  const gchar *getSourcePadName ()
  {
    return sourcePadName;
  }

  std::shared_ptr<MediaElementImpl> getSource ()
  {
    try {
      return std::dynamic_pointer_cast <MediaElementImpl> (source.lock() );
    } catch (std::bad_cast) {
      GST_WARNING ("Bad cast for source element");
      return std::shared_ptr<MediaElementImpl> ();
    }
  }

  std::shared_ptr<MediaElementImpl> getSink ()
  {
    try {
      return std::dynamic_pointer_cast <MediaElementImpl> (sink.lock() );
    } catch (std::bad_cast) {
      GST_WARNING ("Bad cast for source element");
      return std::shared_ptr<MediaElementImpl> ();
    }
  }

  std::string getSinkPadName ()
  {
    return sinkPadName;
  }

  GstPad *getSinkPad ()
  {
    std::shared_ptr <MediaElementImpl> sinkLocked = getSink ();

    if (!sinkLocked) {
      return NULL;
    }

    return gst_element_get_static_pad (sinkLocked->getGstreamerElement (),
                                       getSinkPadName ().c_str() );
  }

  GstPad *getSourcePad ()
  {
    std::shared_ptr <MediaElementImpl> sourceLocked = getSource ();

    if (!sourceLocked || sourcePadName == NULL) {
      return NULL;
    }

    return gst_element_get_static_pad (sourceLocked->getGstreamerElement (),
                                       sourcePadName);
  }

  std::shared_ptr<ElementConnectionData> toInterface ()
  {
    std::shared_ptr<ElementConnectionData> iface (new ElementConnectionData (
          source.lock(), sink.lock(), type, sourceDescription, sinkDescription) );

    if (!iface->getSink () ) {
      throw KurentoException (MEDIA_OBJECT_NOT_FOUND, "Reference to sink is null");
    }

    if (!iface->getSource () ) {
      throw KurentoException (MEDIA_OBJECT_NOT_FOUND, "Reference to source is null");
    }

    return iface;
  }

private:

  std::weak_ptr<MediaElement> source;
  std::weak_ptr<MediaElement> sink;
  std::shared_ptr<MediaType> type;
  std::string sourceDescription;
  std::string sinkDescription;
  std::string sinkPadName;
  gchar *sourcePadName;
};

static KmsElementPadType
convertMediaType (std::shared_ptr<MediaType> mediaType)
{
  switch (mediaType->getValue () ) {
  case MediaType::AUDIO:
    return KMS_ELEMENT_PAD_TYPE_AUDIO;

  case MediaType::VIDEO:
    return KMS_ELEMENT_PAD_TYPE_VIDEO;

  case MediaType::DATA:
    return KMS_ELEMENT_PAD_TYPE_DATA;
  }

  throw KurentoException (UNSUPPORTED_MEDIA_TYPE, "Usupported media type");
}

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

void
_media_element_pad_added (GstElement *elem, GstPad *pad, gpointer data)
{
  MediaElementImpl *self = (MediaElementImpl *) data;

  GST_LOG_OBJECT (pad, "Pad added");

  if (GST_PAD_IS_SRC (pad) ) {
    std::unique_lock<std::recursive_mutex> lock (self->sinksMutex);
    std::shared_ptr<MediaType> type;
    //FIXME: This method of pad recognition should change as well as pad names

    if (g_str_has_prefix (GST_OBJECT_NAME (pad), "audio") ) {
      type = std::shared_ptr<MediaType> (new MediaType (MediaType::AUDIO) );
    } else if (g_str_has_prefix (GST_OBJECT_NAME (pad), "video") ) {
      type = std::shared_ptr<MediaType> (new MediaType (MediaType::VIDEO) );
    } else {
      type = std::shared_ptr<MediaType> (new MediaType (MediaType::DATA) );
    }

    try {
      auto connections = self->sinks.at (type).at ("");

      for (auto it : connections) {
        if (g_strcmp0 (GST_OBJECT_NAME (pad), it->getSourcePadName() ) == 0) {
          std::unique_lock<std::recursive_mutex> sinkLock (it->getSink()->sourcesMutex);

          self->performConnection (it);
        }
      }
    } catch (std::out_of_range) {

    }
  } else {
    std::unique_lock<std::recursive_mutex> lock (self->sourcesMutex);
    std::shared_ptr<MediaType> type;

    if (g_str_has_prefix (GST_OBJECT_NAME (pad), "sink_audio") ) {
      type = std::shared_ptr<MediaType> (new MediaType (MediaType::AUDIO) );
    } else if (g_str_has_prefix (GST_OBJECT_NAME (pad), "sink_video") ) {
      type = std::shared_ptr<MediaType> (new MediaType (MediaType::VIDEO) );
    } else {
      type = std::shared_ptr<MediaType> (new MediaType (MediaType::DATA) );
    }

    try {
      auto sourceData = self->sources.at (type).at ("");
      auto source = sourceData->getSource();

      if (source) {
        if (g_strcmp0 (GST_OBJECT_NAME (pad),
                       sourceData->getSinkPadName().c_str() ) == 0) {
          std::unique_lock<std::recursive_mutex> sourceLock (source->sinksMutex);

          source->performConnection (sourceData);
        }
      }
    } catch (std::out_of_range) {

    }
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


  padAddedHandlerId = g_signal_connect (element, "pad_added",
                                        G_CALLBACK (_media_element_pad_added), this);

  g_object_ref (element);
  gst_bin_add (GST_BIN ( pipe->getPipeline() ), element);
  gst_element_sync_state_with_parent (element);
}

MediaElementImpl::~MediaElementImpl ()
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  GST_LOG ("Deleting media element %s", getName().c_str () );

  disconnectAll();

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  gst_element_set_locked_state (element, TRUE);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_bin_remove (GST_BIN ( pipe->getPipeline() ), element);
  g_signal_handler_disconnect (element, padAddedHandlerId);
  g_object_unref (element);

  g_signal_handler_disconnect (bus, handlerId);
  g_object_unref (bus);
}

void
MediaElementImpl::release ()
{
  disconnectAll ();

  MediaObjectImpl::release();
}

void MediaElementImpl::disconnectAll ()
{
  std::unique_lock<std::recursive_mutex> sourceLock (sourcesMutex);
  std::unique_lock<std::recursive_mutex> sinkLock (sinksMutex);

  for (std::shared_ptr<ElementConnectionData> connData :
       getSourceConnections() ) {
    connData->getSource ()->disconnect (connData->getSink (),
                                        connData->getType (),
                                        connData->getSourceDescription (),
                                        connData->getSinkDescription () );
  }

  for (std::shared_ptr<ElementConnectionData> connData : getSinkConnections() ) {
    disconnect (connData->getSink (), connData->getType (),
                connData->getSourceDescription (),
                connData->getSinkDescription () );
  }
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections ()
{
  std::unique_lock<std::recursive_mutex> lock (sourcesMutex);
  std::vector<std::shared_ptr<ElementConnectionData>> ret;

  for (auto it : sources) {
    for (auto it2 : it.second) {
      try {
        ret.push_back (it2.second->toInterface() );
      } catch (KurentoException) {
      }
    }
  }

  return ret;
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections (
      std::shared_ptr<MediaType> mediaType)
{
  std::unique_lock<std::recursive_mutex> lock (sourcesMutex);
  std::vector<std::shared_ptr<ElementConnectionData>> ret;

  try {
    for (auto it : sources.at (mediaType) ) {
      try {
        ret.push_back (it.second->toInterface() );
      } catch (KurentoException) {

      }
    }
  } catch (std::out_of_range) {

  }

  return ret;
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections (
      std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  std::unique_lock<std::recursive_mutex> lock (sourcesMutex);
  std::vector<std::shared_ptr<ElementConnectionData>> ret;

  try {
    ret.push_back (sources.at (mediaType).at (description)->toInterface() );
  } catch (KurentoException) {

  } catch (std::out_of_range) {

  }

  return ret;
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSinkConnections ()
{
  std::unique_lock<std::recursive_mutex> lock (sinksMutex);
  std::vector<std::shared_ptr<ElementConnectionData>> ret;

  for (auto it : sinks) {
    for (auto it2 : it.second) {
      for (auto it3 : it2.second) {
        try {
          ret.push_back (it3->toInterface() );
        } catch (KurentoException) {

        }
      }
    }
  }

  return ret;
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSinkConnections (
      std::shared_ptr<MediaType> mediaType)
{
  std::unique_lock<std::recursive_mutex> lock (sinksMutex);
  std::vector<std::shared_ptr<ElementConnectionData>> ret;

  try {
    for (auto it : sinks.at (mediaType) ) {
      for (auto it3 : it.second) {
        try {
          ret.push_back (it3->toInterface() );
        } catch (KurentoException) {

        }
      }
    }
  } catch (std::out_of_range) {

  }

  return ret;
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSinkConnections (
      std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  std::unique_lock<std::recursive_mutex> lock (sinksMutex);
  std::vector<std::shared_ptr<ElementConnectionData>> ret;

  try {
    for (auto it : sinks.at (mediaType).at (description) ) {
      try {
        ret.push_back (it->toInterface() );
      } catch (KurentoException) {

      }
    }
  } catch (std::out_of_range) {

  }

  return ret;
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink)
{
  // Until mediaDescriptions are really used, we just connect audio an video
  connect (sink, std::shared_ptr<MediaType> (new MediaType (MediaType::AUDIO) ),
           "", "");
  connect (sink, std::shared_ptr<MediaType> (new MediaType (MediaType::VIDEO) ),
           "", "");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType)
{
  connect (sink, mediaType, "", "");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType,
                                const std::string &sourceMediaDescription)
{
  connect (sink, mediaType, sourceMediaDescription, "");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType,
                                const std::string &sourceMediaDescription,
                                const std::string &sinkMediaDescription)
{
  KmsElementPadType type;
  gchar *padName;
  std::shared_ptr<MediaElementImpl> sinkImpl =
    std::dynamic_pointer_cast<MediaElementImpl> (sink);

  if (sinkImpl->getMediaPipeline ()->getId () != getMediaPipeline ()->getId() ) {
    throw KurentoException (CONNECT_ERROR,
                            "Media elements does not share pipeline");
  }

  std::unique_lock<std::recursive_mutex> lock (sinksMutex);
  std::unique_lock<std::recursive_mutex> sinkLock (sinkImpl->sourcesMutex);
  std::vector <std::shared_ptr <ElementConnectionData>> connections;
  std::shared_ptr <ElementConnectionDataInternal> connectionData (
    new ElementConnectionDataInternal (std::dynamic_pointer_cast<MediaElement>
                                       (shared_from_this () ), sink, mediaType,
                                       sourceMediaDescription,
                                       sinkMediaDescription) );

  GST_DEBUG ("Connecting %s -> %s params %s %s %s", getName().c_str(),
             sink->getName ().c_str (), mediaType->getString ().c_str (),
             sourceMediaDescription.c_str(), sinkMediaDescription.c_str() );

  connections = sink->getSourceConnections (mediaType, sinkMediaDescription);

  if (!connections.empty () ) {
    std::shared_ptr <ElementConnectionData> connection = connections.at (0);
    connection->getSource()->disconnect (connection->getSink (), mediaType,
                                         sourceMediaDescription,
                                         connection->getSinkDescription () );
  }

  type = convertMediaType (mediaType);
  g_signal_emit_by_name (getGstreamerElement (), "request-new-srcpad", type,
                         sourceMediaDescription.c_str (), &padName, NULL);

  if (padName == NULL) {
    throw KurentoException (CONNECT_ERROR, "Element: '" + getName() +
                            "'does note provide a connection for " +
                            mediaType->getString () + "-" +
                            sourceMediaDescription);
  }

  connectionData->setSourcePadName (padName);

  sinks[mediaType][sourceMediaDescription].insert (connectionData);
  sinkImpl->sources[mediaType][sinkMediaDescription] = connectionData;

  performConnection (connectionData);
}

void
MediaElementImpl::performConnection (std::shared_ptr
                                     <ElementConnectionDataInternal> data)
{
  GstPad *src = NULL, *sink = NULL;

  src = data->getSourcePad ();

  if (!src) {
    GST_TRACE ("Still waiting for src pad %s:%s", getName().c_str(),
               data->getSourcePadName () );
    return;
  }

  sink = data->getSinkPad ();

  if (sink) {
    GstPadLinkReturn ret;

    GST_TRACE ("Linking %s:%s -> %s:%s", getName().c_str(),
               data->getSourcePadName (), data->getSink()->getName().c_str(),
               data->getSinkPadName ().c_str() );

    ret = gst_pad_link_full (src, sink, GST_PAD_LINK_CHECK_NOTHING);

    if (ret != GST_PAD_LINK_OK) {
      GST_WARNING ("Cannot link pads: %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT
                   " reason: %s", src, sink, gst_pad_link_get_name (ret) );
    } else {
      GST_TRACE ("Link done");
    }

    g_object_unref (sink);
  } else {
    GST_TRACE ("Still waiting for sink pad %s:%s",
               data->getSink()->getName().c_str(),
               data->getSinkPadName ().c_str() );
  }

  g_object_unref (src);
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink)
{
  // Until mediaDescriptions are really used, we just disconnect audio an video
  disconnect (sink, std::shared_ptr<MediaType> (new MediaType (
                MediaType::AUDIO) ), "", "");
  disconnect (sink, std::shared_ptr<MediaType> (new MediaType (
                MediaType::VIDEO) ), "", "");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType)
{
  disconnect (sink, mediaType, "", "");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType,
                                   const std::string &sourceMediaDescription)
{
  disconnect (sink, mediaType, sourceMediaDescription, "");
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType,
                                   const std::string &sourceMediaDescription,
                                   const std::string &sinkMediaDescription)
{
  if (!sink) {
    GST_WARNING ("Sink not available while disconnecting");
    return;
  }

  std::shared_ptr<MediaElementImpl> sinkImpl =
    std::dynamic_pointer_cast<MediaElementImpl> (sink);
  std::unique_lock<std::recursive_mutex> sinkLock (sinkImpl->sourcesMutex);
  std::unique_lock<std::recursive_mutex> lock (sinksMutex);

  GST_DEBUG ("Disconnecting %s - %s params %s %s %s", getName().c_str(),
             sink->getName ().c_str (), mediaType->getString ().c_str (),
             sourceMediaDescription.c_str(), sinkMediaDescription.c_str() );

  try {
    std::shared_ptr<ElementConnectionDataInternal> connectionData;
    connectionData = sinkImpl->sources.at (mediaType).at (sourceMediaDescription);
    sinkImpl->sources.at (mediaType).erase (sourceMediaDescription);
    sinks.at (mediaType).at (sinkMediaDescription).erase (connectionData);

    // TODO: Disconnect gstreamer elements
  } catch (std::out_of_range) {

  }

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
