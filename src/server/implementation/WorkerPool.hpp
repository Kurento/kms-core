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
