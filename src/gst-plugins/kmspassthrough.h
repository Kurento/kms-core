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
#ifndef _KMS_PASS_THROUGH_H_
#define _KMS_PASS_THROUGH_H_

#include "kmselement.h"

G_BEGIN_DECLS
#define KMS_TYPE_PASS_THROUGH (kms_pass_through_get_type())
#define KMS_PASS_THROUGH(obj) (                 \
  G_TYPE_CHECK_INSTANCE_CAST (                  \
    (obj),                                      \
    KMS_TYPE_PASS_THROUGH,                      \
    KmsPassThrough                              \
  )                                             \
)
#define KMS_PASS_THROUGH_CLASS(klass) (         \
  G_TYPE_CHECK_CLASS_CAST (                     \
    (klass),                                    \
    KMS_TYPE_PASS_THROUGH,                      \
    KmsPassThroughClass                         \
  )                                             \
)
#define KMS_IS_PASS_THROUGH(obj) (              \
  G_TYPE_CHECK_INSTANCE_TYPE (                  \
    (obj),                                      \
    KMS_TYPE_PASS_THROUGH                       \
    )                                           \
)
#define KMS_IS_PASS_THROUGH_CLASS(klass) (      \
  G_TYPE_CHECK_CLASS_TYPE (                     \
  (klass),                                      \
  KMS_TYPE_PASS_THROUGH                         \
  )                                             \
)
typedef struct _KmsPassThrough KmsPassThrough;
typedef struct _KmsPassThroughClass KmsPassThroughClass;

struct _KmsPassThrough
{
  KmsElement parent;
};

struct _KmsPassThroughClass
{
  KmsElementClass parent_class;
};

GType kms_pass_through_get_type (void);

gboolean kms_pass_through_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif
