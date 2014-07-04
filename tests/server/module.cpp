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

int
main (int argc, char **argv)
{
  const kurento::FactoryRegistrar *registrar;
  void *registrarFactory;

  std::string moduleName = "../../src/server/implementation/libkms-core-module.so";

  Glib::Module module (moduleName);

  if (!module) {
    std::cerr << "module cannot be loaded: " << Glib::Module::get_last_error() << std::endl;
    return 1;
  }

  if (!module.get_symbol ("getFactoryRegistrar", registrarFactory) ) {
    std::cerr << "symbol not found" << std::endl;
    return 1;
  }

  std::cout << "symbol found" << std::endl;
  registrar = ( (RegistrarFactoryFunc) registrarFactory) ();
  const std::list <std::shared_ptr <kurento::Factory > > &factories = registrar->getFactories();

  std::cout << "Found " << factories.size() << " factories" << std::endl;

  for (auto factory : factories) {
    std::cout << "factory " << factory->getName() << std::endl;
  }

  // TODO:
  return 0;
}
