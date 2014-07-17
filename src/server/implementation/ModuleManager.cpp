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

#include "ModuleManager.hpp"

#include <gst/gst.h>
#include <KurentoException.hpp>
#include <glibmm/module.h>
#include <MediaObjectImpl.hpp>

#define GST_CAT_DEFAULT kurento_media_set
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoModuleManager"

namespace kurento
{
int
ModuleManager::addModule (std::string modulePath)
{
  const kurento::FactoryRegistrar *registrar;
  void *registrarFactory;

  Glib::Module module (modulePath);

  if (!module) {
    std::cerr << "module cannot be loaded: " << Glib::Module::get_last_error() << std::endl;
    return -1;
  }

  module.make_resident();

  if (!module.get_symbol ("getFactoryRegistrar", registrarFactory) ) {
    std::cerr << "symbol not found" << std::endl;
    return -1;
  }

  registrar = ( (RegistrarFactoryFunc) registrarFactory) ();
  const std::map <std::string, std::shared_ptr <kurento::Factory > > &factories = registrar->getFactories();

  loadedFactories.insert (factories.begin(), factories.end() );

  return 0;
}

const std::map <std::string, std::shared_ptr <kurento::Factory > >
ModuleManager::getLoadedFactories ()
{
  return loadedFactories;
}

std::shared_ptr<kurento::Factory>
ModuleManager::getFactory (std::string symbolName)
{
  std::shared_ptr<kurento::Factory> ret;

  try {
    ret = loadedFactories.at (symbolName);
  } catch (std::exception &e) {
    std::cerr << "Factory not found: " << e.what() << std::endl;
    ret = NULL;
  }

  return ret;
}

ModuleManager::StaticConstructor ModuleManager::staticConstructor;

ModuleManager::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} // kurento
