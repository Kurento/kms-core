/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gst/gst.h>
#include "ServerInfo.hpp"
#include "MediaPipelineImpl.hpp"
#include "ServerManagerImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <MediaSet.hpp>
#include <boost/property_tree/json_parser.hpp>

#define GST_CAT_DEFAULT kurento_server_manager_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoServerManagerImpl"

#define METADATA "metadata"

namespace kurento
{

static std::string
childToString (const boost::property_tree::ptree tree, const char *nodeName)
{
  boost::property_tree::ptree childTree;
  std::stringstream ss;

  try {
    childTree = tree.get_child (nodeName);

    if (childTree.size() > 0) {
      write_json (ss, childTree);
    }
  } catch (boost::property_tree::ptree_bad_path &e) {
    GST_LOG ("No %s in config file", nodeName);
  }

  return ss.str ();
}

ServerManagerImpl::ServerManagerImpl (const std::shared_ptr<ServerInfo> info,
                                      const boost::property_tree::ptree &config,
                                      ModuleManager &moduleManager) : MediaObjectImpl (config),
  info (info), moduleManager (moduleManager)
{
  metadata = childToString (config, METADATA);
}

std::shared_ptr<ServerInfo> ServerManagerImpl::getInfo ()
{
  return info;
}

std::vector<std::shared_ptr<MediaPipeline>> ServerManagerImpl::getPipelines ()
{
  std::vector<std::shared_ptr<MediaPipeline>> ret;

  for (auto it : MediaSet::getMediaSet ()->getPipelines() ) {
    ret.push_back (std::dynamic_pointer_cast <MediaPipeline> (it) );
  }

  return ret;
}

std::vector<std::string> ServerManagerImpl::getSessions ()
{
  return MediaSet::getMediaSet ()->getSessions();
}

std::string ServerManagerImpl::getMetadata ()
{
  return metadata;
}

std::string ServerManagerImpl::getKmd (const std::string &moduleName)
{
  for (auto moduleIt : moduleManager.getModules () ) {
    if (moduleIt.second->getName () == moduleName) {
      return moduleIt.second->getDescriptor();
    }
  }

  GST_WARNING ("Requested kmd module doesn't exist");

  throw KurentoException (SERVER_MANAGER_ERROR_KMD_NOT_FOUND,
                          "Requested kmd module doesn't exist");
}

static int64_t
get_int64 (std::string &str, char sep, int nToken)
{
  size_t start = str.find_first_not_of (sep), end;
  int count = 0;

  while (start != std::string::npos) {
    end = str.find (sep, start);

    if (count++ == nToken) {
      str[end] = '\0';
      return atol (&str.c_str() [start]);
    }

    start = str.find_first_not_of (sep, end);
  }

  return 0;
}

int64_t
ServerManagerImpl::getUsedMemory()
{
  std::string stat;
  std::ifstream stat_file ("/proc/self/stat");

  std::getline (stat_file, stat);
  stat_file.close();

  return get_int64 (stat, ' ', 22) / 1024;
}

ServerManagerImpl::StaticConstructor ServerManagerImpl::staticConstructor;

ServerManagerImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
