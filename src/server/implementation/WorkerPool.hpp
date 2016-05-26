/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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

#ifndef __WORKERPOOL_HPP__
#define __WORKERPOOL_HPP__

#include <mutex>
#include <thread>
#include <boost/asio.hpp>

namespace kurento
{

class WorkerPool
{
public:
  WorkerPool (int threads);
  ~WorkerPool();

  template <typename CompletionHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE (CompletionHandler, void () )
  post (BOOST_ASIO_MOVE_ARG (CompletionHandler) handler)
  {
    setWatcher();
    return io_service->post (handler);
  }

private:
  void setWatcher();
  void checkWorkers();

  boost::shared_ptr< boost::asio::io_service > io_service;
  std::shared_ptr< boost::asio::io_service::work > work;
  std::vector<std::thread> workers;

  boost::shared_ptr< boost::asio::io_service > watcher_service;
  std::shared_ptr< boost::asio::io_service::work > watcher_work;
  std::thread watcher;

  std::mutex mutex;

  bool terminated = false;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;
};

} // kurento

#endif /* __WORKERPOOL_HPP__ */
