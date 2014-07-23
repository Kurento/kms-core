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

  MediaObjectImpl (std::shared_ptr <MediaObject> parent);

  virtual ~MediaObjectImpl () {};

  virtual std::shared_ptr<MediaPipeline> getMediaPipeline ();

  virtual std::shared_ptr<MediaObject> getParent () {
    return parent;
  }

  virtual std::string getId () {
    return id;
  }

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  sigc::signal<void, Error> signalError;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  std::string id;
  std::shared_ptr<MediaObject> parent;

  std::string createId();

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __MEDIA_OBJECT_IMPL_HPP__ */
