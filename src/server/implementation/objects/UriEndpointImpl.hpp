#ifndef __URI_ENDPOINT_IMPL_HPP__
#define __URI_ENDPOINT_IMPL_HPP__

#include "EndpointImpl.hpp"
#include "UriEndpoint.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class UriEndpointImpl;

void Serialize (std::shared_ptr<UriEndpointImpl> &object,
                JsonSerializer &serializer);

class UriEndpointImpl : public EndpointImpl, public virtual UriEndpoint
{

public:

  UriEndpointImpl (const boost::property_tree::ptree &config,
                   std::shared_ptr< MediaObjectImpl > parent,
                   const std::string &factoryName, const std::string &uri);

  virtual ~UriEndpointImpl () {};

  void pause ();
  void stop ();

  virtual std::string getUri ();
  void checkUri ();

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:

  void start ();

private:

  std::string uri;
  std::string absolute_uri;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __URI_ENDPOINT_IMPL_HPP__ */
