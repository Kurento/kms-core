#ifndef __MEDIA_PIPELINE_IMPL_HPP__
#define __MEDIA_PIPELINE_IMPL_HPP__

#include "MediaObjectImpl.hpp"
#include "MediaPipeline.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class MediaPipelineImpl;

void Serialize (std::shared_ptr<MediaPipelineImpl> &object, JsonSerializer &serializer);

class MediaPipelineImpl : public MediaObjectImpl, public virtual MediaPipeline
{

public:

  MediaPipelineImpl ();

  virtual ~MediaPipelineImpl () {};

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual MediaObjectImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "MediaPipeline";
    };

  private:

    virtual MediaObjectImpl *createObjectPointer (const Json::Value &params) const;

    MediaObjectImpl *createObject () const;
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

#endif /*  __MEDIA_PIPELINE_IMPL_HPP__ */
