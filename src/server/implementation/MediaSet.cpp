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

#include "MediaSet.hpp"

#include <gst/gst.h>
#include <KurentoException.hpp>
#include <MediaPipelineImpl.hpp>

/* This is included to avoid problems with slots and lamdas */
#include <type_traits>
#include <sigc++/sigc++.h>
namespace sigc
{
template <typename Functor>
struct functor_trait<Functor, false> {
  typedef decltype (::sigc::mem_fun (std::declval<Functor &> (),
                                     &Functor::operator() ) ) _intermediate;

  typedef typename _intermediate::result_type result_type;
  typedef Functor functor_type;
};
}

#define GST_CAT_DEFAULT kurento_media_set
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaSet"

const int MEDIASET_THREADS_DEFAULT = 10;

namespace kurento
{

static const std::chrono::seconds COLLECTOR_INTERVAL = std::chrono::seconds (
      240);

static std::shared_ptr<MediaSet> mediaSet;

void
delete_media_set (MediaSet *ms)
{
  // Just delete mediaset when debugging or using valgrind
  // This way we avoid possible problems with static varibles destruction
  if (getenv ("DEBUG_MEDIASET") != NULL) {
    std::cout << "Deleting mediaSet" << std::endl;
    delete ms;
  } else {
    std::cout << "MediaSet destruction disabled by default" << std::endl;
  }
}

const std::shared_ptr<MediaSet>
MediaSet::getMediaSet()
{
  if (!mediaSet) {
    mediaSet = std::shared_ptr<MediaSet> (new MediaSet(), delete_media_set );
  }

  return mediaSet;
}

void MediaSet::doGarbageCollection ()
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);
  auto sessions = sessionInUse;

  lock.unlock();

  GST_DEBUG ("Running garbage collector");

  for (auto it : sessions) {
    if (it.second) {
      lock.lock();
      sessionInUse[it.first] = false;
      lock.unlock();
    } else {
      GST_WARNING ("Session timeout: %s", it.first.c_str() );
      unrefSession (it.first);
    }
  }
}

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

MediaSet::MediaSet()
{
  terminated = false;

  /* Prepare pool of threads */
  io_service = boost::shared_ptr< boost::asio::io_service >
               ( new boost::asio::io_service () );
  work = std::shared_ptr< boost::asio::io_service::work >
         ( new boost::asio::io_service::work (*io_service) );
  n_threads = MEDIASET_THREADS_DEFAULT;

  for (int i = 0; i < n_threads; i++) {
    threads.push_back (std::thread (std::bind (&workerThreadLoop, io_service) ) );
  }

  thread = std::thread ( [&] () {
    std::unique_lock <std::recursive_mutex> lock (recMutex);


    while (waitCond.wait_for (recMutex,
                              COLLECTOR_INTERVAL) == std::cv_status::timeout) {
      if (terminated) {
        return;
      }

      try {
        doGarbageCollection();
      } catch (...) {
        GST_ERROR ("Error during garbage collection");
      }
    }

  });
}

MediaSet::~MediaSet ()
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  if (!objectsMap.empty() ) {
    std::cerr << "Warning: Still " + std::to_string (objectsMap.size() ) +
              " object/s alive" << std::endl;
  }

  if (!sessionMap.empty() ) {
    std::cerr << "Warning: Still " + std::to_string (sessionMap.size() ) +
              " session/s alive" << std::endl;
  }

  if (!sessionInUse.empty() ) {
    std::cerr << "Warning: Still " + std::to_string (sessionInUse.size() ) +
              " session/s with timeout" << std::endl;
  }

  if (!empty() ) {
    auto copy = sessionMap;

    for (auto it : copy) {
      unrefSession (it.first);
    }
  }

  childrenMap.clear();
  sessionMap.clear();

  terminated = true;
  waitCond.notify_all();

  lock.unlock();

  if (std::this_thread::get_id() != thread.get_id() ) {
    thread.join();
  }

  io_service->stop();

  for (int i = 0; i < n_threads; i++) {
    threads[i].join();
  }
}

std::shared_ptr<MediaObjectImpl>
MediaSet::ref (MediaObjectImpl *mediaObjectPtr)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  std::shared_ptr<MediaObjectImpl> mediaObject;

  if (mediaObjectPtr == NULL) {
    throw KurentoException (MEDIA_OBJECT_NOT_FOUND, "Invalid object");
  }

  try {
    mediaObject = std::dynamic_pointer_cast<MediaObjectImpl>
                  (mediaObjectPtr->shared_from_this() );
  } catch (std::bad_weak_ptr e) {
    mediaObject =  std::shared_ptr<MediaObjectImpl> (mediaObjectPtr, [this] (
    MediaObjectImpl * obj) {
      this->releasePointer (obj);
    });
  }

  objectsMap[mediaObject->getId()] = std::weak_ptr<MediaObjectImpl> (mediaObject);

  if (mediaObject->getParent() ) {
    std::shared_ptr<MediaObjectImpl> parent = std::dynamic_pointer_cast
        <MediaObjectImpl> (mediaObject->getParent() );

    ref (parent.get() );
    childrenMap[parent->getId()][mediaObject->getId()] = mediaObject;
  }

  return mediaObject;
}

void
MediaSet::ref (const std::string &sessionId,
               std::shared_ptr<MediaObjectImpl> mediaObject)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  keepAliveSession (sessionId, true);

  if (mediaObject->getParent() ) {
    ref (sessionId,
         std::dynamic_pointer_cast<MediaObjectImpl> (mediaObject->getParent() ) );
  }

  sessionMap[sessionId][mediaObject->getId()] = mediaObject;
  reverseSessionMap[mediaObject->getId()].insert (sessionId);
  ref (mediaObject.get() );
}

void
MediaSet::ref (const std::string &sessionId,
               const std::string &mediaObjectRef)
{
  std::shared_ptr <MediaObjectImpl> object = getMediaObject (mediaObjectRef);

  ref (sessionId, object);
}

void
MediaSet::keepAliveSession (const std::string &sessionId)
{
  keepAliveSession (sessionId, false);
}

void
MediaSet::keepAliveSession (const std::string &sessionId, bool create)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  auto it = sessionInUse.find (sessionId);

  if (it == sessionInUse.end() ) {
    if (create) {
      sessionInUse[sessionId] = true;
    } else {
      throw KurentoException (INVALID_SESSION, "Invalid session");
    }
  } else {
    it->second = true;
  }
}

void
MediaSet::releaseSession (const std::string &sessionId)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  auto it = sessionMap.find (sessionId);

  if (it != sessionMap.end() ) {
    auto objects = it->second;

    for (auto it2 : objects) {
      release (it2.second);
    }
  }

  sessionMap.erase (sessionId);
  sessionInUse.erase (sessionId);
  eventHandler.erase (sessionId);
  lock.unlock ();

}

void
MediaSet::unrefSession (const std::string &sessionId)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  auto it = sessionMap.find (sessionId);

  if (it != sessionMap.end() ) {
    auto objects = it->second;

    for (auto it2 : objects) {
      unref (sessionId, it2.second);
    }
  }

  sessionMap.erase (sessionId);
  sessionInUse.erase (sessionId);
  eventHandler.erase (sessionId);

  lock.unlock();
}

void
MediaSet::unref (const std::string &sessionId,
                 std::shared_ptr< MediaObjectImpl > mediaObject)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  if (!mediaObject) {
    return;
  }

  auto it = sessionMap.find (sessionId);

  if (it != sessionMap.end() ) {
    auto it2 = it->second.find (mediaObject->getId() );

    if (it2 == it->second.end() ) {
      return;
    }

    it->second.erase (it2);

    if (it->second.size() == 0) {
      unrefSession (sessionId);
    }
  }

  auto it3 = reverseSessionMap.find (mediaObject->getId() );

  if (it3 != reverseSessionMap.end() ) {
    it3->second.erase (sessionId);

    if (it3->second.empty() ) {
      std::shared_ptr<MediaObjectImpl> parent;

      parent = std::dynamic_pointer_cast<MediaObjectImpl> (mediaObject->getParent() );

      if (parent) {
        childrenMap[parent->getId()].erase (mediaObject->getId() );
      }

      auto childrenIt = childrenMap.find (mediaObject->getId() );

      if (childrenIt != childrenMap.end() ) {
        auto childMap = childrenIt->second;

        for (auto child : childMap) {
          unref (sessionId, child.second);
        }
      }

      childrenMap.erase (mediaObject->getId() );
    }
  }

  auto eventIt = eventHandler.find (sessionId);

  if (eventIt != eventHandler.end() ) {
    eventIt->second.erase (mediaObject->getId() );
  }

  lock.unlock();
}

void
MediaSet::unref (const std::string &sessionId,
                 const std::string &mediaObjectRef)
{
  std::shared_ptr <MediaObjectImpl> object;

  try {
    object = getMediaObject (mediaObjectRef);
  } catch (KurentoException e) {
    return;
  }

  unref (sessionId, object);
}

static void
async_delete (MediaObjectImpl *mediaObject, std::string id)
{
  GST_DEBUG ("Destroying %s -> %s", mediaObject->getType().c_str(), id.c_str() );
  /* Deletion of mediaObject weak reference */
  delete mediaObject;
}

void MediaSet::releasePointer (MediaObjectImpl *mediaObject)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);
  std::string id = mediaObject->getId();

  objectsMap.erase (id );

  lock.unlock();

  io_service->post ( boost::bind ( async_delete, mediaObject, id ) );

  checkEmpty();
}

void MediaSet::release (std::shared_ptr< MediaObjectImpl > mediaObject)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  auto it = reverseSessionMap.find (mediaObject->getId() );

  if (it == reverseSessionMap.end() ) {
    /* Already released */
    return;
  }

  auto sessions = it->second;

  for (auto it2 : sessions) {
    unref (it2, mediaObject);
  }

  objectsMap.erase (mediaObject->getId() );

  lock.unlock();
}

void MediaSet::release (const std::string &mediaObjectRef)
{
  try {
    std::shared_ptr< MediaObjectImpl > obj = getMediaObject (mediaObjectRef);

    release (obj);
  } catch (...) {
    /* Do not raise exception if it is already released*/
  }
}

std::shared_ptr< MediaObjectImpl >
MediaSet::getMediaObject (const std::string &mediaObjectRef)
{
  std::shared_ptr <MediaObjectImpl> objectLocked;
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  auto it = objectsMap.find (mediaObjectRef);

  if (it == objectsMap.end() ) {
    throw KurentoException (MEDIA_OBJECT_NOT_FOUND, "Object not found");
  }

  try {
    objectLocked = it->second.lock();
  } catch (...) {
    throw KurentoException (MEDIA_OBJECT_NOT_FOUND, "Object not found");
  }

  if (!objectLocked) {
    throw KurentoException (MEDIA_OBJECT_NOT_FOUND, "Object not found");
  }

  return objectLocked;
}

std::shared_ptr< MediaObjectImpl >
MediaSet::getMediaObject (const std::string &sessionId,
                          const std::string &mediaObjectRef)
{
  std::shared_ptr< MediaObjectImpl > obj = getMediaObject (mediaObjectRef);

  ref (sessionId, obj);
  return obj;
}

void
MediaSet::addEventHandler (const std::string &sessionId,
                           const std::string &objectId,
                           const std::string &subscriptionId,
                           std::shared_ptr<EventHandler> handler)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  eventHandler[sessionId][objectId][subscriptionId] = handler;
}

void
MediaSet::removeEventHandler (const std::string &sessionId,
                              const std::string &objectId,
                              const std::string &handlerId)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);
  auto it = eventHandler.find (sessionId);

  if (it != eventHandler.end() ) {
    auto it2 = eventHandler[sessionId].find (objectId);

    if (it2 != eventHandler[sessionId].end() ) {
      it2->second.erase (handlerId);
    }
  }
}

void
MediaSet::checkEmpty()
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  if ( empty() ) {
    lock.unlock();
    signalEmpty.emit();
  }
}

bool
MediaSet::empty()
{
  return objectsMap.empty();
}

std::vector<std::string>
MediaSet::getSessions ()
{
  std::vector<std::string> ret (sessionMap.size () );

  for (auto it : sessionMap) {
    ret.push_back (it.first);
  }

  return ret;
}

static void
store_pipelines (std::list<std::shared_ptr<MediaObjectImpl>> &list,
                 std::map<std::string, std::shared_ptr<MediaObjectImpl>> &map)
{
  for (auto it : map) {
    if (std::dynamic_pointer_cast <MediaPipelineImpl> (it.second) ) {
      list.push_back (it.second);
    }
  }
}

std::list<std::shared_ptr<MediaObjectImpl>>
    MediaSet::getPipelines (const std::string &sessionId)
{
  std::list<std::shared_ptr<MediaObjectImpl>> ret;

  try {
    if (sessionId.empty () ) {
      for (auto it : sessionMap) {
        store_pipelines (ret, it.second);
      }
    } else {
      store_pipelines (ret, sessionMap.at (sessionId) );
    }
  } catch (std::out_of_range) {
    GST_ERROR ("Cannot get session %s", sessionId.c_str() );
  }

  return ret;
}

std::list<std::shared_ptr<MediaObjectImpl>>
    MediaSet::getChilds (std::shared_ptr<MediaObjectImpl> obj)
{
  std::list<std::shared_ptr<MediaObjectImpl>> ret;

  try {
    for (auto it : childrenMap.at (obj->getId() ) ) {
      ret.push_back (it.second);
    }
  } catch (std::out_of_range) {
    GST_ERROR ("Cannot get childrens of object %s", obj->getId().c_str() );
  }

  return ret;
}

MediaSet::StaticConstructor MediaSet::staticConstructor;

MediaSet::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} // kurento
