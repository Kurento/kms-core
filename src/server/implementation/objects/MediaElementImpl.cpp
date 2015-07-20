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
#include <DotGraph.hpp>
#include <GstreamerDotDetails.hpp>
#include <StatsType.hpp>
#include "ElementStats.hpp"
#include "kmsstats.h"

#define GST_CAT_DEFAULT kurento_media_element_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaElementImpl"

#define TARGET_BITRATE "output-bitrate"

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
    this->source = data->getSource();
    this->sink = data->getSink();
    this->type = data->getType();
    this->sourceDescription = data->getSourceDescription();
    this->sinkDescription = data->getSinkDescription();
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
  bool retry = false;

  GST_LOG_OBJECT (pad, "Pad added");

  do {
    if (retry) {
      GST_DEBUG_OBJECT (pad, "Retriying connection");
      retry = false;
    }

    if (GST_PAD_IS_SRC (pad) ) {
      std::unique_lock<std::recursive_timed_mutex> lock (self->sinksMutex,
          std::defer_lock);
      std::shared_ptr<MediaType> type;

      retry = !lock.try_lock_for (std::chrono::milliseconds {self->dist (self->rnd) });

      if (retry) {
        continue;
      }

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
            std::unique_lock<std::recursive_timed_mutex> sinkLock (
              it->getSink()->sourcesMutex,
              std::defer_lock);

            retry = !sinkLock.try_lock_for (std::chrono::milliseconds {self->dist (self->rnd) });

            if (retry) {
              continue;
            }

            self->performConnection (it);
          }
        }
      } catch (std::out_of_range) {

      }
    } else {
      std::unique_lock<std::recursive_timed_mutex> lock (self->sourcesMutex,
          std::defer_lock);
      std::shared_ptr<MediaType> type;

      retry = !lock.try_lock_for (std::chrono::milliseconds {self->dist (self->rnd) });

      if (retry) {
        continue;
      }

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
            std::unique_lock<std::recursive_timed_mutex> sourceLock (source->sinksMutex,
                std::defer_lock);

            retry = !sourceLock.try_lock_for (std::chrono::milliseconds {self->dist (self->rnd) });

            if (retry) {
              continue;
            }

            source->performConnection (sourceData);
          }
        }
      } catch (std::out_of_range) {

      }
    }
  } while (retry);
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

  //read default configuration for output bitrate
  try {
    int bitrate = getConfigValue<int, MediaElement> ("outputBitrate");
    GST_DEBUG ("Output bitrate configured to %d bps", bitrate);
    g_object_set (G_OBJECT (element), TARGET_BITRATE, bitrate, NULL);
  } catch (boost::property_tree::ptree_error &e) {
  }

}

MediaElementImpl::~MediaElementImpl ()
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  GST_LOG ("Deleting media element %s", getName().c_str () );

  disconnectAll();

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  gst_element_send_event (element, gst_event_new_eos () );
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
  while (!getSinkConnections().empty() ) {
    std::unique_lock<std::recursive_timed_mutex> sinkLock (sinksMutex,
        std::defer_lock);

    if (!sinkLock.try_lock_for (std::chrono::milliseconds {dist (rnd) }) ) {
      GST_DEBUG_OBJECT (getGstreamerElement(), "Retry disconnect all");
      continue;
    }

    for (std::shared_ptr<ElementConnectionData> connData : getSinkConnections() ) {
      auto sinkImpl = std::dynamic_pointer_cast <MediaElementImpl>
                      (connData->getSink () );
      std::unique_lock<std::recursive_timed_mutex> sinkLock (sinkImpl->sourcesMutex,
          std::defer_lock);

      if (sinkLock.try_lock_for (std::chrono::milliseconds {dist (rnd) }) ) {
        disconnect (connData->getSink (), connData->getType (),
                    connData->getSourceDescription (),
                    connData->getSinkDescription () );
      }
      else {
        GST_DEBUG_OBJECT (sinkImpl->getGstreamerElement(),
                          "Retry disconnect all %" GST_PTR_FORMAT, getGstreamerElement() );
      }
    }
  }

  while (!getSourceConnections().empty() ) {
    std::unique_lock<std::recursive_timed_mutex> sourceLock (sourcesMutex,
        std::defer_lock);

    if (!sourceLock.try_lock_for (std::chrono::milliseconds {dist (rnd) }) ) {
      GST_DEBUG_OBJECT (getGstreamerElement(), "Retry disconnect all");
      continue;
    }

    for (std::shared_ptr<ElementConnectionData> connData :
         getSourceConnections() ) {
      auto sourceImpl = std::dynamic_pointer_cast <MediaElementImpl>
                        (connData->getSource () );
      std::unique_lock<std::recursive_timed_mutex> sourceLock (sourceImpl->sinksMutex,
          std::defer_lock);

      if (sourceLock.try_lock_for (std::chrono::milliseconds {dist (rnd) }) ) {
        connData->getSource ()->disconnect (connData->getSink (),
                                            connData->getType (),
                                            connData->getSourceDescription (),
                                            connData->getSinkDescription () );
      }
      else {
        GST_DEBUG_OBJECT (sourceImpl->getGstreamerElement (),
                          "Retry disconnect all %" GST_PTR_FORMAT, getGstreamerElement() );
      }
    }
  }
}

std::vector<std::shared_ptr<ElementConnectionData>>
    MediaElementImpl::getSourceConnections ()
{
  std::unique_lock<std::recursive_timed_mutex> lock (sourcesMutex);
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
  std::unique_lock<std::recursive_timed_mutex> lock (sourcesMutex);
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
  std::unique_lock<std::recursive_timed_mutex> lock (sourcesMutex);
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
  std::unique_lock<std::recursive_timed_mutex> lock (sinksMutex);
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
  std::unique_lock<std::recursive_timed_mutex> lock (sinksMutex);
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
  std::unique_lock<std::recursive_timed_mutex> lock (sinksMutex);
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
  connect (sink, std::shared_ptr<MediaType> (new MediaType (MediaType::DATA) ),
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

  std::unique_lock<std::recursive_timed_mutex> lock (sinksMutex);
  std::unique_lock<std::recursive_timed_mutex> sinkLock (sinkImpl->sourcesMutex);
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

  sinkLock.unlock();
  lock.unlock ();

  ElementConnected elementConnected (shared_from_this(),
                                     ElementConnected::getName (),
                                     sink, mediaType, sourceMediaDescription,
                                     sinkMediaDescription);
  signalElementConnected (elementConnected);
}

void
MediaElementImpl::performConnection (std::shared_ptr
                                     <ElementConnectionDataInternal> data)
{
  GstPad *src = NULL, *sink = NULL;

  src = data->getSourcePad ();

  if (!src) {
    GST_LOG ("Still waiting for src pad %s:%s", getName().c_str(),
             data->getSourcePadName () );
    return;
  }

  sink = data->getSinkPad ();

  if (sink) {
    GstPadLinkReturn ret;

    GST_LOG ("Linking %s:%s -> %s:%s", getName().c_str(),
             data->getSourcePadName (), data->getSink()->getName().c_str(),
             data->getSinkPadName ().c_str() );

    ret = gst_pad_link_full (src, sink, GST_PAD_LINK_CHECK_NOTHING);

    if (ret != GST_PAD_LINK_OK) {
      GST_WARNING ("Cannot link pads: %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT
                   " reason: %s", src, sink, gst_pad_link_get_name (ret) );
    } else {
      GST_LOG ("Link done");
    }

    g_object_unref (sink);
  } else {
    GST_LOG ("Still waiting for sink pad %s:%s",
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
  disconnect (sink, std::shared_ptr<MediaType> (new MediaType (
                MediaType::DATA) ), "", "");
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
  std::unique_lock<std::recursive_timed_mutex> sinkLock (sinkImpl->sourcesMutex);
  std::unique_lock<std::recursive_timed_mutex> lock (sinksMutex);

  GST_DEBUG ("Disconnecting %s - %s params %s %s %s", getName().c_str(),
             sink->getName ().c_str (), mediaType->getString ().c_str (),
             sourceMediaDescription.c_str(), sinkMediaDescription.c_str() );

  try {
    std::shared_ptr<ElementConnectionDataInternal> connectionData;
    gboolean ret;

    connectionData = sinkImpl->sources.at (mediaType).at (sourceMediaDescription);
    sinkImpl->sources.at (mediaType).erase (sourceMediaDescription);
    sinks.at (mediaType).at (sinkMediaDescription).erase (connectionData);

    g_signal_emit_by_name (getGstreamerElement (), "release-requested-srcpad",
                           connectionData->getSourcePadName (), &ret, NULL);
  } catch (std::out_of_range) {

  }

  sinkLock.unlock();
  lock.unlock ();

  ElementDisconnected elementDisconnected (shared_from_this(),
      ElementDisconnected::getName (),
      sink, mediaType, sourceMediaDescription,
      sinkMediaDescription);
  signalElementDisconnected (elementDisconnected);
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

std::string MediaElementImpl::getGstreamerDot (
  std::shared_ptr<GstreamerDotDetails> details)
{
  switch (details->getValue() ) {
  case GstreamerDotDetails::SHOW_MEDIA_TYPE:
    return generateDotGraph (GST_BIN (element), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE);

  case GstreamerDotDetails::SHOW_CAPS_DETAILS:
    return generateDotGraph (GST_BIN (element), GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS);

  case GstreamerDotDetails::SHOW_NON_DEFAULT_PARAMS:
    return generateDotGraph (GST_BIN (element),
                             GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);

  case GstreamerDotDetails::SHOW_STATES:
    return generateDotGraph (GST_BIN (element), GST_DEBUG_GRAPH_SHOW_STATES);

  case GstreamerDotDetails::SHOW_ALL:
  default:
    return generateDotGraph (GST_BIN (element), GST_DEBUG_GRAPH_SHOW_ALL);
  }
}

std::string MediaElementImpl::getGstreamerDot()
{
  return generateDotGraph (GST_BIN (element), GST_DEBUG_GRAPH_SHOW_ALL);
}

void MediaElementImpl::setOutputBitrate (int bitrate)
{
  g_object_set (G_OBJECT (element), TARGET_BITRATE, bitrate, NULL);
}

std::map <std::string, std::shared_ptr<Stats>>
    MediaElementImpl::generateStats (const gchar *selector)
{
  std::map <std::string, std::shared_ptr<Stats>> statsReport;
  GstStructure *stats;

  g_signal_emit_by_name (getGstreamerElement(), "stats", selector, &stats);

  fillStatsReport (statsReport, stats, time (NULL) );

  gst_structure_free (stats);

  return statsReport;
}

std::map <std::string, std::shared_ptr<Stats>>
    MediaElementImpl::getStats ()
{
  return generateStats (NULL);
}

std::map <std::string, std::shared_ptr<Stats>>
    MediaElementImpl::getStats (std::shared_ptr<MediaType> mediaType)
{
  const gchar *selector = NULL;

  switch (mediaType->getValue () ) {
  case MediaType::AUDIO:
    selector = "audio";
    break;

  case MediaType::VIDEO:
    selector = "video";
    break;

  default:
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Unsupported media type: " + mediaType->getString() );
  }

  return generateStats (selector);
}

void
MediaElementImpl::fillStatsReport (std::map
                                   <std::string, std::shared_ptr<Stats>>
                                   &report, const GstStructure *stats, double timestamp)
{
  std::shared_ptr<Stats> elementStats;
  guint64 input_video, input_audio;
  const GValue *value;

  value = gst_structure_get_value (stats, KMS_MEDIA_ELEMENT_FIELD);

  if (value == NULL) {
    /* No element stats available */
    return;
  }

  if (!GST_VALUE_HOLDS_STRUCTURE (value) ) {
    gchar *str_val;

    str_val = g_strdup_value_contents (value);
    GST_WARNING ("Unexpected field type (%s) = %s", KMS_MEDIA_ELEMENT_FIELD,
                 str_val);
    g_free (str_val);

    return;
  }

  /* Get common element base parameters */
  gst_structure_get (gst_value_get_structure (value), "input-video-latency",
                     G_TYPE_UINT64, &input_video, "input-audio-latency", G_TYPE_UINT64,
                     &input_audio, NULL);

  if (report.find (getId () ) != report.end() ) {
    std::shared_ptr<ElementStats> eStats =
      std::dynamic_pointer_cast <ElementStats> (report[getId ()]);
    eStats->setInputAudioLatency (input_audio);
    eStats->setInputVideoLatency (input_video);
  } else {
    elementStats = std::make_shared <ElementStats> (getId (),
                   std::make_shared <StatsType> (StatsType::element), timestamp,
                   input_audio, input_video);
    report[getId ()] = elementStats;
  }
}

MediaElementImpl::StaticConstructor MediaElementImpl::staticConstructor;

MediaElementImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
