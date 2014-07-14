#ifndef __URI_ENDPOINT_IMPL_HPP__
#define __URI_ENDPOINT_IMPL_HPP__

#include "EndpointImpl.hpp"
#include "UriEndpoint.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class UriEndpointImpl;

void Serialize (std::shared_ptr<UriEndpointImpl> &object, JsonSerializer &serializer);

class UriEndpointImpl : public EndpointImpl, public virtual UriEndpoint
{

public:

  UriEndpointImpl ();

  virtual ~UriEndpointImpl () {};

  std::string getUri ();
  void pause ();
  void stop ();

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual EndpointImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "UriEndpoint";
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

#endif /*  __URI_ENDPOINT_IMPL_HPP__ */
