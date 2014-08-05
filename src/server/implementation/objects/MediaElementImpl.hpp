#ifndef __MEDIA_ELEMENT_IMPL_HPP__
#define __MEDIA_ELEMENT_IMPL_HPP__

#include "MediaObjectImpl.hpp"
#include "MediaElement.hpp"
#include <EventHandler.hpp>
#include <gst/gst.h>
#include <mutex>

namespace kurento
{

class MediaType;
class MediaSourceImpl;
class MediaSinkImpl;
class MediaElementImpl;

void Serialize (std::shared_ptr<MediaElementImpl> &object, JsonSerializer &serializer);

class MediaElementImpl : public MediaObjectImpl, public virtual MediaElement
{

public:

  MediaElementImpl (std::shared_ptr<MediaObjectImpl> parent, const std::string &factoryName);

  virtual ~MediaElementImpl ();

  GstElement *getGstreamerElement() {
    return element;
  };

  std::vector<std::shared_ptr<MediaSource>> getMediaSrcs ();
  std::vector<std::shared_ptr<MediaSource>> getMediaSrcs (std::shared_ptr<MediaType> mediaType);
  std::vector<std::shared_ptr<MediaSource>> getMediaSrcs (std::shared_ptr<MediaType> mediaType, const std::string &description);
  std::vector<std::shared_ptr<MediaSink>> getMediaSinks ();
  std::vector<std::shared_ptr<MediaSink>> getMediaSinks (std::shared_ptr<MediaType> mediaType);
  std::vector<std::shared_ptr<MediaSink>> getMediaSinks (std::shared_ptr<MediaType> mediaType, const std::string &description);
  void connect (std::shared_ptr<MediaElement> sink);
  void connect (std::shared_ptr<MediaElement> sink, std::shared_ptr<MediaType> mediaType);
  void connect (std::shared_ptr<MediaElement> sink, std::shared_ptr<MediaType> mediaType, const std::string &mediaDescription);

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:
  GstElement *element;

private:
  std::recursive_mutex mutex;

  std::weak_ptr<MediaSourceImpl> audioMediaSrc;
  std::weak_ptr<MediaSourceImpl> videoMediaSrc;
  std::weak_ptr<MediaSinkImpl> audioMediaSink;
  std::weak_ptr<MediaSinkImpl> videoMediaSink;

  std::shared_ptr<MediaSourceImpl> getOrCreateAudioMediaSrc();
  std::shared_ptr<MediaSourceImpl> getOrCreateVideoMediaSrc();
  std::shared_ptr<MediaSinkImpl> getOrCreateAudioMediaSink();
  std::shared_ptr<MediaSinkImpl> getOrCreateVideoMediaSink();

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __MEDIA_ELEMENT_IMPL_HPP__ */
