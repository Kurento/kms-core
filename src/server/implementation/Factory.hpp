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

#ifndef __FACTORY_HPP__
#define __FACTORY_HPP__

#include <memory>
#include <json/json.h>
#include <boost/property_tree/ptree.hpp>

namespace kurento
{

class MediaObjectImpl;

class Factory
{
public:
  Factory() {};
  virtual ~Factory() {};

  std::shared_ptr<MediaObjectImpl> createObject (const boost::property_tree::ptree
      &conf, const std::string &session, const Json::Value &params) const;

  static std::shared_ptr<MediaObjectImpl> getObject (const std::string &id);

  virtual std::string getName() const = 0;

protected:
  virtual MediaObjectImpl *createObjectPointer (const boost::property_tree::ptree
      &conf, const Json::Value &params) const = 0;
};

} /* kurento */

#endif /* __FACTORY_HPP__ */
