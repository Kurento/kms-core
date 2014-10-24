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

#include <gst/gst.h>

#include "WorkerPool.hpp"

#define GST_CAT_DEFAULT kurento_worker_pool
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoWorkerPool"

const int WORKER_THREADS_TIMEOUT = 3; /* seconds */

namespace kurento
{

static void
workerThreadLoop ( boost::shared_ptr< boost::asio::io_service > io_service )
{
  bool running = true;

  while (running) {
    try {
      GST_DEBUG ("Working thread starting");
      io_service->run();
      running = false;
    } catch (std::exception &e) {
      GST_ERROR ("Unexpected error while running the server: %s", e.what() );
    } catch (...) {
      GST_ERROR ("Unexpected error while running the server");
    }
  }

  GST_DEBUG ("Working thread finished");
}

WorkerPool::WorkerPool (int threads)
{
  /* Prepare watcher */
  watcher_service = boost::shared_ptr< boost::asio::io_service >
                    ( new boost::asio::io_service () );
  watcher_work = std::shared_ptr< boost::asio::io_service::work >
                 ( new boost::asio::io_service::work (*watcher_service) );
  watcher = std::thread (std::bind (&workerThreadLoop, watcher_service) );

  /* Prepare pool of threads */
  io_service = boost::shared_ptr< boost::asio::io_service >
               ( new boost::asio::io_service () );
  work = std::shared_ptr< boost::asio::io_service::work >
         ( new boost::asio::io_service::work (*io_service) );

  for (int i = 0; i < threads; i++) {
    workers.push_back (std::thread (std::bind (&workerThreadLoop, io_service) ) );
  }
}

WorkerPool::~WorkerPool()
{
  watcher_service->stop();
  io_service->stop();

  watcher.join();

  for (uint i = 0; i < workers.size (); i++) {
    workers[i].join();
  }
}

static void
async_worker_test (std::shared_ptr<std::atomic<bool>> alive)
{
  *alive = true;
}

void
WorkerPool::checkWorkers ()
{
  std::shared_ptr<std::atomic<bool>> alive;

  alive = std::shared_ptr<std::atomic<bool>> ( new std::atomic<bool> (false) );

  io_service->post (std::bind (&async_worker_test, alive) );

  boost::shared_ptr< boost::asio::deadline_timer > timer (
    new boost::asio::deadline_timer ( *watcher_service ) );

  timer->expires_from_now ( boost::posix_time::seconds ( WORKER_THREADS_TIMEOUT) );
  timer->async_wait ( [timer, alive, this] (const boost::system::error_code & error) {
    if (error) {
      GST_ERROR ("ERROR: %s", error.message().c_str() );
      return;
    }

    if (*alive) {
      return;
    }

    /* TODO: Create working thread */
  });
}

void
WorkerPool::setWatcher ()
{
  watcher_service->post (std::bind (&WorkerPool::checkWorkers, this) );
}

WorkerPool::StaticConstructor WorkerPool::staticConstructor;

WorkerPool::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} // kurento
