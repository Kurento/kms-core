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

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE SdpEndpoint
#include <boost/test/unit_test.hpp>
#include <MediaPipelineImpl.hpp>
#include <MediaElementImpl.hpp>
#include <ElementConnectionData.hpp>
#include <MediaType.hpp>
#include <KurentoException.hpp>
#include <objects/SdpEndpointImpl.hpp>

using namespace kurento;

BOOST_AUTO_TEST_CASE (duplicate_offer)
{
  gst_init (NULL, NULL);
  std::shared_ptr <MediaPipelineImpl> pipe (new MediaPipelineImpl (
        boost::property_tree::ptree() ) );
  boost::property_tree::ptree config;
  std::string offer;

  config.add ("configPath", "../../../tests" );
  config.add ("modules.kurento.SdpEndpoint.sdpPattern", "sdp_pattern.txt");

  std::shared_ptr <SdpEndpointImpl> sdpEndpoint ( new  SdpEndpointImpl
      (config, pipe, "dummysdp") );

  offer = sdpEndpoint->generateOffer ();

  try {
    offer = sdpEndpoint->generateOffer ();
    BOOST_ERROR ("Duplicate offer not detected");
  } catch (KurentoException &e) {
    if (e.getCode () != 40208) {
      BOOST_ERROR ("Duplicate offer not detected");
    }
  }

  sdpEndpoint.reset ();
}
