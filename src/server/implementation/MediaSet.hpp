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

#ifndef __MEDIA_SET_H__
#define __MEDIA_SET_H__

#include <MediaObjectImpl.hpp>

#include <unordered_set>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace kurento
{

typedef struct _KeepAliveData KeepAliveData;

class MediaSet
{
public:
  ~MediaSet ();

  void ref (const std::string &sessionId,
            std::shared_ptr<MediaObjectImpl> mediaObject);
  void ref (const std::string &sessionId, const std::string &mediaObjectRef);
  std::shared_ptr<MediaObjectImpl> ref (MediaObjectImpl *mediaObject);

  void unref (const std::string &sessionId,
              std::shared_ptr<MediaObjectImpl> mediaObject);
  void unref (const std::string &sessionId, const std::string &mediaObjectRef);

  void addEventHandler (const std::string &sessionId,
                        const std::string &objectId,
                        const std::string &subscriptionId,
                        std::shared_ptr<EventHandler> handler);
  void removeEventHandler (const std::string &sessionId,
                           const std::string &objectId,
                           const std::string &handlerId);

  void releaseSession (const std::string &sessionId);
  void unrefSession (const std::string &sessionId);
  void keepAliveSession (const std::string &sessionId);

  void release (std::shared_ptr<MediaObjectImpl> mediaObject);
  void release (const std::string &mediaObjectRef);

  std::shared_ptr<MediaObjectImpl> getMediaObject (const std::string
      &mediaObjectRef);
  std::shared_ptr<MediaObjectImpl> getMediaObject (
    const std::string &sessionId, const std::string &mediaObjectRef);

  bool empty();

  static const std::shared_ptr<MediaSet> getMediaSet();
  static void destroyMediaSet();

  sigc::signal<void> signalEmpty;

private:

  void keepAliveSession (const std::string &sessionId, bool create);
  void doGarbageCollection ();

  std::thread thread;

  void releasePointer (MediaObjectImpl *obj);

  void checkEmpty ();

  MediaSet ();

  std::recursive_mutex recMutex;
  std::condition_variable_any waitCond;
  std::atomic<bool> terminated;

  std::map<std::string, std::weak_ptr <MediaObjectImpl>> objectsMap;

  std::map<std::string, std::map <std::string, std::shared_ptr <MediaObjectImpl>>>
  childrenMap;

  std::map<std::string, std::map <std::string, std::shared_ptr<MediaObjectImpl>>>
  sessionMap;

  std::map<std::string, bool> sessionInUse;
  std::map<std::string, std::map<std::string, std::map<std::string, std::shared_ptr<EventHandler>>>>
  eventHandler;

  std::map<std::string, std::unordered_set<std::string>> reverseSessionMap;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;
};

} // kurento

#endif /* __MEDIA_SET_H__ */
