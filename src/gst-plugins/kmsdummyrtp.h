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
#ifndef _KMS_DUMMY_RTP_H_
#define _KMS_DUMMY_RTP_H_

#include "kmselement.h"
#include "commons/kmsbasertpendpoint.h"

G_BEGIN_DECLS
#define KMS_TYPE_DUMMY_RTP     \
  (kms_dummy_rtp_get_type())
#define KMS_DUMMY_RTP(obj) (   \
  G_TYPE_CHECK_INSTANCE_CAST(  \
    (obj),                     \
    KMS_TYPE_DUMMY_RTP,        \
    KmsDummyRtp                \
  )                            \
)

#define KMS_DUMMY_RTP_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_CAST (          \
    (klass),                         \
    KMS_TYPE_DUMMY_RTP,              \
    KmsDummyRtpClass                 \
  )                                  \
)
#define KMS_IS_DUMMY_RTP(obj) (  \
  G_TYPE_CHECK_INSTANCE_TYPE (   \
    (obj),                       \
    KMS_TYPE_DUMMY_RTP           \
  )                              \
)
#define KMS_IS_DUMMY_RTP_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_TYPE(               \
    (klass),                             \
    KMS_TYPE_DUMMY_RTP                   \
  )                                      \
)

typedef struct _KmsDummyRtp KmsDummyRtp;
typedef struct _KmsDummyRtpClass KmsDummyRtpClass;

struct _KmsDummyRtp
{
  KmsBaseRtpEndpoint parent;
};

struct _KmsDummyRtpClass
{
  KmsBaseRtpEndpointClass parent_class;
};

GType kms_dummy_rtp_get_type (void);

gboolean kms_dummy_rtp_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _KMS_DUMMY_rTP_H_ */
