#ifndef __HUB_PORT_IMPL_HPP__
#define __HUB_PORT_IMPL_HPP__

#include "MediaElementImpl.hpp"
#include "HubPort.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class HubImpl;
class HubPortImpl;

void Serialize (std::shared_ptr<HubPortImpl> &object, JsonSerializer &serializer);

class HubPortImpl : public MediaElementImpl, public virtual HubPort
{

public:

  HubPortImpl (std::shared_ptr<HubImpl> hub);

  virtual ~HubPortImpl ();

  int getHandlerId () {
    return handlerId;
  }

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual MediaElementImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "HubPort";
    };

  private:

    virtual MediaObjectImpl *createObjectPointer (const Json::Value &params) const;

    MediaObjectImpl *createObject (std::shared_ptr<Hub> hub) const;
  };

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  int handlerId;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __HUB_PORT_IMPL_HPP__ */
