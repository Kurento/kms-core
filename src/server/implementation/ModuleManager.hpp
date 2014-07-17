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

#include <glibmm.h>
#include <unordered_set>
#include <map>
#include <memory>
#include <string>
#include <FactoryRegistrar.hpp>
#include <MediaObjectImpl.hpp>

namespace kurento
{

class ModuleManager
{
public:
  ModuleManager () {};
  ~ModuleManager () {};

  int addModule (std::string modulePath);
  const std::map <std::string, std::shared_ptr <kurento::Factory > > getLoadedFactories ();
  std::shared_ptr<kurento::Factory> getFactory (std::string symbolName);

private:

  std::map <std::string, std::shared_ptr <kurento::Factory > > loadedFactories;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;
};

} // kurento

#endif /* __MODULE_MANAGER_H__ */
