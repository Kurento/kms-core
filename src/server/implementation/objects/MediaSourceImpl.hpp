#ifndef __MEDIA_SOURCE_IMPL_HPP__
#define __MEDIA_SOURCE_IMPL_HPP__

#include "MediaPadImpl.hpp"
#include "MediaSource.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class MediaSinkImpl;
class MediaSourceImpl;

void Serialize (std::shared_ptr<MediaSourceImpl> &object, JsonSerializer &serializer);

class MediaSourceImpl : public MediaPadImpl, public virtual MediaSource
{

public:

  MediaSourceImpl ();

  virtual ~MediaSourceImpl () {};

  std::vector<std::shared_ptr<MediaSink>> getConnectedSinks ();
  void connect (std::shared_ptr<MediaSink> sink);

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual MediaPadImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "MediaSource";
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

#endif /*  __MEDIA_SOURCE_IMPL_HPP__ */
