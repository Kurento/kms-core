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
#define BOOST_TEST_MODULE ComplexType
#include <boost/test/unit_test.hpp>
#include <ModuleManager.hpp>
#include <KurentoException.hpp>
#include <jsonrpc/JsonSerializer.hpp>
#include <Fraction.hpp>
#include <gst/gst.h>
#include <MediaSet.hpp>

using namespace kurento;

struct GF {
  GF();
  ~GF();
};

BOOST_GLOBAL_FIXTURE (GF)

GF::GF()
{
}

GF::~GF()
{
  MediaSet::deleteMediaSet();
}


BOOST_AUTO_TEST_CASE (complex_type)
{
  std::shared_ptr <ModuleManager> moduleManager (new ModuleManager() );
  std::shared_ptr<kurento::Factory> mediaPipelineFactory;
  std::shared_ptr<kurento::MediaObject> mediaPipeline;

  gst_init (NULL, NULL);

  std::string moduleName = "../../src/server/libkmscoremodule.so";

  moduleManager->loadModule (moduleName);

  mediaPipelineFactory = moduleManager->getFactory ("MediaPipeline");

  mediaPipeline = mediaPipelineFactory->createObject (
                    boost::property_tree::ptree(), "", Json::Value() );

  try {
    kurento::JsonSerializer writer (true);
    kurento::JsonSerializer writer2 (true);
    kurento::JsonSerializer reader (false);

    kurento::Fraction complexType (15, 1);
    std::shared_ptr<kurento::Fraction > complexTypeReturned;

    complexType.Serialize (writer);

    reader.JsonValue = writer.JsonValue;
    Serialize (complexTypeReturned , reader);
    Serialize (complexTypeReturned , writer2);

    if (writer.JsonValue.toStyledString() != writer2.JsonValue.toStyledString() ) {
      BOOST_ERROR ("Serialization does not match");
    }

  } catch (kurento::KurentoException &e) {
    std::cerr << "Unexpected exception: " << e.what() << std::endl;
    BOOST_ERROR ("Unexpected error");
  }

  kurento::MediaSet::getMediaSet()->release (std::dynamic_pointer_cast
      <MediaObjectImpl> (mediaPipeline) );
}
