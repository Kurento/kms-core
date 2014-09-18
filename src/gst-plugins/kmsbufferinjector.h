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

#ifndef __KMS_BUFFER_INJECTOR_H__
#define __KMS_BUFFER_INJECTOR_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define KMS_TYPE_BUFFER_INJECTOR \
  (kms_buffer_injector_get_type())
#define KMS_BUFFER_INJECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_BUFFER_INJECTOR,KmsBufferInjector))
#define KMS_BUFFER_INJECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_BUFFER_INJECTOR,KmsBufferInjectorClass))
#define KMS_IS_BUFFER_INJECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_BUFFER_INJECTOR))
#define KMS_IS_BUFFER_INJECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_BUFFER_INJECTOR))
#define KMS_BUFFER_INJECTOR_CAST(obj) ((KmsBufferInjector*)(obj))

typedef struct _KmsBufferInjector KmsBufferInjector;
typedef struct _KmsBufferInjectorClass KmsBufferInjectorClass;
typedef struct _KmsBufferInjectorPrivate KmsBufferInjectorPrivate;

struct _KmsBufferInjector
{
  GstElement element;

  KmsBufferInjectorPrivate *priv;
};

struct _KmsBufferInjectorClass
{
  GstElementClass parent_class;
};

GType kms_buffer_injector_get_type (void);

gboolean kms_buffer_injector_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __KMS_BUFFER_INJECTOR_H__ */
