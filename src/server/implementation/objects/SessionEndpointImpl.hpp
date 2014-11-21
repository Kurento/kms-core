#ifndef __SESSION_ENDPOINT_IMPL_HPP__
#define __SESSION_ENDPOINT_IMPL_HPP__

#include "EndpointImpl.hpp"
#include "SessionEndpoint.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class SessionEndpointImpl;

void Serialize (std::shared_ptr<SessionEndpointImpl> &object,
                JsonSerializer &serializer);

class SessionEndpointImpl : public EndpointImpl, public virtual SessionEndpoint
{

public:

  SessionEndpointImpl (const boost::property_tree::ptree &config,
                       std::shared_ptr< MediaObjectImpl > parent,
                       const std::string &factoryName);

  virtual ~SessionEndpointImpl () {};

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  sigc::signal<void, MediaSessionTerminated> signalMediaSessionTerminated;
  sigc::signal<void, MediaSessionStarted> signalMediaSessionStarted;

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

#endif /*  __SESSION_ENDPOINT_IMPL_HPP__ */
