#ifndef __ENDPOINT_IMPL_HPP__
#define __ENDPOINT_IMPL_HPP__

#include "MediaElementImpl.hpp"
#include "Endpoint.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class EndpointImpl;

void Serialize (std::shared_ptr<EndpointImpl> &object,
                JsonSerializer &serializer);

class EndpointImpl : public MediaElementImpl, public virtual Endpoint
{

public:

  EndpointImpl (const boost::property_tree::ptree &config,
                std::shared_ptr< MediaObjectImpl > parent,
                const std::string &factoryName);

  virtual ~EndpointImpl () {};

  /* Next methods are automatically implemented by code generator */
  using MediaElementImpl::connect;
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler) override;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response) override;

  virtual void Serialize (JsonSerializer &serializer) override;

private:

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __ENDPOINT_IMPL_HPP__ */
