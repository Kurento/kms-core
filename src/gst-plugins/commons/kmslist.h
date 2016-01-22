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

#ifndef __KMS_LIST_H__
#define __KMS_LIST_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _KmsList KmsList;
#define KMS_LIST_CAST(obj) ((KmsList *) obj)

#define kms_list_new(key_equal_func) \
  kms_list_new_full (key_equal_func, NULL, NULL);

KmsList * kms_list_new_full (GEqualFunc key_equal_func,
                      GDestroyNotify key_destroy_func,
                      GDestroyNotify value_destroy_func);

guint kms_list_length (KmsList *list);

void kms_list_append (KmsList *list, gpointer key, gpointer value);
void kms_list_prepend (KmsList *list, gpointer key, gpointer value);
void kms_list_remove (KmsList *list, gpointer key);

typedef struct _KmsListIter KmsListIter;
void kms_list_iter_init (KmsListIter *iter, KmsList *list);
gboolean kms_list_iter_next (KmsListIter *iter, gpointer *key, gpointer *value);

void kms_list_foreach (KmsList *list, GHFunc func, gpointer user_data);

gboolean kms_list_contains (KmsList *list, gpointer key);
gpointer kms_list_lookup (KmsList *list, gpointer key);

KmsList * kms_list_ref (KmsList *list);
void kms_list_unref (KmsList *list);

struct _KmsListIter
{
  /*< private >*/
  GSList *item;
};

G_END_DECLS

#endif /* __KMS_LIST_H__ */
