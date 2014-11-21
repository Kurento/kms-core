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

#ifndef __FACTORY_REGISTRAR_HPP__
#define __FACTORY_REGISTRAR_HPP__

#include <list>
#include <memory>
#include <Factory.hpp>

namespace kurento
{

class FactoryRegistrar
{
public:
  FactoryRegistrar (std::map<std::string, std::shared_ptr<Factory>> &factories) :
    factories (factories) {};
  ~FactoryRegistrar() {};

  const std::map<std::string, std::shared_ptr<Factory>> &getFactories () const
  {
    return factories;
  }

private:
  std::map<std::string, std::shared_ptr<Factory>> factories;
};

} /* kurento */

typedef const kurento::FactoryRegistrar * (*RegistrarFactoryFunc) ();

#endif /*  __FACTORY_REGISTRAR_HPP__ */
