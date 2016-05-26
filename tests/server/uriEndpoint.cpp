/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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
#define BOOST_TEST_MODULE UriEndpoint
#include <boost/test/unit_test.hpp>
#include <MediaPipelineImpl.hpp>
#include <MediaElementImpl.hpp>
#include <ElementConnectionData.hpp>
#include <MediaType.hpp>
#include <KurentoException.hpp>
#include <objects/UriEndpointImpl.hpp>
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

class UriEndpointFactory : public Factory
{
public:
  std::string getName() const
  {
    return "UriEndpointFactory";
  }

protected:
  MediaObjectImpl *createObjectPointer (const boost::property_tree::ptree
                                        &conf, const Json::Value &params) const
  {
    std::string mediaPipelineId = params["mediaPipeline"].asString ();
    std::string uri = params["uri"].asString ();

    std::shared_ptr <MediaObjectImpl> pipe =
      MediaSet::getMediaSet ()->getMediaObject (mediaPipelineId);

    return new  UriEndpointImpl (config, pipe, "dummyuri", uri);
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


static std::shared_ptr <UriEndpointImpl> generateUriEndpoint (
  const std::string &mediaPipelineId, const std::string &uri)
{
  UriEndpointFactory factory;
  Json::Value params;

  params["mediaPipeline"] = mediaPipelineId;
  params["uri"] = uri;

  return std::dynamic_pointer_cast<UriEndpointImpl> (
           factory.createObject (config, "", params) );
}

BOOST_AUTO_TEST_CASE (check_regular_uri)
{
  std::string uri;
  std::string first_uri = "file:///tmp/file.webm";

  mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();

  config.add ("modules.kurento.UriEndpoint.configPath", "../../../tests" );
  config.add ("modules.kurento.UriEndpoint.defaultPath", "file:///var/kurento/");

  std::shared_ptr <UriEndpointImpl> uriEndpoint = generateUriEndpoint (
        mediaPipelineId, first_uri);

  uri = uriEndpoint->getUri ();

  if (uri != first_uri) {
    BOOST_ERROR ("Wrong URI returned by UriEndpoint");
  }

  releaseMediaObject (uriEndpoint->getId() );
  releaseMediaObject (mediaPipelineId);

  uriEndpoint.reset ();
}

BOOST_AUTO_TEST_CASE (check_media_element_uri)
{
  std::string uri;
  std::string first_uri = "file.webm";
  std::string expected_uri = "file:///var/kurento/file.webm";
  char *uri_value;

  mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();

  config.add ("modules.kurento.UriEndpoint.configPath", "../../../tests" );
  config.add ("modules.kurento.UriEndpoint.defaultPath", "file:///var/kurento/");

  std::shared_ptr <UriEndpointImpl> uriEndpoint = generateUriEndpoint (
        mediaPipelineId, first_uri);

  uri = uriEndpoint->getUri ();

  if (uri != first_uri) {
    BOOST_ERROR ("Wrong URI returned by UriEndpoint");
  }

  g_object_get (G_OBJECT (uriEndpoint->getGstreamerElement() ), "uri", &uri_value,
                NULL);

  if (g_strcmp0 (uri_value, expected_uri.c_str () ) != 0) {
    BOOST_ERROR ("Wrong URI returned by gstreamer element");
  }

  g_free (uri_value);

  releaseMediaObject (uriEndpoint->getId() );
  releaseMediaObject (mediaPipelineId);

  uriEndpoint.reset ();
}
