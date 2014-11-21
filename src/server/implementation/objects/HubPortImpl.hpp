#ifndef __HUB_PORT_IMPL_HPP__
#define __HUB_PORT_IMPL_HPP__

#include "MediaElementImpl.hpp"
#include "HubPort.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class HubImpl;
class HubPortImpl;

void Serialize (std::shared_ptr<HubPortImpl> &object,
                JsonSerializer &serializer);

class HubPortImpl : public MediaElementImpl, public virtual HubPort
{

public:

  HubPortImpl (const boost::property_tree::ptree &config,
               std::shared_ptr<HubImpl> hub);

  virtual ~HubPortImpl ();

  int getHandlerId ()
  {
    return handlerId;
  }

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

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
