/*
* (C) Copyright 2015 Kurento (http://kurento.org/)
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

#ifndef __REGISTER_PARENT_CPP__
#define __REGISTER_PARENT_CPP__

#include "RegisterParent.hpp"

namespace kurento
{

std::map <std::string, std::function<RegisterParent* (void) >>
    RegisterParent::complexTypesRegistered;

void
RegisterParent::registerType (std::string type,
                              std::function<RegisterParent* (void) > func)
{
  complexTypesRegistered[type] = func;
}

RegisterParent *
RegisterParent::createRegister (std::string type)
{
  return complexTypesRegistered [type] ();
}

} /* kurento */

#endif /*  __REGISTER_PARENT_CPP__ */
