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

#ifndef __KMS_SDP_GROUP_MANAGER_H__
#define __KMS_SDP_GROUP_MANAGER_H__

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>

#include "kmssdpcontext.h"
#include "kmsisdpmediaextension.h"
#include "kmssdpbasegroup.h"

G_BEGIN_DECLS

#define KMS_TYPE_SDP_GROUP_MANAGER \
  (kms_sdp_group_manager_get_type())

#define KMS_SDP_GROUP_MANAGER(obj) (    \
  G_TYPE_CHECK_INSTANCE_CAST (          \
    (obj),                              \
    KMS_TYPE_SDP_GROUP_MANAGER,         \
    KmsSdpGroupManager                  \
  )                                     \
)
#define KMS_SDP_GROUP_MANAGER_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (                   \
    (klass),                                  \
    KMS_TYPE_SDP_GROUP_MANAGER,               \
    KmsSdpGroupManagerClass                   \
  )                                           \
)
#define KMS_IS_SDP_GROUP_MANAGER(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (          \
    (obj),                              \
    KMS_TYPE_SDP_GROUP_MANAGER          \
  )                                     \
)
#define KMS_IS_SDP_GROUP_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_GROUP_MANAGER))
#define KMS_SDP_GROUP_MANAGER_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                   \
    (obj),                                      \
    KMS_TYPE_SDP_GROUP_MANAGER,                 \
    KmsSdpGroupManagerClass                     \
  )                                             \
)

typedef struct _KmsSdpGroupManager KmsSdpGroupManager;
typedef struct _KmsSdpGroupManagerClass KmsSdpGroupManagerClass;
typedef struct _KmsSdpGroupManagerPrivate KmsSdpGroupManagerPrivate;

struct _KmsSdpGroupManager
{
  GObject parent;

  /*< private > */
  KmsSdpGroupManagerPrivate *priv;
};

struct _KmsSdpGroupManagerClass
{
  GObjectClass parent_class;

  /*< public >*/
  gint (*add_group) (KmsSdpGroupManager *obj, KmsSdpBaseGroup *group);
  void (*add_handler) (KmsSdpGroupManager *obj, KmsSdpHandler *handler);
  gboolean (*remove_handler) (KmsSdpGroupManager *obj, KmsSdpHandler *handler);
  gboolean (*add_handler_to_group) (KmsSdpGroupManager *obj, guint gid, guint hid);
  gboolean (*remove_handler_from_group) (KmsSdpGroupManager *obj, guint gid, guint hid);
};

GType kms_sdp_group_manager_get_type ();

KmsSdpGroupManager * kms_sdp_group_manager_new ();

gint kms_sdp_group_manager_add_group (KmsSdpGroupManager *obj, KmsSdpBaseGroup *group);
void kms_sdp_group_manager_add_handler (KmsSdpGroupManager *obj, KmsSdpHandler *handler);
gboolean kms_sdp_group_manager_remove_handler (KmsSdpGroupManager *obj, KmsSdpHandler *handler);

gboolean kms_sdp_group_manager_add_handler_to_group (KmsSdpGroupManager *obj, guint gid, guint hid);
gboolean kms_sdp_group_manager_remove_handler_from_group (KmsSdpGroupManager *obj, guint gid, guint hid);

G_END_DECLS

#endif /* __KMS_SDP_GROUP_MANAGER_H__ */
