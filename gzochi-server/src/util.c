/* util.c: Assorted utility routines for gzochid
 * Copyright (C) 2016 Julian Graham
 *
 * gzochi is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <gmp.h>
#include <gzochi-common.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void
gzochid_util_serialize_boolean (gboolean bool, GString *out)
{
  char bool_str[1];
  
  bool_str[0] = bool ? 0x1 : 0x0;
  g_string_append_len (out, bool_str, 1);
}

void
gzochid_util_serialize_int (int n, GString *out)
{
  unsigned char n_str[4];

  gzochi_common_io_write_int (n, n_str, 0);
  g_string_append_len (out, (char *) n_str, 4);
}

void
gzochid_util_serialize_bytes (unsigned char *str, int len, GString *out)
{
  gzochid_util_serialize_int (len, out);
  g_string_append_len (out, (char *) str, len);
}

void
gzochid_util_serialize_string (char *str, GString *out)
{
  gzochid_util_serialize_bytes ((unsigned char *) str, strlen (str) + 1, out);
}

void
gzochid_util_serialize_mpz (mpz_t i, GString *out)
{
  char *i_str = mpz_get_str (NULL, 16, i);
  gzochid_util_serialize_string (i_str, out);
  free (i_str);
}

void
gzochid_util_serialize_list (GList *list,
			     void (*serializer) (gpointer, GString *),
			     GString *out)
{
  unsigned char len_str[4];
  int len = g_list_length (list);
  GList *list_ptr = list;

  gzochi_common_io_write_int (len, len_str, 0);
  g_string_append_len (out, (char *) len_str, 4);
  
  while (list_ptr != NULL)
    {
      serializer (list_ptr->data, out);
      list_ptr = list_ptr->next;
    }
}

void
gzochid_util_serialize_sequence (GSequence *sequence,
				 void (*serializer) (gpointer, GString *),
				 GString *out)
{
  unsigned char len_str[4];
  int len = g_sequence_get_length (sequence);
  GSequenceIter *iter = g_sequence_get_begin_iter (sequence);
  
  gzochi_common_io_write_int (len, len_str, 0);
  g_string_append_len (out, (char *) len_str, 4);

  while (!g_sequence_iter_is_end (iter))
    {
      serializer (g_sequence_get (iter), out);
      iter = g_sequence_iter_next (iter);
    }
}

void
gzochid_util_serialize_hash_table
(GHashTable *hashtable, void (*key_serializer) (gpointer, GString *),
 void (*value_serializer) (gpointer, GString *), GString *out)
{
  unsigned char len_str[4];
  int len = g_hash_table_size (hashtable);
  GHashTableIter iter;
  gpointer key, value;

  gzochi_common_io_write_int (len, len_str, 0);
  g_string_append_len (out, (char *) len_str, 4);

  g_hash_table_iter_init (&iter, hashtable);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      key_serializer (key, out);
      value_serializer (value, out);
    }
}

void gzochid_util_serialize_timeval (struct timeval tv, GString *out)
{
  unsigned char str[4];

  gzochi_common_io_write_int (tv.tv_sec, str, 0);
  g_string_append_len (out, (char *) str, 4);

  gzochi_common_io_write_int (tv.tv_usec, str, 0);
  g_string_append_len (out, (char *) str, 4);
}

gboolean
gzochid_util_deserialize_boolean (GString *in)
{
  gboolean ret = in->str[0] == 0x1 ? TRUE : FALSE;
  g_string_erase (in, 0, 1);
  return ret;
}

int
gzochid_util_deserialize_int (GString *in)
{
  int ret = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  g_string_erase (in, 0, 4);
  return ret;
}

unsigned char *
gzochid_util_deserialize_bytes (GString *in, int *len)
{
  int str_len = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  unsigned char *i_str = malloc (sizeof (unsigned char) * str_len);

  memcpy (i_str, in->str + 4, str_len);
  g_string_erase (in, 0, 4 + str_len);

  if (len != NULL)
    *len = str_len;

  return i_str;
}

char *
gzochid_util_deserialize_string (GString *in)
{
  return (char *) gzochid_util_deserialize_bytes (in, NULL);
}

void
gzochid_util_deserialize_mpz (GString *in, mpz_t o)
{
  char *o_str = gzochid_util_deserialize_string (in);
  mpz_set_str (o, o_str, 16);
  free (o_str);
}

GList *
gzochid_util_deserialize_list (GString *in,
			       gpointer (*deserializer) (GString *))
{
  GList *ret = NULL;
  int len = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  g_string_erase (in, 0, 4);

  while (len > 0)
    {
      ret = g_list_append (ret, deserializer (in));
      len--;
    }

  return ret;
}

GSequence *
gzochid_util_deserialize_sequence (GString *in,
				   gpointer (*deserializer) (GString *),
				   GDestroyNotify destroy_fn)
{
  GSequence *ret = g_sequence_new (destroy_fn);
  int len = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  g_string_erase (in, 0, 4);

  while (len > 0)
    {
      g_sequence_append (ret, deserializer (in));
      len--;
    }

  return ret;
}

GHashTable *
gzochid_util_deserialize_hash_table (GString *in, GHashFunc hash_func,
				     GEqualFunc key_equal_func, 
				     gpointer (*key_deserializer) (GString *), 
				     gpointer (*value_deserializer) (GString *))
{
  GHashTable *ret = g_hash_table_new (hash_func, key_equal_func);
  int len = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  g_string_erase (in, 0, 4);

  while (len > 0)
    {
      gpointer key = key_deserializer (in);
      gpointer value = value_deserializer (in);
      g_hash_table_insert (ret, key, value);
      len--;
    }

  return ret;
}

struct timeval
gzochid_util_deserialize_timeval (GString *in)
{
  struct timeval tv;

  tv.tv_sec = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  g_string_erase (in, 0, 4);
  tv.tv_usec = gzochi_common_io_read_int ((unsigned char *) in->str, 0);
  g_string_erase (in, 0, 4);

  return tv;
}

gint
gzochid_util_string_data_compare (gconstpointer a, gconstpointer b,
				  gpointer user_data)
{
  return g_strcmp0 ((const char *) a, (const char *) b);
}

gint
gzochid_util_bytes_compare_null_first (gconstpointer o1, gconstpointer o2)
{
  if (o1 == NULL)
    return o2 == NULL ? 0 : -1;
  else return o2 == NULL ? 1 : g_bytes_compare (o1, o2);
}

gint
gzochid_util_bytes_compare_null_last (gconstpointer o1, gconstpointer o2)
{
  if (o1 == NULL)
    return o2 == NULL ? 0 : 1;
  else return o2 == NULL ? -1 : g_bytes_compare (o1, o2);
}
