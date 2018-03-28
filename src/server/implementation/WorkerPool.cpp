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

#include <gst/gst.h>

#include "WorkerPool.hpp"
#include <atomic>
#include <memory>

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
  watcher_work =
      std::make_shared<boost::asio::io_service::work>(*watcher_service);
  watcher = std::thread (std::bind (&workerThreadLoop, watcher_service) );

  /* Prepare pool of threads */
  io_service = boost::shared_ptr< boost::asio::io_service >
               ( new boost::asio::io_service () );
  work = std::make_shared<boost::asio::io_service::work>(*io_service);

  for (int i = 0; i < threads; i++) {
    workers.emplace_back(std::bind(&workerThreadLoop, io_service));
  }
}

WorkerPool::~WorkerPool()
{
  std::unique_lock <std::mutex> lock (mutex);
  terminated = true ;
  lock.unlock();

  watcher_service->stop();
  io_service->stop();

  try {
    if (std::this_thread::get_id() != watcher.get_id() ) {
      watcher.join();
    }
  } catch (std::system_error &e) {
    GST_ERROR ("Error joining: %s", e.what() );
  }

  try {
    if (watcher.joinable() ) {
      watcher.detach();
    }
  } catch (std::system_error &e) {
    GST_ERROR ("Error detaching: %s", e.what() );
  }

  for (auto &worker : workers) {
    try {
      if (std::this_thread::get_id() != worker.get_id()) {
        worker.join();
      }
    } catch (std::system_error &e) {
      GST_ERROR ("Error joining: %s", e.what() );
    }

    try {
      if (worker.joinable()) {
        worker.detach();
      }
    } catch (std::system_error &e) {
      GST_ERROR ("Error detaching: %s", e.what() );
    }
  }

  workers.empty();

  // Executing queued tasks
  io_service->reset();

  while (io_service->poll() ) {
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

  alive = std::make_shared<std::atomic<bool>>(false);

  io_service->post (std::bind (&async_worker_test, alive) );

  boost::shared_ptr< boost::asio::deadline_timer > timer (
    new boost::asio::deadline_timer ( *watcher_service ) );

  timer->expires_from_now ( boost::posix_time::seconds (
                              WORKER_THREADS_TIMEOUT) );
  timer->async_wait ( [timer, alive,
  this] (const boost::system::error_code & error) {
    if (error) {
      GST_ERROR ("ERROR: %s", error.message().c_str() );
      return;
    }

    if (*alive) {
      return;
    }

    GST_WARNING ("Worker threads locked. Spawning a new one.");

    std::unique_lock <std::mutex> lock (mutex);

    if (!terminated) {
      workers.emplace_back(std::bind(&workerThreadLoop, io_service));
    }
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
