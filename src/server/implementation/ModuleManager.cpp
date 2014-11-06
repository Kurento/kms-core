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
#include <boost/filesystem.hpp>

#define GST_CAT_DEFAULT kurento_media_set
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoModuleManager"

namespace kurento
{

typedef const char * (*GetVersionFunc) ();
typedef const char * (*GetNameFunc) ();
typedef const char * (*GetDescFunc) ();

int
ModuleManager::loadModule (std::string modulePath)
{
  const kurento::FactoryRegistrar *registrar;
  void *registrarFactory, *getVersion = NULL, *getName = NULL, *getDescriptor = NULL;
  std::string moduleFileName;
  std::string moduleName;
  std::string moduleVersion;
  const char *moduleDescriptor = NULL;

  boost::filesystem::path path (modulePath);

  moduleFileName = path.filename().string();

  if (loadedModules.find (moduleFileName) != loadedModules.end() ) {
    GST_WARNING ("Module named %s already loaded", moduleFileName.c_str() );
    return -1;
  }

  Glib::Module module (modulePath);

  if (!module) {
    GST_WARNING ("Module %s cannot be loaded: %s", modulePath.c_str(), Glib::Module::get_last_error().c_str() );
    return -1;
  }

  if (!module.get_symbol ("getFactoryRegistrar", registrarFactory) ) {
    GST_WARNING ("Symbol not found");
    return -1;
  }

  registrar = ( (RegistrarFactoryFunc) registrarFactory) ();
  const std::map <std::string, std::shared_ptr <kurento::Factory > > &factories = registrar->getFactories();

  for (auto it : factories) {
    if (loadedFactories.find (it.first) != loadedFactories.end() ) {
      GST_WARNING ("Factory %s is already registered, skiping module %s", it.first.c_str(), module.get_name().c_str() );
      return -1;
    }
  }

  module.make_resident();

  loadedFactories.insert (factories.begin(), factories.end() );

  GST_DEBUG ("Module loaded from %s", module.get_name().c_str() );

  if (!module.get_symbol ("getModuleVersion", getVersion) ) {
    GST_WARNING ("Cannot get module version");
  } else {
    moduleVersion = ( (GetNameFunc) getVersion) ();
  }

  if (!module.get_symbol ("getModuleName", getName) ) {
    GST_WARNING ("Cannot get module name");
  } else {
    moduleName = ( (GetVersionFunc) getName) ();
  }

  if (!module.get_symbol ("getModuleDescriptor", getDescriptor) ) {
    GST_WARNING ("Cannot get module descriptor");
  } else {
    moduleDescriptor = ( (GetDescFunc) getDescriptor) ();
  }

  loadedModules[moduleFileName] = std::shared_ptr<ModuleData> (new ModuleData (moduleName, moduleVersion, moduleDescriptor, factories) );

  GST_INFO ("Loaded %s version %s", moduleName.c_str() , moduleVersion.c_str() );

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
ModuleManager::getFactory (std::string factoryName)
{
  try {
    return loadedFactories.at (factoryName);
  } catch (std::exception &e) {
    GST_ERROR ("Factory %s not found: %s", factoryName.c_str(), e.what() );
    throw kurento::KurentoException (MEDIA_OBJECT_NOT_AVAILABLE, "Factory '" +
                                     factoryName + "'' not found");
  }
}

ModuleManager::StaticConstructor ModuleManager::staticConstructor;

ModuleManager::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} // kurento
