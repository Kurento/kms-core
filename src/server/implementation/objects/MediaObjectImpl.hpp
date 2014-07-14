#ifndef __MEDIA_OBJECT_IMPL_HPP__
#define __MEDIA_OBJECT_IMPL_HPP__

#include <Factory.hpp>
#include "MediaObject.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class MediaPipelineImpl;
class MediaObjectImpl;

void Serialize (std::shared_ptr<MediaObjectImpl> &object, JsonSerializer &serializer);

class MediaObjectImpl : public virtual MediaObject
{

public:

  MediaObjectImpl ();

  virtual ~MediaObjectImpl () {};

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  sigc::signal<void, Error> signalError;

  class Factory : public virtual kurento::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "MediaObject";
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

#endif /*  __MEDIA_OBJECT_IMPL_HPP__ */
