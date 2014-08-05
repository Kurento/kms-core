#include <gst/gst.h>
#include "MediaType.hpp"
#include "MediaSourceImpl.hpp"
#include "MediaSinkImpl.hpp"
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

MediaElementImpl::MediaElementImpl (std::shared_ptr<MediaObjectImpl> parent, const std::string &factoryName) : MediaObjectImpl (parent)
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  element = gst_element_factory_make (factoryName.c_str(), NULL);

  if (element == NULL) {
    throw KurentoException (MEDIA_OBJECT_NOT_AVAILABLE,
                            "Cannot create gstreamer element: " + factoryName);
  }

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
}

std::vector<std::shared_ptr<MediaSource>> MediaElementImpl::getMediaSrcs ()
{
  std::vector<std::shared_ptr<MediaSource>> srcs;

  srcs.push_back (getOrCreateAudioMediaSrc() );
  srcs.push_back (getOrCreateVideoMediaSrc() );

  return srcs;
}

std::vector<std::shared_ptr<MediaSource>> MediaElementImpl::getMediaSrcs (std::shared_ptr<MediaType> mediaType)
{
  std::vector<std::shared_ptr<MediaSource>> srcs;

  if (mediaType->getValue() == MediaType::AUDIO) {
    srcs.push_back (getOrCreateAudioMediaSrc() );
  } else if (mediaType->getValue() == MediaType::VIDEO) {
    srcs.push_back (getOrCreateVideoMediaSrc() );
  }

  return srcs;
}

std::vector<std::shared_ptr<MediaSource>> MediaElementImpl::getMediaSrcs (std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  if (description == "")  {
    return getMediaSrcs (mediaType);
  } else {
    std::vector<std::shared_ptr<MediaSource>> srcs;

    return srcs;
  }
}

std::vector<std::shared_ptr<MediaSink>> MediaElementImpl::getMediaSinks ()
{
  std::vector<std::shared_ptr<MediaSink>> sinks;

  sinks.push_back (getOrCreateAudioMediaSink() );
  sinks.push_back (getOrCreateVideoMediaSink() );

  return sinks;
}

std::vector<std::shared_ptr<MediaSink>> MediaElementImpl::getMediaSinks (std::shared_ptr<MediaType> mediaType)
{
  std::vector<std::shared_ptr<MediaSink>> sinks;

  if (mediaType->getValue() == MediaType::AUDIO) {
    sinks.push_back (getOrCreateAudioMediaSink() );
  } else if (mediaType->getValue() == MediaType::VIDEO) {
    sinks.push_back (getOrCreateVideoMediaSink() );
  }

  return sinks;
}

std::vector<std::shared_ptr<MediaSink>> MediaElementImpl::getMediaSinks (std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  if (description == "")  {
    return getMediaSinks (mediaType);
  } else {
    std::vector<std::shared_ptr<MediaSink>> sinks;

    return sinks;
  }
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink)
{
  std::shared_ptr<MediaElementImpl> sinkImpl =
    std::dynamic_pointer_cast<MediaElementImpl> (sink);

  std::shared_ptr<MediaSource> audio_src = getOrCreateAudioMediaSrc();
  std::shared_ptr<MediaSink> audio_sink = sinkImpl->getOrCreateAudioMediaSink();

  std::shared_ptr<MediaSource> video_src = getOrCreateVideoMediaSrc();
  std::shared_ptr<MediaSink> video_sink = sinkImpl->getOrCreateVideoMediaSink();

  audio_src->connect (audio_sink);

  try {
    video_src->connect (video_sink);
  } catch (...) {
    try {
      audio_sink->disconnect (audio_src);
    } catch (...) {
    }

    throw;
  }
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink, std::shared_ptr<MediaType> mediaType)
{
  std::shared_ptr<MediaElementImpl> sinkImpl =
    std::dynamic_pointer_cast<MediaElementImpl> (sink);

  if (mediaType->getValue() == MediaType::AUDIO) {
    std::shared_ptr<MediaSource> audio_src = getOrCreateAudioMediaSrc();
    std::shared_ptr<MediaSink> audio_sink = sinkImpl->getOrCreateAudioMediaSink();

    audio_src->connect (audio_sink);
  } else if (mediaType->getValue() == MediaType::VIDEO) {
    std::shared_ptr<MediaSource> video_src = getOrCreateVideoMediaSrc();
    std::shared_ptr<MediaSink> video_sink = sinkImpl->getOrCreateVideoMediaSink();

    video_src->connect (video_sink);
  }
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink, std::shared_ptr<MediaType> mediaType, const std::string &mediaDescription)
{
  if (mediaDescription == "") {
    connect (sink, mediaType);
  }
}

/*Internal utilities methods*/

std::shared_ptr<MediaSourceImpl>
MediaElementImpl::getOrCreateAudioMediaSrc()
{
  std::unique_lock<std::recursive_mutex> lock (mutex);

  std::shared_ptr<MediaSourceImpl> src;

  try {
    src = audioMediaSrc.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if (src.get() == NULL) {
    std::shared_ptr<MediaType> mediaType (new MediaType (MediaType::AUDIO) );

    MediaSourceImpl *srcPtr = new  MediaSourceImpl (mediaType, "",
        std::dynamic_pointer_cast <MediaElementImpl> (shared_from_this() ) );

    src = std::dynamic_pointer_cast<MediaSourceImpl>
          (MediaSet::getMediaSet()->ref (srcPtr) );

    audioMediaSrc = std::weak_ptr<MediaSourceImpl> (src);
  }

  return src;
}

std::shared_ptr<MediaSourceImpl>
MediaElementImpl::getOrCreateVideoMediaSrc()
{
  std::unique_lock<std::recursive_mutex> lock (mutex);

  std::shared_ptr<MediaSourceImpl> src;

  try {
    src = videoMediaSrc.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if (src.get() == NULL) {
    std::shared_ptr<MediaType> mediaType (new MediaType (MediaType::VIDEO) );

    MediaSourceImpl *srcPtr = new  MediaSourceImpl (mediaType, "",
        std::dynamic_pointer_cast <MediaElementImpl> (shared_from_this() ) );

    src = std::dynamic_pointer_cast<MediaSourceImpl>
          (MediaSet::getMediaSet()->ref (srcPtr) );

    videoMediaSrc = std::weak_ptr<MediaSourceImpl> (src);
  }

  return src;
}

std::shared_ptr<MediaSinkImpl>
MediaElementImpl::getOrCreateAudioMediaSink()
{
  std::unique_lock<std::recursive_mutex> lock (mutex);

  std::shared_ptr<MediaSinkImpl> sink;

  try {
    sink = audioMediaSink.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if (sink.get() == NULL) {
    std::shared_ptr<MediaType> mediaType (new MediaType (MediaType::AUDIO) );

    MediaSinkImpl *sinkPtr = new  MediaSinkImpl (mediaType, "",
        std::dynamic_pointer_cast <MediaElementImpl> (shared_from_this() ) );

    sink = std::dynamic_pointer_cast<MediaSinkImpl>
           (MediaSet::getMediaSet()->ref (sinkPtr) );

    audioMediaSink = std::weak_ptr<MediaSinkImpl> (sink);
  }

  return sink;
}

std::shared_ptr<MediaSinkImpl>
MediaElementImpl::getOrCreateVideoMediaSink()
{
  std::unique_lock<std::recursive_mutex> lock (mutex);

  std::shared_ptr<MediaSinkImpl> sink;

  try {
    sink = videoMediaSink.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if (sink.get() == NULL) {
    std::shared_ptr<MediaType> mediaType (new MediaType (MediaType::VIDEO) );

    MediaSinkImpl *sinkPtr = new  MediaSinkImpl (mediaType, "",
        std::dynamic_pointer_cast <MediaElementImpl> (shared_from_this() ) );

    sink = std::dynamic_pointer_cast<MediaSinkImpl>
           (MediaSet::getMediaSet()->ref (sinkPtr) );

    videoMediaSink = std::weak_ptr<MediaSinkImpl> (sink);
  }

  return sink;
}

MediaElementImpl::StaticConstructor MediaElementImpl::staticConstructor;

MediaElementImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
