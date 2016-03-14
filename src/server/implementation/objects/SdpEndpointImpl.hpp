#ifndef __SDP_ENDPOINT_IMPL_HPP__
#define __SDP_ENDPOINT_IMPL_HPP__

#include "SessionEndpointImpl.hpp"
#include "SdpEndpoint.hpp"
#include <EventHandler.hpp>
#include <atomic>

namespace kurento
{

class SdpEndpointImpl;

void Serialize (std::shared_ptr<SdpEndpointImpl> &object,
                JsonSerializer &serializer);

class SdpEndpointImpl : public SessionEndpointImpl, public virtual SdpEndpoint
{

public:

  SdpEndpointImpl (const boost::property_tree::ptree &config,
                   std::shared_ptr< MediaObjectImpl > parent,
                   const std::string &factoryName, bool useIpv6 = false);

  virtual ~SdpEndpointImpl () {};

  int getMaxVideoRecvBandwidth () override;
  void setMaxVideoRecvBandwidth (int maxVideoRecvBandwidth) override;
  int getMaxAudioRecvBandwidth () override;
  void setMaxAudioRecvBandwidth (int maxAudioRecvBandwidth) override;
  std::string generateOffer () override;
  std::string processOffer (const std::string &offer) override;
  std::string processAnswer (const std::string &answer) override;
  std::string getLocalSessionDescriptor () override;
  std::string getRemoteSessionDescriptor () override;

  /* Next methods are automatically implemented by code generator */
  using SessionEndpointImpl::connect;
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler) override;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response) override;

  virtual void Serialize (JsonSerializer &serializer) override;

protected:

  virtual void postConstructor () override;
  std::string sessId;

private:

  static std::mutex sdpMutex;
  std::atomic_bool offerInProcess;
  std::atomic_bool waitingAnswer;
  std::atomic_bool answerProcessed;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __SDP_ENDPOINT_IMPL_HPP__ */
