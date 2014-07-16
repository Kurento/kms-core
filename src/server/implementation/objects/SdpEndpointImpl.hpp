#ifndef __SDP_ENDPOINT_IMPL_HPP__
#define __SDP_ENDPOINT_IMPL_HPP__

#include "SessionEndpointImpl.hpp"
#include "SdpEndpoint.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class SdpEndpointImpl;

void Serialize (std::shared_ptr<SdpEndpointImpl> &object, JsonSerializer &serializer);

class SdpEndpointImpl : public SessionEndpointImpl, public virtual SdpEndpoint
{

public:

  SdpEndpointImpl (std::shared_ptr< MediaObjectImpl > parent,
                   const std::string &factoryName);

  virtual ~SdpEndpointImpl () {};

  std::string generateOffer ();
  std::string processOffer (const std::string &offer);
  std::string processAnswer (const std::string &answer);
  std::string getLocalSessionDescriptor ();
  std::string getRemoteSessionDescriptor ();

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

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

#endif /*  __SDP_ENDPOINT_IMPL_HPP__ */
