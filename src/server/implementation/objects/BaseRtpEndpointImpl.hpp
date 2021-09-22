/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __BASE_RTP_ENDPOINT_IMPL_HPP__
#define __BASE_RTP_ENDPOINT_IMPL_HPP__

#include "SdpEndpointImpl.hpp"
#include "BaseRtpEndpoint.hpp"
#include <EventHandler.hpp>
#include <boost/property_tree/ptree.hpp>

namespace kurento
{
class BaseRtpEndpointImpl;
} /* kurento */

namespace kurento
{
void Serialize (std::shared_ptr<kurento::BaseRtpEndpointImpl> &object,
                JsonSerializer &serializer);
} /* kurento */

namespace kurento
{

class BaseRtpEndpointImpl : public SdpEndpointImpl,
  public virtual BaseRtpEndpoint
{

public:

  BaseRtpEndpointImpl (const boost::property_tree::ptree &config,
                       std::shared_ptr< MediaObjectImpl > parent,
                       const std::string &factoryName, bool useIpv6 = false);

  virtual ~BaseRtpEndpointImpl ();

  virtual int getMinVideoRecvBandwidth ();
  virtual void setMinVideoRecvBandwidth (int minVideoRecvBandwidth);

  virtual int getMinVideoSendBandwidth ();
  virtual void setMinVideoSendBandwidth (int minVideoSendBandwidth);

  virtual int getMaxVideoSendBandwidth ();
  virtual void setMaxVideoSendBandwidth (int maxVideoSendBandwidth);

  virtual std::shared_ptr<MediaState> getMediaState ();
  virtual std::shared_ptr<ConnectionState> getConnectionState ();

  virtual std::shared_ptr<RembParams> getRembParams ();
  virtual void setRembParams (std::shared_ptr<RembParams> rembParams);

  virtual int getMtu ();
  virtual void setMtu (int mtu);

  virtual std::string getExternalIPv4 ();
  virtual void setExternalIPv4 (const std::string &externalIPv4);

  virtual std::string getExternalIPv6 ();
  virtual void setExternalIPv6 (const std::string &externalIPv6);

  sigc::signal<void, MediaStateChanged> signalMediaStateChanged;
  sigc::signal<void, ConnectionStateChanged> signalConnectionStateChanged;

  /* Next methods are automatically implemented by code generator */
  using SdpEndpointImpl::connect;
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);
  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:
  virtual void postConstructor ();
  virtual void fillStatsReport (std::map <std::string, std::shared_ptr<Stats>>
                                &report, const GstStructure *stats,
                                double timestamp, int64_t timestampMillis);

private:

  std::string formatGstStructure (const GstStructure *stats);
  std::shared_ptr<MediaState> current_media_state;
  gulong mediaStateChangedHandlerId;
  std::shared_ptr<ConnectionState> current_conn_state;
  gulong connStateChangedHandlerId;
  std::recursive_mutex mutex;

  void updateMediaState (guint new_state);
  void updateConnectionState (gchar *sessId, guint new_state);

  void collectEndpointStats (std::map <std::string, std::shared_ptr<Stats>>
                             &statsReport, std::string id, const GstStructure *stats,
                             double timestamp, int64_t timestampMillis);
  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __BASE_RTP_ENDPOINT_IMPL_HPP__ */
