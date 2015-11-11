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
#ifndef _KMS_DUMMY_URI_H_
#define _KMS_DUMMY_URI_H_

#include "kmselement.h"
#include "commons/kmsuriendpoint.h"

G_BEGIN_DECLS
#define KMS_TYPE_DUMMY_URI       \
  (kms_dummy_uri_get_type())
#define KMS_DUMMY_URI(obj) (   \
  G_TYPE_CHECK_INSTANCE_CAST(     \
    (obj),                        \
    KMS_TYPE_DUMMY_URI,        \
    KmsDummyUri                \
  )                               \
)

#define KMS_DUMMY_URI_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_CAST (             \
    (klass),                            \
    KMS_TYPE_DUMMY_URI,              \
    KmsDummyUriClass                 \
  )                                     \
)
#define KMS_IS_DUMMY_URI(obj) (  \
  G_TYPE_CHECK_INSTANCE_TYPE (      \
    (obj),                          \
    KMS_TYPE_DUMMY_URI           \
  )                                 \
)
#define KMS_IS_DUMMY_URI_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_TYPE(                  \
    (klass),                                \
    KMS_TYPE_DUMMY_URI                   \
  )                                         \
)
typedef struct _KmsDummyUri KmsDummyUri;
typedef struct _KmsDummyUriClass KmsDummyUriClass;
typedef struct _KmsDummyUriPrivate KmsDummyUriPrivate;

struct _KmsDummyUri
{
  KmsUriEndpoint parent;

  /*< private > */
  KmsDummyUriPrivate *priv;
};

struct _KmsDummyUriClass
{
  KmsUriEndpointClass parent_class;
};

GType kms_dummy_uri_get_type (void);

gboolean kms_dummy_uri_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _KMS_DUMMY_URI_H_ */
