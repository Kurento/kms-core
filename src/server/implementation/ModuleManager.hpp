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

#ifndef __MODULE_MANAGER_H__
#define __MODULE_MANAGER_H__

#include <glibmm/module.h>
#include <unordered_set>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <FactoryRegistrar.hpp>
#include <MediaObjectImpl.hpp>

namespace kurento
{

class ModuleData
{
public:
  ModuleData (const std::string &name, const std::string &version,
              const std::string &compilationTime,
              const char *descriptor,
              const std::map <std::string, std::shared_ptr <kurento::Factory > > &factories) :
    name (name), version (version), generationTime (compilationTime),
    descriptor (descriptor), factories (factories)
  {
  }

  std::string getName() const
  {
    return name;
  }

  std::string getVersion () const
  {
    return version;
  }

  std::string getGenerationTime () const
  {
    return generationTime;
  }

  std::string getDescriptor () const
  {
    if (descriptor == NULL) {
      return "";
    } else {
      return descriptor;
    }
  }

  const std::map <std::string, std::shared_ptr <kurento::Factory > >
  &getFactories()
  {
    return factories;
  }


private:
  std::string name;
  std::string version;
  std::string generationTime;
  const char *descriptor;
  const std::map <std::string, std::shared_ptr <kurento::Factory > > &factories;
};

class ModuleManager
{
public:
  ModuleManager () {};
  ~ModuleManager () {};

  int loadModule (std::string modulePath);
  void loadModulesFromDirectories (std::string dirPath);
  const std::map <std::string, std::shared_ptr <kurento::Factory > >
  getLoadedFactories ();
  std::shared_ptr<kurento::Factory> getFactory (std::string symbolName);

  const std::map <std::string, std::shared_ptr <ModuleData>> getModules () const
  {
    return loadedModules;
  }

private:

  std::map <std::string, std::shared_ptr <kurento::Factory > > loadedFactories;
  std::map <std::string, std::shared_ptr <ModuleData>> loadedModules;
  void loadModules (std::string path);


  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;
};

} // kurento

#endif /* __MODULE_MANAGER_H__ */
