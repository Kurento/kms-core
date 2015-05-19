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
#include <objects/MediaObjectImpl.hpp>
#include <string>
#include <MediaSet.hpp>
#include <ModuleManager.hpp>

using namespace kurento;

ModuleManager moduleManager;
boost::property_tree::ptree config;

static std::shared_ptr <MediaElementImpl>
createDummyElement (std::string name, std::string mediaPipelineId)
{
  std::shared_ptr <MediaElementImpl> element = std::dynamic_pointer_cast
      <MediaElementImpl> (MediaSet::getMediaSet()->ref (new  MediaElementImpl (
                            boost::property_tree::ptree(),
                            MediaSet::getMediaSet()->getMediaObject (mediaPipelineId),
                            name) ) );

  return element;
}

BOOST_AUTO_TEST_CASE (add_tag)
{
  gst_init (NULL, NULL);

  std::shared_ptr <MediaObjectImpl> mediaObject ( new  MediaObjectImpl
      (boost::property_tree::ptree() ) );

  mediaObject->removeTag ("1");

  mediaObject->addTag ("1", "test1");
  mediaObject->addTag ("2", "test2");
  mediaObject->addTag ("3", "test3");

  mediaObject->removeTag ("3");
  mediaObject->removeTag ("5");
  mediaObject->removeTag ("3");

  mediaObject.reset ();
}

BOOST_AUTO_TEST_CASE (add_tag_media_element)
{
  moduleManager.loadModulesFromDirectories ("../../src/server");

  std::string mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();
  std::shared_ptr <MediaElementImpl> mediaElement =
    createDummyElement ("dummyduplex", mediaPipelineId);

  mediaElement->removeTag ("1");

  mediaElement->addTag ("1", "test1");
  mediaElement->addTag ("2", "test2");
  mediaElement->addTag ("3", "test3");

  mediaElement->removeTag ("3");
  mediaElement->removeTag ("5");
  mediaElement->removeTag ("3");

  mediaElement.reset ();
}

BOOST_AUTO_TEST_CASE (get_tag)
{
  moduleManager.loadModulesFromDirectories ("../../src/server");

  std::string mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();
  std::shared_ptr <MediaElementImpl> mediaElement =
    createDummyElement ("dummyduplex", mediaPipelineId);

  mediaElement->addTag ("1", "test1");
  mediaElement->addTag ("2", "test2");

  try {
    std::string ret = mediaElement->getTag ("1");

    if (ret != "test1") {
      BOOST_ERROR ("Bad response");
    }
  } catch (KurentoException &e) {
    BOOST_ERROR ("Tag not found");
  }

  try {
    std::string ret = mediaElement->getTag ("3");
    BOOST_ERROR ("Tag not found not detected");
  } catch (KurentoException &e) {
    if (e.getCode () != 40110) {
      BOOST_ERROR ("Tag not found not detected");
    }
  }

  mediaElement.reset ();
}

BOOST_AUTO_TEST_CASE (get_tags)
{
  moduleManager.loadModulesFromDirectories ("../../src/server");

  std::string mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();
  std::shared_ptr <MediaElementImpl> mediaElement =
    createDummyElement ("dummyduplex", mediaPipelineId);
  std::vector<std::shared_ptr<Tag>> ret ;
  int i = 1;

  mediaElement->addTag ("1", "test1");
  mediaElement->addTag ("2", "test2");
  mediaElement->addTag ("3", "test3");

  ret = mediaElement->getTags ();

  if (ret.size () == 0) {
    BOOST_ERROR ("Tag list is empty");
  }

  for (auto it : ret ) {
    std::string key (std::to_string (i) );
    std::string value ("test" + std::to_string (i) );

    if ( (it->getKey() != key) || (it->getValue() != value) ) {
      BOOST_ERROR ("Tag list is wrong");
    }

    i++;
  }

  mediaElement.reset ();
}

BOOST_AUTO_TEST_CASE (creation_time)
{
  moduleManager.loadModulesFromDirectories ("../../src/server");

  std::string mediaPipelineId =
    moduleManager.getFactory ("MediaPipeline")->createObject (
      config, "",
      Json::Value() )->getId();
  std::shared_ptr <MediaElementImpl> mediaElement =
    createDummyElement ("dummyduplex", mediaPipelineId);
  std::shared_ptr <MediaPipelineImpl> pipe = std::dynamic_pointer_cast
      <MediaPipelineImpl> (MediaSet::getMediaSet()->getMediaObject (
                             mediaPipelineId) );
  time_t now = time (NULL);

  BOOST_CHECK (pipe->getCreationTime() <= mediaElement->getCreationTime() );
  BOOST_CHECK (pipe->getCreationTime() <= now);
  BOOST_CHECK (mediaElement->getCreationTime() <= now);
}

