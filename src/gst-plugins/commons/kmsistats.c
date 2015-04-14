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

#include "kmsistats.h"
#include "kms-core-marshal.h"

enum
{
  SIGNAL_STATS,
  LAST_SIGNAL
};

static guint kms_i_stats_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (KmsIStats, kms_istats, 0);

static void
kms_istats_default_init (KmsIStatsInterface * iface)
{
  /* set actions */
  kms_i_stats_signals[SIGNAL_STATS] =
      g_signal_new ("stats", G_TYPE_FROM_CLASS (iface),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (KmsIStatsInterface, stats),
      NULL, NULL, __kms_core_marshal_BOXED__VOID, GST_TYPE_STRUCTURE, 0);
}
