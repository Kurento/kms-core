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

#ifndef __SIGNAL_HANDLER_HPP__
#define __SIGNAL_HANDLER_HPP__

#include <map>
#include <functional>
#include <memory>

namespace kurento
{

template <class F, class T>
class MessageAdaptorAux
{
public:
  MessageAdaptorAux (F func,
                     std::weak_ptr <T> object) : func (func),
    object (object) {}

  F func;
  std::weak_ptr <T> object;
};


template <typename R, typename ...Args>
static R
signal_handler_adaptor (Args... args, gpointer data)
{
  MessageAdaptorAux <std::function <R (Args...) >, MediaObjectImpl> *adaptor =
    (MessageAdaptorAux <std::function <R (Args...) >, MediaObjectImpl> *) data;

  std::shared_ptr <MediaObjectImpl> aux (adaptor->object.lock() );

  if (!aux) {
    return;
  }

  return adaptor->func (args...);
}

template <typename R, typename... Args>
static void
signal_handler_destroy (gpointer d, GClosure *closure)
{
  MessageAdaptorAux <std::function <R (Args...) >, MediaObjectImpl> *data =
    (MessageAdaptorAux <std::function <R (Args...) >, MediaObjectImpl> *) d;

  delete data;
}

template <typename R, typename... Args>
static gulong
register_signal_handler (GObject *element,
                         const gchar *signal_name,
                         std::function <R (Args...) > func,
                         std::shared_ptr <MediaObjectImpl> object)
{
  gulong id;

  MessageAdaptorAux<std::function <R (Args...) >, MediaObjectImpl> *data =
    new MessageAdaptorAux <std::function <R (Args...) >, MediaObjectImpl> (func,
        object);

  id = g_signal_connect_data (element, signal_name,
                              G_CALLBACK ( (&signal_handler_adaptor<R, Args...>) ),
                              (gpointer) data, (&signal_handler_destroy<R, Args...>), (GConnectFlags) 0);
  return id;
}

static void
unregister_signal_handler (gpointer element, gulong id)
{
  g_signal_handler_disconnect (element, id);
}

} /* kurento */

#endif /*  __SIGNAL_HANDLER_HPP__ */
