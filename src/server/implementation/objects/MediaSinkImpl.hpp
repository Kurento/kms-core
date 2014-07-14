#ifndef __MEDIA_SINK_IMPL_HPP__
#define __MEDIA_SINK_IMPL_HPP__

#include "MediaPadImpl.hpp"
#include "MediaSink.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class MediaSourceImpl;
class MediaSinkImpl;

void Serialize (std::shared_ptr<MediaSinkImpl> &object, JsonSerializer &serializer);

class MediaSinkImpl : public MediaPadImpl, public virtual MediaSink
{

public:

  MediaSinkImpl ();

  virtual ~MediaSinkImpl () {};

  void disconnect (std::shared_ptr<MediaSource> src);
  std::shared_ptr<MediaSource> getConnectedSrc ();

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual MediaPadImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "MediaSink";
    };

  };

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __MEDIA_SINK_IMPL_HPP__ */
