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
#ifndef _KMS_DUMMY_SRC_H_
#define _KMS_DUMMY_SRC_H_

#include "kmselement.h"

G_BEGIN_DECLS
#define KMS_TYPE_DUMMY_SRC       \
  (kms_dummy_src_get_type())
#define KMS_DUMMY_SRC(obj) (     \
  G_TYPE_CHECK_INSTANCE_CAST(    \
    (obj),                       \
    KMS_TYPE_DUMMY_SRC,          \
    KmsDummySrc                  \
  )                              \
)
#define KMS_DUMMY_SRC_CLASS(klass) (   \
  G_TYPE_CHECK_CLASS_CAST (            \
    (klass),                           \
    KMS_TYPE_DUMMY_SRC,                \
    KmsDummySrcClass                   \
  )                                    \
)
#define KMS_IS_DUMMY_SRC(obj) (        \
  G_TYPE_CHECK_INSTANCE_TYPE (         \
    (obj),                             \
    KMS_TYPE_DUMMY_SRC                 \
  )                                    \
)
#define KMS_IS_DUMMY_SRC_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_TYPE(               \
    (klass),                             \
    KMS_TYPE_DUMMY_SRC                   \
  )                                      \
)
typedef struct _KmsDummySrc KmsDummySrc;
typedef struct _KmsDummySrcClass KmsDummySrcClass;
typedef struct _KmsDummySrcPrivate KmsDummySrcPrivate;

struct _KmsDummySrc
{
  KmsElement parent;

  /*< private > */
  KmsDummySrcPrivate *priv;
};

struct _KmsDummySrcClass
{
  KmsElementClass parent_class;
};

GType kms_dummy_src_get_type (void);

gboolean kms_dummy_src_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _KMS_DUMMY_SRC_H_ */
