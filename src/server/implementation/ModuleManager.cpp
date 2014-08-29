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
#include <config.h>

#include "ModuleManager.hpp"

#include <gst/gst.h>
#include <KurentoException.hpp>
#include <sstream>

#define GST_CAT_DEFAULT kurento_media_set
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoModuleManager"

namespace kurento
{
int
ModuleManager::loadModule (std::string modulePath)
{
  const kurento::FactoryRegistrar *registrar;
  void *registrarFactory;

  Glib::Module module (modulePath);

  if (!module) {
    GST_WARNING ("Module %s cannot be loaded: %s", modulePath.c_str(), Glib::Module::get_last_error().c_str() );
    return -1;
  }

  if (!module.get_symbol ("getFactoryRegistrar", registrarFactory) ) {
    GST_WARNING ("Symbol not found");
    return -1;
  }

  module.make_resident();

  registrar = ( (RegistrarFactoryFunc) registrarFactory) ();
  const std::map <std::string, std::shared_ptr <kurento::Factory > > &factories = registrar->getFactories();

  loadedFactories.insert (factories.begin(), factories.end() );

  GST_INFO ("Module %s loaded", module.get_name().c_str() );

  return 0;
}

std::list<std::string> split (const std::string &s, char delim)
{
  std::list<std::string> elems;
  std::stringstream ss (s);
  std::string item;

  while (std::getline (ss, item, delim) ) {
    elems.push_back (item);
  }

  return elems;
}

void
ModuleManager::loadModules (std::string dirPath)
{
  DIR *dir;
  std::string name;
  struct dirent *ent;

  GST_INFO ("Looking for modules in %s", dirPath.c_str() );
  dir = opendir (dirPath.c_str() );

  if (dir == NULL) {
    GST_WARNING ("Unable to load modules from:  %s", dirPath.c_str() );
    return;
  }

  /* print all the files and directories within directory */
  while ( (ent = readdir (dir) ) != NULL) {
    name = ent->d_name;

    if (ent->d_type == DT_REG) {
      if (name.size () > 3) {
        std::string ext = name.substr (name.size() - 3);

        if ( ext == ".so" ) {
          std::string name = dirPath + "/" + ent->d_name;
          loadModule (name);
        }
      }
    } else if (ent->d_type == DT_DIR && "." != name && ".." != name) {
      std::string dirName = dirPath + "/" + ent->d_name;

      this->loadModules (dirName);
    }
  }

  closedir (dir);
}

void
ModuleManager::loadModulesFromDirectories (std::string path)
{
  std::list <std::string> locations;

  locations = split (path, ':');

  for (std::string location : locations) {
    this->loadModules (location);
  }

  //try to load modules from the default path
  this->loadModules (KURENTO_MODULES_DIR);

  return;
}

const std::map <std::string, std::shared_ptr <kurento::Factory > >
ModuleManager::getLoadedFactories ()
{
  return loadedFactories;
}

std::shared_ptr<kurento::Factory>
ModuleManager::getFactory (std::string symbolName)
{
  try {
    return loadedFactories.at (symbolName);
  } catch (std::exception &e) {
    GST_ERROR ("Factory not found: %s", e.what() );
    throw kurento::KurentoException (MEDIA_OBJECT_NOT_AVAILABLE, "Factory not found");
  }
}

ModuleManager::StaticConstructor ModuleManager::staticConstructor;

ModuleManager::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} // kurento
