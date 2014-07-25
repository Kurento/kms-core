/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __MEDIA_SERVER_CONFIG_IMPL_HPP__
#define __MEDIA_SERVER_CONFIG_IMPL_HPP__

#include <gst/sdp/gstsdpmessage.h>
#include <string>

namespace kurento
{

class MediaServerConfig
{
public:
  MediaServerConfig () {};

  virtual ~MediaServerConfig() throw () {};

  const GstSDPMessage* getSdpPattern () const {
    return this->sdpPattern;
  };

  void setSdpPattern (GstSDPMessage* sdpPattern){
    this->sdpPattern = sdpPattern;
  };

  const std::string getStunServerAddress () const {
    return this->stunServerAddress;
  };

  void setStunServerAddress (std::string stunServerAddress){
    this->stunServerAddress = stunServerAddress;
  };

  const gint getStunServerPort () const {
    return this->stunServerPort;
  };

  void setStunServerPort (gint stunServerPort){
    this->stunServerPort = stunServerPort;
  };

  const std::string getTurnURL () const {
    return this->turnURL;
  };

  void setTurnURL (std::string turnURL){
    this->turnURL = turnURL;
  };

  const std::string getPemCertificate () const {
    return this->pemCertificate;
  };

  void setPemCertificate (std::string pemCertificate){
    this->pemCertificate = pemCertificate;
  };

  const uint getHttpPort () const {
    return this->httpPort;
  };

  void setHttpPort (uint httpPort){
    this->httpPort = httpPort;
  };

  const std::string getHttpInterface () const {
    return this->httpInterface;
  };

  void setHttpInterface (std::string httpInterface){
    this->httpInterface = httpInterface;
  };

  const std::string getHttpAnnouncedAddr () const {
    return this->httpAnnouncedAddr;
  };

  void setHttpAnnouncedAddr (std::string httpAnnouncedAddr){
    this->httpAnnouncedAddr = httpAnnouncedAddr;
  };

private:

  GstSDPMessage *sdpPattern = NULL;
  std::string stunServerAddress;
  gint stunServerPort;
  std::string turnURL;
  std::string pemCertificate;
  uint httpPort = 0;
  std::string httpInterface;
  std::string httpAnnouncedAddr;
};

} /* kurento */

#endif /* __RTP_CONFIG_IMPL_HPP__ */
