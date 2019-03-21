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
#include "MediaType.hpp"
#include "MediaLatencyStat.hpp"
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
#include <DotGraph.hpp>
#include <GstreamerDotDetails.hpp>
#include <StatsType.hpp>
#include "ElementStats.hpp"
#include "kmsstats.h"
#include <SignalHandler.hpp>

#include <chrono>
#include <memory>
#include <random>

#define GST_CAT_DEFAULT kurento_media_element_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaElementImpl"

#define KMS_DEFAULT_MEDIA_DESCRIPTION "default"

#define MIN_OUTPUT_BITRATE "min-output-bitrate"
#define MAX_OUTPUT_BITRATE "max-output-bitrate"

#define TYPE_VIDEO "video_"
#define TYPE_AUDIO "audio_"

namespace kurento
{

const static std::string DEFAULT = "default";

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
    this->sourcePadName = nullptr;
    setSinkPadName ();
  }

  ~ElementConnectionDataInternal()
  {
    if (sourcePadName != nullptr) {
      g_free (sourcePadName);
    }
  }

  ElementConnectionDataInternal (std::shared_ptr<ElementConnectionData> data)
  {
    this->source = data->getSource();
    this->sink = data->getSink();
    this->type = data->getType();
    this->sourceDescription = data->getSourceDescription();
    this->sinkDescription = data->getSinkDescription();
    this->sourcePadName = nullptr;
    setSinkPadName ();
  }

  void setSinkPadName()
  {
    std::string desc = "_" + (sinkDescription.empty () ?
                              KMS_DEFAULT_MEDIA_DESCRIPTION : sinkDescription);

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
    if (this->sourcePadName != nullptr) {
      GST_WARNING ("Resetting padName for connection");

      if (this->sourcePadName != padName) {
        g_free (this->sourcePadName);
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
      return nullptr;
    }

    return gst_element_get_static_pad (sinkLocked->getGstreamerElement (),
                                       getSinkPadName ().c_str() );
  }

  GstPad *getSourcePad ()
  {
    std::shared_ptr <MediaElementImpl> sourceLocked = getSource ();

    if (!sourceLocked || sourcePadName == nullptr) {
      return nullptr;
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
processBusMessage (GstBus *bus, GstMessage *msg, MediaElementImpl *self)
{
  GstDebugLevel log_level = GST_LEVEL_NONE;
  GError *err = NULL;
  gchar *dbg_info = NULL;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      log_level = GST_LEVEL_ERROR;
      gst_message_parse_error (msg, &err, &dbg_info);
      break;
    case GST_MESSAGE_WARNING:
      log_level = GST_LEVEL_WARNING;
      gst_message_parse_warning (msg, &err, &dbg_info);
      break;
    default:
      return;
      break;
  }

  GstElement *parent = self->element;
  gint err_code = 0;
  gchar *err_msg = NULL;

  if (!gst_object_has_as_ancestor (msg->src, GST_OBJECT (parent))) {
    return;
  }

  if (err != NULL) {
    err_code = err->code;
    err_msg = err->message;
  }

  GST_CAT_LEVEL_LOG (GST_CAT_DEFAULT, log_level, NULL,
      "Error code %d: '%s', element: %s, parent: %s", err_code,
      (err_msg ? err_msg : "(None)"), GST_MESSAGE_SRC_NAME (msg),
      GST_ELEMENT_NAME (parent));

  GST_CAT_LEVEL_LOG (GST_CAT_DEFAULT, log_level, NULL,
      "Debugging info: %s", (dbg_info ? dbg_info : "(None)"));

  std::string errorMessage (err_msg);
  if (dbg_info) {
    errorMessage += " (" + std::string (dbg_info) + ")";
  }

  try {
    gint code = err_code;
    Error error (self->shared_from_this(), errorMessage, code,
                 "UNEXPECTED_ELEMENT_ERROR");

    self->signalError (error);
  } catch (std::bad_weak_ptr &e) {
  }

  g_error_free (err);
  g_free (dbg_info);

  return;
}

/* https://stackoverflow.com/questions/21237905/how-do-i-generate-thread-safe-uniform-random-numbers/21238187#21238187
 * > Distributions are extremely cheap (they will be completely inlined by the
 * optimiser so that the only remaining overhead is the actual random number
 * rescaling). Don't be afraid to regenerate them as often as you need.
 *
 * The actual random number generator, on the other hand, is a heavy-weight
 * object carrying a lot of state and requiring quite some time to be
 * constructed, so that should only be initialised once per thread (or even
 * across threads, but then you'd need to synchronise access which is more
 * costly in the long run).
 */
static std::chrono::milliseconds
millisRand ()
{
    static thread_local std::mt19937_64 generator;
    std::uniform_int_distribution<int> distribution (1, 100);
    return std::chrono::milliseconds (distribution (generator));
}

void
_media_element_pad_added (GstElement *elem, GstPad *pad, gpointer data)
{
  MediaElementImpl *self = (MediaElementImpl *) data;
  bool retry = false;

  GST_LOG_OBJECT (pad, "Pad added");

  do {
    if (retry) {
      GST_DEBUG_OBJECT (pad, "Retrying connection");
    }

    if (GST_PAD_IS_SRC (pad) ) {
      std::unique_lock<std::recursive_timed_mutex> lock (self->sinksMutex,
          std::defer_lock);
      std::shared_ptr<MediaType> type;

      retry = !lock.try_lock_for (millisRand ());

      if (retry) {
        continue;
      }

      std::string description;

      if (g_str_has_prefix (GST_OBJECT_NAME (pad), "audio_") ) {
        type = std::make_shared<MediaType>(MediaType::AUDIO);
        description = std::string (GST_OBJECT_NAME (pad) + sizeof ("audio_src") );
      } else if (g_str_has_prefix (GST_OBJECT_NAME (pad), "video_") ) {
        type = std::make_shared<MediaType>(MediaType::VIDEO);
        description = std::string (GST_OBJECT_NAME (pad) + sizeof ("video_src") );
      } else {
        type = std::make_shared<MediaType>(MediaType::DATA);
        description = std::string (GST_OBJECT_NAME (pad) + sizeof ("data_src") );
      }

      auto pos = description.find_last_of ("_");
      description.erase (pos);

      try {
        auto connections = self->sinks.at (type).at (description);

        for (auto it : connections) {
          if (g_strcmp0 (GST_OBJECT_NAME (pad), it->getSourcePadName() ) == 0) {
            std::unique_lock<std::recursive_timed_mutex> sinkLock (
              it->getSink()->sourcesMutex,
              std::defer_lock);

            retry = !sinkLock.try_lock_for (millisRand ());

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

      retry = !lock.try_lock_for (millisRand ());

      if (retry) {
        continue;
      }

      std::string description;

      if (g_str_has_prefix (GST_OBJECT_NAME (pad), "sink_audio_") ) {
        type = std::make_shared<MediaType>(MediaType::AUDIO);
        description = std::string (GST_OBJECT_NAME (pad) + sizeof ("sink_audio") );
      } else if (g_str_has_prefix (GST_OBJECT_NAME (pad), "sink_video_") ) {
        type = std::make_shared<MediaType>(MediaType::VIDEO);
        description = std::string (GST_OBJECT_NAME (pad) + sizeof ("sink_video") );
      } else {
        type = std::make_shared<MediaType>(MediaType::DATA);
        description = std::string (GST_OBJECT_NAME (pad) + sizeof ("sink_data") );
      }

      try {
        auto sourceData = self->sources.at (type).at (description);

        auto source = sourceData->getSource();

        if (source) {
          if (g_strcmp0 (GST_OBJECT_NAME (pad),
                         sourceData->getSinkPadName().c_str() ) == 0) {
            std::unique_lock<std::recursive_timed_mutex> sourceLock (source->sinksMutex,
                std::defer_lock);

            retry = !sourceLock.try_lock_for (millisRand ());

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

std::string
padTypeToString (KmsElementPadType type)
{
  switch (type) {
  case KMS_ELEMENT_PAD_TYPE_AUDIO:
    return "audio";

  case KMS_ELEMENT_PAD_TYPE_VIDEO:
    return "video";

  default:
    return "undefined";
  }
}

std::shared_ptr<MediaType>
padTypeToMediaType (KmsElementPadType type)
{
  switch (type) {
  case KMS_ELEMENT_PAD_TYPE_AUDIO:
    return std::make_shared <MediaType> (MediaType::AUDIO);

  case KMS_ELEMENT_PAD_TYPE_VIDEO:
    return std::make_shared <MediaType> (MediaType::VIDEO);

  default:
    break;
  }

  throw KurentoException (UNSUPPORTED_MEDIA_TYPE, "Usupported media type");
}

void
MediaElementImpl::mediaFlowOutStateChange (gboolean isFlowing, gchar *padName,
    KmsElementPadType type)
{
  std::shared_ptr<MediaFlowState> state;
  if (isFlowing) {
    GST_DEBUG_OBJECT (element, "MediaFlowOutStateChange: FLOWING"
        ", pad: '%s', type: '%s'", padName, padTypeToString (type).c_str ());
    state = std::make_shared <MediaFlowState> (MediaFlowState::FLOWING);
  } else {
    GST_DEBUG_OBJECT (element, "MediaFlowOutStateChange: NOT FLOWING"
        ", pad: '%s', type: '%s'", padName, padTypeToString (type).c_str ());
    state = std::make_shared <MediaFlowState> (MediaFlowState::NOT_FLOWING);
  }

  std::string key;
  if (type == KMS_ELEMENT_PAD_TYPE_VIDEO) {
    key = std::string (TYPE_VIDEO) + std::string (padName);
  } else {
    key = std::string (TYPE_AUDIO) + std::string (padName);
  }
  mediaFlowOutStates[key] = state;

  try {
    MediaFlowOutStateChange event (shared_from_this(),
                                   MediaFlowOutStateChange::getName (),
                                   state, padName, padTypeToMediaType (type));
    signalMediaFlowOutStateChange (event);
  } catch (std::bad_weak_ptr &e) {
  }
}

void
MediaElementImpl::mediaFlowInStateChange (gboolean isFlowing, gchar *padName,
    KmsElementPadType type)
{
  std::shared_ptr<MediaFlowState> state;
  if (isFlowing) {
    GST_DEBUG_OBJECT (element, "MediaFlowInStateChange: FLOWING"
        ", pad: '%s', type: '%s'", padName, padTypeToString (type).c_str ());
    state = std::make_shared <MediaFlowState> (MediaFlowState::FLOWING);
  } else {
    GST_DEBUG_OBJECT (element, "MediaFlowInStateChange: NOT FLOWING"
        ", pad: '%s', type: '%s'", padName, padTypeToString (type).c_str ());
    state = std::make_shared <MediaFlowState> (MediaFlowState::NOT_FLOWING);
  }

  std::string key;
  if (type == KMS_ELEMENT_PAD_TYPE_VIDEO) {
    key = std::string (TYPE_VIDEO) + std::string (padName);
  } else {
    key = std::string (TYPE_AUDIO) + std::string (padName);
  }
  mediaFlowInStates[key] = state;

  try {
    MediaFlowInStateChange event (shared_from_this(),
                                  MediaFlowInStateChange::getName (),
                                  state, padName, padTypeToMediaType (type));
    signalMediaFlowInStateChange (event);
  } catch (std::bad_weak_ptr &e) {
  }
}

void
MediaElementImpl::onMediaTranscodingStateChange (gboolean isTranscoding,
    gchar *binName, KmsElementPadType type)
{
  std::shared_ptr<MediaTranscodingState> state;
  if (isTranscoding) {
    GST_DEBUG_OBJECT (element, "MediaTranscodingStateChange: TRANSCODING"
        ", bin: '%s', type: '%s'", binName, padTypeToString (type).c_str ());
    state = std::make_shared <MediaTranscodingState> (
        MediaTranscodingState::TRANSCODING);
  } else {
    GST_DEBUG_OBJECT (element, "MediaTranscodingStateChange: NOT TRANSCODING"
        ", bin: '%s', type: '%s'", binName, padTypeToString (type).c_str ());
    state = std::make_shared <MediaTranscodingState> (
        MediaTranscodingState::NOT_TRANSCODING);
  }

  std::string key;
  if (type == KMS_ELEMENT_PAD_TYPE_VIDEO) {
    key = std::string (TYPE_VIDEO) + std::string (binName);
  } else {
    key = std::string (TYPE_AUDIO) + std::string (binName);
  }
  mediaTranscodingStates[key] = state;

  try {
    MediaTranscodingStateChange event (shared_from_this(),
                                       MediaTranscodingStateChange::getName (),
                                       state, binName, padTypeToMediaType (type));
    signalMediaTranscodingStateChange (event);
  } catch (std::bad_weak_ptr &e) {
    GST_WARNING_OBJECT (element, "Cannot emit event: MediaTranscodingStateChange");
  }
}

void
MediaElementImpl::postConstructor ()
{
  MediaObjectImpl::postConstructor ();

  mediaFlowOutHandler = register_signal_handler (G_OBJECT (element),
                        "flow-out-media",
                        std::function <void (GstElement *, gboolean, gchar *, KmsElementPadType) >
                        (std::bind (&MediaElementImpl::mediaFlowOutStateChange, this,
                                    std::placeholders::_2, std::placeholders::_3, std::placeholders::_4) ),
                        std::dynamic_pointer_cast<MediaElementImpl>
                        (shared_from_this() ) );

  mediaFlowInHandler = register_signal_handler (G_OBJECT (element),
                       "flow-in-media",
                       std::function <void (GstElement *, gboolean, gchar *, KmsElementPadType) >
                       (std::bind (&MediaElementImpl::mediaFlowInStateChange, this,
                                   std::placeholders::_2, std::placeholders::_3, std::placeholders::_4) ),
                       std::dynamic_pointer_cast<MediaElementImpl>
                       (shared_from_this() ) );

  mediaTranscodingHandler = register_signal_handler (G_OBJECT (element),
                       "media-transcoding",
                       std::function <void (GstElement *, gboolean, gchar *, KmsElementPadType) >
                       (std::bind (&MediaElementImpl::onMediaTranscodingStateChange, this,
                                   std::placeholders::_2, std::placeholders::_3, std::placeholders::_4) ),
                       std::dynamic_pointer_cast<MediaElementImpl>
                       (shared_from_this() ) );
}

MediaElementImpl::MediaElementImpl (const boost::property_tree::ptree &config,
                                    std::shared_ptr<MediaObjectImpl> parent,
                                    const std::string &factoryName) : MediaObjectImpl (config, parent)
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  element = gst_element_factory_make(factoryName.c_str(), nullptr);

  if (element == nullptr) {
    throw KurentoException (MEDIA_OBJECT_NOT_AVAILABLE,
                            "Cannot create gstreamer element: " + factoryName);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe->getPipeline () ) );
  handlerId = g_signal_connect (bus, "message",
                                G_CALLBACK (processBusMessage), this);


  padAddedHandlerId = g_signal_connect (element, "pad_added",
                                        G_CALLBACK (_media_element_pad_added), this);

  g_object_ref (element);
  pipe->addElement (element);

  //read default configuration for output bitrate
  int bitrate;
  if (getConfigValue<int, MediaElement> (&bitrate, "outputBitrate")) {
    GST_DEBUG ("Output bitrate configured to %d bps", bitrate);
    g_object_set (G_OBJECT (element), MIN_OUTPUT_BITRATE, bitrate,
                  MAX_OUTPUT_BITRATE, bitrate, NULL);
  }
}

MediaElementImpl::~MediaElementImpl ()
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  GST_LOG ("Deleting media element %s", getName().c_str () );

  if (padAddedHandlerId) {
    g_signal_handler_disconnect (element, padAddedHandlerId);
  }

  if (mediaFlowOutHandler > 0) {
    unregister_signal_handler (element, mediaFlowOutHandler);
  }

  if (mediaFlowInHandler > 0) {
    unregister_signal_handler (element, mediaFlowInHandler);
  }

  if (mediaTranscodingHandler > 0) {
    unregister_signal_handler (element, mediaTranscodingHandler);
  }

  disconnectAll();

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  gst_element_set_locked_state (element, TRUE);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_bin_remove (GST_BIN ( pipe->getPipeline() ), element);

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

    if (!sinkLock.try_lock_for (millisRand ())) {
      GST_DEBUG_OBJECT (getGstreamerElement(), "Retry disconnect all");
      continue;
    }

    for (std::shared_ptr<ElementConnectionData> connData : getSinkConnections() ) {
      auto sinkImpl = std::dynamic_pointer_cast <MediaElementImpl>
                      (connData->getSink () );
      std::unique_lock<std::recursive_timed_mutex> sinkLock (sinkImpl->sourcesMutex,
          std::defer_lock);

      if (sinkLock.try_lock_for (millisRand ())) {
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

    if (!sourceLock.try_lock_for (millisRand ())) {
      GST_DEBUG_OBJECT (getGstreamerElement(), "Retry disconnect all");
      continue;
    }

    for (std::shared_ptr<ElementConnectionData> connData :
         getSourceConnections() ) {
      auto sourceImpl = std::dynamic_pointer_cast <MediaElementImpl>
                        (connData->getSource () );
      std::unique_lock<std::recursive_timed_mutex> sourceLock (sourceImpl->sinksMutex,
          std::defer_lock);

      if (sourceLock.try_lock_for (millisRand ())) {
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
  connect(sink, std::make_shared<MediaType>(MediaType::AUDIO), DEFAULT,
          DEFAULT);
  connect(sink, std::make_shared<MediaType>(MediaType::VIDEO), DEFAULT,
          DEFAULT);
  connect(sink, std::make_shared<MediaType>(MediaType::DATA), DEFAULT, DEFAULT);
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType)
{
  connect (sink, mediaType, DEFAULT, DEFAULT);
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink,
                                std::shared_ptr<MediaType> mediaType,
                                const std::string &sourceMediaDescription)
{
  connect (sink, mediaType, sourceMediaDescription, DEFAULT);
}

void MediaElementImpl::prepareSinkConnection (std::shared_ptr<MediaElement> src,
    std::shared_ptr< MediaType > mediaType,
    const std::string &sourceMediaDescription,
    const std::string &sinkMediaDescription)
{
  /* Do nothing by default */
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
                            "Media elements do not share pipeline");
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

  sinkImpl->prepareSinkConnection (connectionData->getSource(), mediaType,
                                   sourceMediaDescription, sinkMediaDescription);

  type = convertMediaType (mediaType);
  g_signal_emit_by_name (getGstreamerElement (), "request-new-pad", type,
                         sourceMediaDescription.c_str (), GST_PAD_SRC, &padName,
                         NULL);

  if (padName == nullptr) {
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
  GstPad *src = nullptr, *sink = nullptr;

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
  disconnect(sink, std::make_shared<MediaType>(MediaType::AUDIO), DEFAULT,
             DEFAULT);
  disconnect(sink, std::make_shared<MediaType>(MediaType::VIDEO), DEFAULT,
             DEFAULT);
  disconnect(sink, std::make_shared<MediaType>(MediaType::DATA), DEFAULT,
             DEFAULT);
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType)
{
  disconnect (sink, mediaType, DEFAULT, DEFAULT);
}

void MediaElementImpl::disconnect (std::shared_ptr<MediaElement> sink,
                                   std::shared_ptr<MediaType> mediaType,
                                   const std::string &sourceMediaDescription)
{
  disconnect (sink, mediaType, sourceMediaDescription, DEFAULT);
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

    connectionData = sinkImpl->sources.at (mediaType).at (sinkMediaDescription);

    if (connectionData->toInterface()->getSourceDescription() ==
        sourceMediaDescription) {
      sinkImpl->sources.at (mediaType).erase (sinkMediaDescription);
    }

    for (auto conn : sinks.at (mediaType).at (sourceMediaDescription) ) {
      if (conn->toInterface()->getSink() == sink &&
          conn->toInterface()->getSinkDescription() == sinkMediaDescription) {
        sinks.at (mediaType).at (sourceMediaDescription).erase (conn);
        break;
      }
    }

    g_signal_emit_by_name (getGstreamerElement (), "release-requested-pad",
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
  GstCaps *c = nullptr;

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
  GstCaps *c = nullptr;

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
  return generateDotGraph (GST_BIN (element), details);
}

std::string MediaElementImpl::getGstreamerDot()
{
  return generateDotGraph(
      GST_BIN(element),
      std::make_shared<GstreamerDotDetails>(GstreamerDotDetails::SHOW_VERBOSE));
}

void MediaElementImpl::setOutputBitrate (int bitrate)
{
  GST_WARNING ("setOutputBitrate method is deprecated, use properties "
               "minOutputBitrate and maxOutputBitrate");
  g_object_set (G_OBJECT (element), MIN_OUTPUT_BITRATE, bitrate,
                MAX_OUTPUT_BITRATE, bitrate, NULL);
}

int MediaElementImpl::getMinOuputBitrate ()
{
  gint bitrate;

  GST_WARNING ("minOuputBitrate property is deprecated, use property "
               "minOutputBitrate");
  g_object_get (G_OBJECT (element), MIN_OUTPUT_BITRATE, &bitrate, NULL);

  return bitrate;
}

int MediaElementImpl::getMinOutputBitrate ()
{
  gint bitrate;

  g_object_get (G_OBJECT (element), MIN_OUTPUT_BITRATE, &bitrate, NULL);

  return bitrate;
}

void MediaElementImpl::setMinOuputBitrate (int minOuputBitrate)
{
  GST_WARNING ("minOuputBitrate property is deprecated, use property "
               "minOutputBitrate");
  g_object_set (G_OBJECT (element), MIN_OUTPUT_BITRATE, minOuputBitrate,
                NULL);
}

void MediaElementImpl::setMinOutputBitrate (int minOutputBitrate)
{
  g_object_set (G_OBJECT (element), MIN_OUTPUT_BITRATE, minOutputBitrate,
                NULL);
}

int MediaElementImpl::getMaxOuputBitrate ()
{
  gint bitrate;

  GST_WARNING ("maxOuputBitrate property is deprecated, use property "
               "maxOutputBitrate");
  g_object_get (G_OBJECT (element), MAX_OUTPUT_BITRATE, &bitrate, NULL);

  return bitrate;
}

int MediaElementImpl::getMaxOutputBitrate ()
{
  gint bitrate;

  g_object_get (G_OBJECT (element), MAX_OUTPUT_BITRATE, &bitrate, NULL);

  return bitrate;
}

void MediaElementImpl::setMaxOuputBitrate (int maxOuputBitrate)
{
  GST_WARNING ("maxOuputBitrate property is deprecated, use property "
               "maxOutputBitrate");
  g_object_set (G_OBJECT (element), MAX_OUTPUT_BITRATE, maxOuputBitrate,
                NULL);
}

void MediaElementImpl::setMaxOutputBitrate (int maxOutputBitrate)
{
  g_object_set (G_OBJECT (element), MAX_OUTPUT_BITRATE, maxOutputBitrate,
                NULL);
}

std::map <std::string, std::shared_ptr<Stats>>
    MediaElementImpl::generateStats (const gchar *selector)
{
  std::map <std::string, std::shared_ptr<Stats>> statsReport;
  GstStructure *stats;

  g_signal_emit_by_name (getGstreamerElement(), "stats", selector, &stats);

  const auto epoch = std::chrono::high_resolution_clock::now ()
      .time_since_epoch ();
  const int64_t timestampMillis =
      std::chrono::duration_cast<std::chrono::milliseconds> (epoch).count ();

  fillStatsReport(statsReport, stats, time(nullptr), timestampMillis);

  gst_structure_free (stats);

  return statsReport;
}

std::map <std::string, std::shared_ptr<Stats>>
    MediaElementImpl::getStats ()
{
  return generateStats(nullptr);
}

std::map <std::string, std::shared_ptr<Stats>>
    MediaElementImpl::getStats (std::shared_ptr<MediaType> mediaType)
{
  const gchar *selector = nullptr;

  switch (mediaType->getValue () ) {
  case MediaType::AUDIO:
    selector = "audio";
    break;

  case MediaType::VIDEO:
    selector = "video";
    break;

  case MediaType::DATA:
    selector = "data";
    break;

  default:
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Unsupported media type: " + mediaType->getString() );
  }

  return generateStats (selector);
}

static std::shared_ptr<MediaType>
getMediaTypeFromTypeSelector (const gchar *type)
{
  if (g_strcmp0 (type, "audio") == 0) {
    return std::make_shared <MediaType> (MediaType::AUDIO);
  } else if (g_strcmp0 (type, "video") == 0) {
    return std::make_shared <MediaType> (MediaType::VIDEO);
  } else {
    return std::make_shared <MediaType> (MediaType::DATA);
  }
}

void
MediaElementImpl::collectLatencyStats (
  std::vector<std::shared_ptr<MediaLatencyStat>> &latencyStats,
  const GstStructure *stats)
{
  gint i, fields;

  fields = gst_structure_n_fields (stats);

  for (i = 0; i < fields; i ++) {
    const gchar *fieldname;
    const GValue *val;
    gchar *mediaType;
    guint64 avg;

    fieldname = gst_structure_nth_field_name (stats, i);
    val = gst_structure_get_value (stats, fieldname);

    if (!GST_VALUE_HOLDS_STRUCTURE (val) ) {
      GST_DEBUG ("Ignore unexpected value for field %s", fieldname);
      continue;
    }

    gst_structure_get (gst_value_get_structure (val), "type", G_TYPE_STRING,
                       &mediaType, "avg", G_TYPE_UINT64, &avg, NULL);

    std::shared_ptr<MediaType> type = getMediaTypeFromTypeSelector (mediaType);
    std::shared_ptr<MediaLatencyStat> latency =
      std::make_shared <MediaLatencyStat> (fieldname, type, avg);
    g_free (mediaType);

    latencyStats.push_back (latency);
  }
}

static void
setDeprecatedProperties (std::shared_ptr<ElementStats> eStats)
{
  std::vector<std::shared_ptr<MediaLatencyStat>> inStats =
        eStats->getInputLatency();

  for (auto &inStat : inStats) {
    if (inStat->getName() == "sink_audio_default") {
      eStats->setInputAudioLatency(inStat->getAvg());
    } else if (inStat->getName() == "sink_video_default") {
      eStats->setInputVideoLatency(inStat->getAvg());
    }
  }
}

void
MediaElementImpl::fillStatsReport (std::map
                                   <std::string, std::shared_ptr<Stats>>
                                   &report, const GstStructure *stats,
                                   double timestamp, int64_t timestampMillis)
{
  std::shared_ptr<Stats> elementStats;
  GstStructure *latencies;
  const GValue *value;

  value = gst_structure_get_value (stats, KMS_MEDIA_ELEMENT_FIELD);

  if (value == nullptr) {
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

  std::vector<std::shared_ptr<MediaLatencyStat>> inputLatencies;

  if (gst_structure_get (gst_value_get_structure (value), "input-latencies",
                         GST_TYPE_STRUCTURE, &latencies, NULL) ) {
    collectLatencyStats (inputLatencies, latencies);
    gst_structure_free (latencies);
  }

  if (report.find (getId () ) != report.end() ) {
    std::shared_ptr<ElementStats> eStats =
      std::dynamic_pointer_cast <ElementStats> (report[getId ()]);
    eStats->setInputLatency (inputLatencies);
  } else {
    elementStats = std::make_shared <ElementStats> (getId (),
                   std::make_shared <StatsType> (StatsType::element), timestamp,
                   timestampMillis, 0.0, 0.0, inputLatencies);
    report[getId ()] = elementStats;
  }

  setDeprecatedProperties (std::dynamic_pointer_cast <ElementStats>
                           (report[getId ()]) );
}

bool MediaElementImpl::isMediaFlowingIn (std::shared_ptr<MediaType> mediaType)
{
  return isMediaFlowingIn (mediaType, KMS_DEFAULT_MEDIA_DESCRIPTION);
}

bool MediaElementImpl::isMediaFlowingIn (std::shared_ptr<MediaType> mediaType,
    const std::string &sinkMediaDescription)
{
  std::string key;
  gboolean ret = false;

  if (mediaType->getValue () == MediaType::VIDEO) {
    key = std::string (TYPE_VIDEO) + std::string (sinkMediaDescription);
  } else if (mediaType->getValue () == MediaType::AUDIO) {
    key = std::string (TYPE_AUDIO) + std::string (sinkMediaDescription);
  } else {
    GST_ERROR ("Media type DATA is not supported for MediaFlowingIn");
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Media type DATA is not supported for MediaFlowingIn");
  }

  auto it = mediaFlowInStates.find (key);
  if (it != mediaFlowInStates.end()) {
    if (it->second->getValue () == MediaFlowState::FLOWING) {
      ret = true;
    }
  }

  return ret;
}

bool MediaElementImpl::isMediaFlowingOut (std::shared_ptr<MediaType> mediaType)
{
  return isMediaFlowingOut (mediaType, KMS_DEFAULT_MEDIA_DESCRIPTION);
}

bool MediaElementImpl::isMediaFlowingOut (std::shared_ptr<MediaType> mediaType,
    const std::string &sourceMediaDescription)
{
  std::string key;
  gboolean ret = false;

  if (mediaType->getValue () == MediaType::VIDEO) {
    key = std::string (TYPE_VIDEO) + std::string (sourceMediaDescription);
  } else if (mediaType->getValue () == MediaType::AUDIO) {
    key = std::string (TYPE_AUDIO) + std::string (sourceMediaDescription);
  } else {
    GST_ERROR ("Media type DATA is not supported for MediaFlowingOut");
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Media type DATA is not supported for MediaFlowingOut");
  }

  auto it = mediaFlowOutStates.find (key);
  if (it != mediaFlowOutStates.end()) {
    if (it->second->getValue () == MediaFlowState::FLOWING) {
      ret = true;
    }
  }

  return ret;
}

bool MediaElementImpl::isMediaTranscoding (std::shared_ptr<MediaType> mediaType)
{
  return isMediaTranscoding (mediaType, KMS_DEFAULT_MEDIA_DESCRIPTION);
}

bool MediaElementImpl::isMediaTranscoding (std::shared_ptr<MediaType> mediaType,
    const std::string &binName)
{
  gboolean ret = false;

  std::string key;
  if (mediaType->getValue () == MediaType::VIDEO) {
    key = std::string (TYPE_VIDEO) + std::string (binName);
  } else if (mediaType->getValue () == MediaType::AUDIO) {
    key = std::string (TYPE_AUDIO) + std::string (binName);
  } else {
    GST_ERROR ("Media type DATA is not supported for MediaTranscoding");
    throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                            "Media type DATA is not supported for MediaTranscoding");
  }

  auto it = mediaTranscodingStates.find (key);
  if (it != mediaTranscodingStates.end()) {
    if (it->second->getValue () == MediaTranscodingState::TRANSCODING) {
      ret = true;
    }
  }

  return ret;
}

MediaElementImpl::StaticConstructor MediaElementImpl::staticConstructor;

MediaElementImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
