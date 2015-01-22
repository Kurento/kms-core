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

using namespace kurento;

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
  std::shared_ptr <MediaPipelineImpl> pipe (new MediaPipelineImpl (
        boost::property_tree::ptree() ) );
  std::shared_ptr <MediaElementImpl> mediaElement (new  MediaElementImpl (
        boost::property_tree::ptree(), pipe, "dummyduplex") );

  mediaElement->removeTag ("1");

  mediaElement->addTag ("1", "test1");
  mediaElement->addTag ("2", "test2");
  mediaElement->addTag ("3", "test3");

  mediaElement->removeTag ("3");
  mediaElement->removeTag ("5");
  mediaElement->removeTag ("3");

  mediaElement.reset ();
}
