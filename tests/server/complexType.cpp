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

#include <ModuleManager.hpp>
#include <KurentoException.hpp>
#include <jsonrpc/JsonSerializer.hpp>
#include <Fraction.hpp>
#include <gst/gst.h>
#include <MediaSet.hpp>

using namespace kurento;
int
main (int argc, char **argv)
{
  std::shared_ptr <ModuleManager> moduleManager (new ModuleManager() );
  std::shared_ptr<kurento::Factory> mediaPipelineFactory;
  std::shared_ptr<kurento::MediaObject> mediaPipeline;

  gst_init (&argc, &argv);

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
      std::cerr << "Serialization does not match" << std::endl;
      return 1;
    }

  } catch (kurento::KurentoException &e) {
    std::cerr << "Unexpected exception: " << e.what() << std::endl;
    return 1;
  }

  kurento::MediaSet::getMediaSet()->release (std::dynamic_pointer_cast
      <MediaObjectImpl> (mediaPipeline) );

  return 0;
}
