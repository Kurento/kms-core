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
  MediaServerConfig (GstSDPMessage *sdpPattern, std::string stunServerAddress,
             gint stunServerPort, std::string turnURL,
             std::string pemCertificate, uint httpPort, std::string httpInterface,
             std::string httpAnnouncedAddr)
  {
    this->sdpPattern = sdpPattern;
    this->stunServerAddress = stunServerAddress;
    this->stunServerPort = stunServerPort;
    this->turnURL = turnURL;
    this->pemCertificate = pemCertificate;
    this->httpPort = httpPort;
    this->httpInterface = httpInterface;
    this->httpAnnouncedAddr = httpAnnouncedAddr;
  };

  virtual ~MediaServerConfig() throw () {};

  GstSDPMessage* getSdpPattern () {
    return this->sdpPattern;
  };

  std::string getStunServerAddress (){
    return this->stunServerAddress;
  };

  gint getStunServerPort (){
    return this->stunServerPort;
  };

  std::string getTurnURL (){
    return this->turnURL;
  };

  std::string getPemCertificate (){
    return this->pemCertificate;
  };

  uint getHttpPort (){
    return this->httpPort;
  };

  std::string getHttpInterface (){
    return this->httpInterface;
  };

  std::string getHttpAnnouncedAddr (){
    return this->httpAnnouncedAddr;
  };

private:

  GstSDPMessage *sdpPattern;
  std::string stunServerAddress;
  gint stunServerPort;
  std::string turnURL;
  std::string pemCertificate;
  uint httpPort;
  std::string httpInterface;
  std::string httpAnnouncedAddr;
};

} /* kurento */

#endif /* __RTP_CONFIG_IMPL_HPP__ */
