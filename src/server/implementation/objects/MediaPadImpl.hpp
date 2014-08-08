#ifndef __MEDIA_PAD_IMPL_HPP__
#define __MEDIA_PAD_IMPL_HPP__

#include "MediaObjectImpl.hpp"
#include "MediaPad.hpp"
#include <EventHandler.hpp>
#include <gst/gst.h>

namespace kurento
{

class MediaElementImpl;
class MediaType;
class MediaPadImpl;

void Serialize (std::shared_ptr<MediaPadImpl> &object, JsonSerializer &serializer);

class MediaPadImpl : public MediaObjectImpl, public virtual MediaPad
{

public:

  MediaPadImpl (const boost::property_tree::ptree &config,
                std::shared_ptr<MediaObjectImpl> parent,
                std::shared_ptr<MediaType> mediaType,
                const std::string &mediaDescription);

  virtual ~MediaPadImpl () {};

  virtual std::shared_ptr<MediaElement> getMediaElement () {
    return mediaElement;
  }

  virtual std::shared_ptr<MediaType> getMediaType () {
    return mediaType;
  }

  virtual std::string getMediaDescription () {
    return mediaDescription;
  }

  GstElement *getGstreamerElement ();

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  std::shared_ptr<MediaElement> mediaElement;
  std::shared_ptr<MediaType> mediaType;
  std::string mediaDescription;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __MEDIA_PAD_IMPL_HPP__ */
