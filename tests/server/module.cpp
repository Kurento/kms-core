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

#include <glibmm/module.h>
#include <FactoryRegistrar.hpp>
#include <MediaObjectImpl.hpp>
#include <KurentoException.hpp>
#include <jsonrpc/JsonSerializer.hpp>
#include <Error.hpp>
#include <gst/gst.h>


int
main (int argc, char **argv)
{
  const kurento::FactoryRegistrar *registrar;
  void *registrarFactory;
  std::shared_ptr<kurento::MediaObject> mediaPipeline;

  gst_init (&argc, &argv);

  std::string moduleName = "../../src/server/libkmscoremodule.so";

  Glib::Module module (moduleName);

  if (!module) {
    std::cerr << "module cannot be loaded: " << Glib::Module::get_last_error() << std::endl;
    return 1;
  }

  if (!module.get_symbol ("getFactoryRegistrar", registrarFactory) ) {
    std::cerr << "symbol not found" << std::endl;
    return 1;
  }

  registrar = ( (RegistrarFactoryFunc) registrarFactory) ();
  const std::map <std::string, std::shared_ptr <kurento::Factory > > &factories = registrar->getFactories();

  mediaPipeline = factories.at ("MediaPipeline")->createObject (Json::Value() );

  try {
    kurento::JsonSerializer writer (true);
    kurento::JsonSerializer writer2 (true);
    kurento::JsonSerializer reader (false);
    kurento::Error e (mediaPipeline, "Error description", 10, "ERROR_TYPE");
    std::shared_ptr<kurento::Error> e2;

    writer.Serialize ("error", e);

    reader.JsonValue = writer.JsonValue;
    reader.Serialize ("error", e2);

    writer2.Serialize ("error", e2);

    if (writer.JsonValue.toStyledString() != writer2.JsonValue.toStyledString() ) {
      std::cerr << "Serialization does not match" << std::endl;
      return 1;
    }

  } catch (kurento::KurentoException &e) {
    std::cerr << "Unexpected exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
