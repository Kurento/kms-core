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
#include <MediaSet.hpp>
#include <ModuleManager.hpp>

using namespace kurento;

std::string mediaPipelineId;
ModuleManager moduleManager;
boost::property_tree::ptree config;

struct GF {
  GF();
  ~GF();
};

class SdpEndpointFactory : public Factory
{
public:
  std::string getName() const
  {
    return "SdpEndpointFactory";
  }

protected:
  MediaObjectImpl *createObjectPointer (const boost::property_tree::ptree
                                        &conf, const Json::Value &params) const
  {
    std::string mediaPipelineId = params["mediaPipeline"].asString ();
    std::shared_ptr <MediaObjectImpl> pipe =
      MediaSet::getMediaSet ()->getMediaObject (mediaPipelineId);

    return new  SdpEndpointImpl (config, pipe, "dummysdp");
  }
};

BOOST_GLOBAL_FIXTURE (GF)

GF::GF()
{
  gst_init (NULL, NULL);
  moduleManager.loadModulesFromDirectories ("../../src/server");
}

GF::~GF()
{
  MediaSet::deleteMediaSet();
}

static void
releaseMediaObject (const std::string &id)
{
  MediaSet::getMediaSet ()->release (id);
}


static std::shared_ptr <SdpEndpointImpl> generateSdpEndpoint (
  const std::string &mediaPipelineId)
{
  SdpEndpointFactory factory;
  Json::Value params;

  params["mediaPipeline"] = mediaPipelineId;

  return std::dynamic_pointer_cast<SdpEndpointImpl> (
           factory.createObject (config, "", params) );
}

BOOST_AUTO_TEST_CASE (duplicate_offer)
{
  mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();

  std::string offer;

  config.add ("modules.kurento.SdpEndpoint.configPath", "../../../tests" );
  config.add ("modules.kurento.SdpEndpoint.numAudioMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.numVideoMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.audioCodecs", "[]");
  config.add ("modules.kurento.SdpEndpoint.videoCodecs", "[]");

  std::shared_ptr <SdpEndpointImpl> sdpEndpoint = generateSdpEndpoint (
        mediaPipelineId);

  offer = sdpEndpoint->generateOffer ();

  try {
    offer = sdpEndpoint->generateOffer ();
    BOOST_ERROR ("Duplicate offer not detected");
  } catch (KurentoException &e) {
    if (e.getCode () != 40208) {
      BOOST_ERROR ("Duplicate offer not detected");
    }
  }

  releaseMediaObject (sdpEndpoint->getId() );
  releaseMediaObject (mediaPipelineId);

  sdpEndpoint.reset ();
}

BOOST_AUTO_TEST_CASE (process_answer_without_offer)
{
  mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();

  std::string answer;

  config.add ("configPath", "../../../tests" );
  config.add ("modules.kurento.SdpEndpoint.numAudioMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.numVideoMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.audioCodecs", "[]");
  config.add ("modules.kurento.SdpEndpoint.videoCodecs", "[]");

  std::shared_ptr <SdpEndpointImpl> sdpEndpoint = generateSdpEndpoint (
        mediaPipelineId);

  try {
    sdpEndpoint->processAnswer (answer);
    BOOST_ERROR ("Process answer without generating offer does not detected");
  } catch (KurentoException &e) {
    if (e.getCode () != 40209) {
      BOOST_ERROR ("Process answer without generating offer does not detected");
    }
  }

  releaseMediaObject (sdpEndpoint->getId() );
  releaseMediaObject (mediaPipelineId);

  sdpEndpoint.reset ();
}

BOOST_AUTO_TEST_CASE (duplicate_answer)
{
  mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();

  std::string offer;

  config.add ("configPath", "../../../tests" );
  config.add ("modules.kurento.SdpEndpoint.numAudioMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.numVideoMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.audioCodecs", "[]");
  config.add ("modules.kurento.SdpEndpoint.videoCodecs", "[]");

  std::shared_ptr <SdpEndpointImpl> sdpEndpoint = generateSdpEndpoint (
        mediaPipelineId);

  offer = sdpEndpoint->generateOffer ();
  sdpEndpoint->processAnswer (offer);

  try {
    sdpEndpoint->processAnswer (offer);
    BOOST_ERROR ("Duplicate answer not detected");
  } catch (KurentoException &e) {
    if (e.getCode () != 40210) {
      BOOST_ERROR ("Duplicate answer not detected");
    }
  }

  releaseMediaObject (sdpEndpoint->getId() );
  releaseMediaObject (mediaPipelineId);

  sdpEndpoint.reset ();
}

BOOST_AUTO_TEST_CASE (codec_parsing)
{
  boost::property_tree::ptree ac, audioCodecs, vc, videoCodecs;

  mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();

  std::string offer;

  config.add ("modules.kurento.SdpEndpoint.numAudioMedias", 1);
  config.add ("modules.kurento.SdpEndpoint.numVideoMedias", 1);
  config.add ("configPath", "../../../tests" );

  ac.put ("name", "opus/48000/2");
  audioCodecs.push_back (std::make_pair ("", ac) );
  ac.put ("name", "PCMU/8000");
  audioCodecs.push_back (std::make_pair ("", ac) );
  config.add_child ("modules.kurento.SdpEndpoint.audioCodecs", audioCodecs);

  vc.put ("name", "VP8/90000");
  videoCodecs.push_back (std::make_pair ("", vc) );
  vc.put ("name", "H264/90000");
  videoCodecs.push_back (std::make_pair ("", vc) );
  config.add_child ("modules.kurento.SdpEndpoint.videoCodecs", videoCodecs);

  std::shared_ptr <SdpEndpointImpl> sdpEndpoint = generateSdpEndpoint (
        mediaPipelineId);

  releaseMediaObject (sdpEndpoint->getId() );
  releaseMediaObject (mediaPipelineId);

  sdpEndpoint.reset ();
}
