#ifndef __HUB_IMPL_HPP__
#define __HUB_IMPL_HPP__

#include "MediaObjectImpl.hpp"
#include "Hub.hpp"
#include <EventHandler.hpp>
#include <gst/gst.h>

namespace kurento
{

class HubImpl;

void Serialize (std::shared_ptr<HubImpl> &object, JsonSerializer &serializer);

class HubImpl : public MediaObjectImpl, public virtual Hub
{

public:

  HubImpl (std::shared_ptr<MediaObjectImpl> parent,
           const std::string &factoryName);

  virtual ~HubImpl ();

  GstElement *getGstreamerElement() {
    return element;
  };

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual MediaObjectImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "Hub";
    };

  };

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:
  GstElement *element;

private:

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __HUB_IMPL_HPP__ */
