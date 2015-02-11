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

#ifndef __KMS_ISTATS_H__
#define __KMS_ISTATS_H__

#include <gst/gst.h>

#define KMS_TYPE_ISTATS \
  (kms_istats_get_type ())
#define KMS_ISTATS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), KMS_TYPE_ISTATS, KmsIStats))
#define KMS_IS_ISTATS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KMS_TYPE_ISTATS))
#define KMS_ISTATS_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), KMS_TYPE_ISTATS, KmsIStatsInterface))

typedef struct _KmsIStats               KmsIStats;
typedef struct _KmsIStatsInterface      KmsIStatsInterface;

struct _KmsIStatsInterface
{
  GTypeInterface parent;
};

GType kms_istats_get_type (void);

#endif /* __KMS_ISTATS_H__ */
