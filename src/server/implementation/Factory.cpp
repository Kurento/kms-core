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

#include "Factory.hpp"
#include "MediaSet.hpp"

namespace kurento
{

std::shared_ptr<MediaObjectImpl>
Factory::getObject (const std::string &id)
{
  return MediaSet::getMediaSet()->getMediaObject (id);
}

std::shared_ptr< MediaObjectImpl >
Factory::createObject (const boost::property_tree::ptree &conf,
                       const std::string &session, const Json::Value &params) const
{
  std::shared_ptr< MediaObjectImpl > object;
  object = MediaSet::getMediaSet()->ref (dynamic_cast <MediaObjectImpl *>
                                         (createObjectPointer (conf, params) ) );

  MediaSet::getMediaSet()->ref (session, object);

  return object;
}

} /* kurento */
