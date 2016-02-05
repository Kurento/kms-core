/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
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
#ifndef _KMS_CONNECTION_EXT_H_
#define _KMS_CONNECTION_EXT_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define KMS_TYPE_CONNECTION_EXT \
  (kms_connection_ext_get_type())

#define KMS_CONNECTION_EXT(obj) ( \
  G_TYPE_CHECK_INSTANCE_CAST (    \
    (obj),                        \
    KMS_TYPE_CONNECTION_EXT,      \
    KmsConnectionExt              \
  )                               \
)
#define KMS_CONNECTION_EXT_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_CAST (               \
    (klass),                              \
    KMS_TYPE_CONNECTION_EXT,              \
    KmsConnectionExtClass                 \
  )                                       \
)
#define KMS_IS_CONNECTION_EXT(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (       \
    (obj),                           \
    KMS_TYPE_CONNECTION_EXT          \
  )                                  \
)
#define KMS_IS_CONNECTION_EXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_CONNECTION_EXT))
#define KMS_CONNECTION_EXT_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                \
    (obj),                                   \
    KMS_TYPE_CONNECTION_EXT,                 \
    KmsConnectionExtClass                    \
  )                                          \
)

typedef struct _KmsConnectionExt KmsConnectionExt;
typedef struct _KmsConnectionExtClass KmsConnectionExtClass;

struct _KmsConnectionExt
{
  GObject parent;
};

/* Array of Ips:
 * Array [
 *   struct_0 {
 *     nettype: string,   ["IN" for internet]
 *     addrtype : string, ["IP4", "IP4"]
 *     address : string,
 *     ttl: uint,
 *     addrnumber: uint
 *   },
 *   struct_1 {
 *     nettype: string,
 *     addrtype : string,
 *     address : string,
 *     ttl: uint,
 *     addrnumber: uint
 *   },
 *   ...
 *   struct_N {
 *     ...
 *   }
 * ]
 */

struct _KmsConnectionExtClass
{
  GObjectClass parent_class;

  /* Emitted when an offer is processed */
  void (*on_answer_ips) (KmsConnectionExt * ext,
    const GArray * ips_offered, GArray * ips_answered);

  /* Emitted when an offer is going to be created  */
  void (*on_offer_ips) (KmsConnectionExt * ext, GArray * ips);

  /* Emmited as a result of precessing the previous offer generated */
  void (*on_answered_ips) (KmsConnectionExt * ext, const GArray * ips);
};

GType kms_connection_ext_get_type ();

KmsConnectionExt * kms_connection_ext_new ();

G_END_DECLS

#endif /* _KMS_CONNECTION_EXT_H_ */
