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

#ifndef __KMS_AGNOSTIC_BIN3_H__
#define __KMS_AGNOSTIC_BIN3_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define KMS_TYPE_AGNOSTIC_BIN3 \
  (kms_agnostic_bin3_get_type())
#define KMS_AGNOSTIC_BIN3(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_AGNOSTIC_BIN3,KmsAgnosticBin3))
#define KMS_AGNOSTIC_BIN3_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_AGNOSTIC_BIN3,KmsAgnosticBin3Class))
#define KMS_IS_AGNOSTIC_BIN3(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_AGNOSTIC_BIN3))
#define KMS_IS_AGNOSTIC_BIN3_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_AGNOSTIC_BIN3))
#define KMS_AGNOSTIC_BIN3_CAST(obj) ((KmsAgnosticBin3*)(obj))

typedef struct _KmsAgnosticBin3 KmsAgnosticBin3;
typedef struct _KmsAgnosticBin3Class KmsAgnosticBin3Class;
typedef struct _KmsAgnosticBin3Private KmsAgnosticBin3Private;

struct _KmsAgnosticBin3
{
  GstBin parent;

  KmsAgnosticBin3Private *priv;
};

struct _KmsAgnosticBin3Class
{
  GstBinClass parent_class;

  /* signals */
  gboolean (*caps_signal) (KmsAgnosticBin3 * self, GstCaps *caps);
};

GType kms_agnostic_bin3_get_type (void);

gboolean kms_agnostic_bin3_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __KMS_AGNOSTIC_BIN3_H__ */
