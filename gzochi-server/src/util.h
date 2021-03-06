/* util.h: Prototypes and declarations for util.c
 * Copyright (C) 2017 Julian Graham
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

#ifndef GZOCHID_UTIL_H
#define GZOCHID_UTIL_H

#include <glib.h>
#include <sys/time.h>

void gzochid_util_serialize_boolean (gboolean, GByteArray *);
void gzochid_util_serialize_int (int, GByteArray *);

/* Writes the big-endian representation of the specified 64-bit unsigned long to
   the specified byte array. */

void gzochid_util_serialize_uint64 (guint64, GByteArray *);

/* A more intentional alias for `gzochid_util_serialize_uint64'. */

void gzochid_util_serialize_oid (guint64, GByteArray *);

void gzochid_util_serialize_bytes (unsigned char *, int, GByteArray *);
void gzochid_util_serialize_string (char *, GByteArray *);
void gzochid_util_serialize_list 
(GList *, void (*) (gpointer, GByteArray *), GByteArray *);
void gzochid_util_serialize_sequence
(GSequence *, void (*) (gpointer, GByteArray *), GByteArray *);
void gzochid_util_serialize_hash_table
(GHashTable *, 
 void (*) (gpointer, GByteArray *), 
 void (*) (gpointer, GByteArray *), 
 GByteArray *);
void gzochid_util_serialize_timeval (struct timeval, GByteArray *);

gboolean gzochid_util_deserialize_boolean (GByteArray *);
int gzochid_util_deserialize_int (GByteArray *);

/* Reads the big-endian representation of a 64-bit unsigned long from the 
   specified byte array, erasing eight bytes from the array (which is assumed to
   have at least eight bytes to spare. */

guint64 gzochid_util_deserialize_uint64 (GByteArray *);

/* A more intentional alias for `gzochid_util_deserialize_uint64'. */

guint64 gzochid_util_deserialize_oid (GByteArray *);

unsigned char *gzochid_util_deserialize_bytes (GByteArray *, int *);
char *gzochid_util_deserialize_string (GByteArray *);
GList *gzochid_util_deserialize_list
(GByteArray *, gpointer (*) (GByteArray *));
GSequence *gzochid_util_deserialize_sequence 
(GByteArray *, gpointer (*) (GByteArray *), GDestroyNotify);
GHashTable *gzochid_util_deserialize_hash_table
(GByteArray *, 
 GHashFunc, 
 GEqualFunc, 
 gpointer (*) (GByteArray *), 
 gpointer (*) (GByteArray *));
struct timeval gzochid_util_deserialize_timeval (GByteArray *);

gint gzochid_util_string_data_compare (gconstpointer, gconstpointer, gpointer);

/* A `GCompareDataFunc' implementation that operates on pointers to 
   `guint64's. */

gint gzochid_util_guint64_data_compare (gconstpointer, gconstpointer, gpointer);

/* Comparison function that sorts `NULL' before all non-`NULL' values. Use for
   comparing the lower bounds of range locks. */

gint gzochid_util_bytes_compare_null_first (gconstpointer, gconstpointer);

/* Comparison function that sorts `NULL' after all non-`NULL' values. Use for
   comparing the upper bounds of range locks. */

gint gzochid_util_bytes_compare_null_last (gconstpointer, gconstpointer);

/* Return the big-endian encoding of the specified 64-bit unsigned integer. (On
   big-endian platforms, this is a no-op.) */

guint64 gzochid_util_encode_oid (guint64);

/* Return the native encoding of the specified big endian-encoded 64-bit 
   unsigned integer. (On big-endian platforms, this is a no-op.) */

guint64 gzochid_util_decode_oid (guint64);

/* 
   Makes a full (deep) copy of a GList.

   This is a "backport" of `g_list_copy_deep' which was introduced in GLib 2.34.
*/

GList *gzochid_util_list_copy_deep (GList *, GCopyFunc, gpointer);

/*
  Formats the contents of the specified `GBytes' for console display, writing
  as much of the result as can fit to the specified buffer, which must be at 
  least one byte in size. The formatting works as follows:

  - Printable, non-whitespace characters are written directly to the buffer.
  - Non-printable / whitespace characters are written as the four-byte string
    "\xNN" where NN is the fixed-width hexadecimal representation of the 
    character.
  - If at any point the output buffer cannot hold the representation of the next
    character in the `GBytes' array, the penultimate character in the buffer is
    set to `_' (provided the buffer is at least two bytes wide).
  - The buffer is `NULL'-terminated.
*/

void gzochid_util_format_bytes (GBytes *, char *, size_t);

/* 
   Helper macro to enable a concise representation of a common use case for 
   `gzochid_util_format_bytes', which is to declare and fill out a 
   stack-allocated buffer of some specified size with a printable representation
   of the target byte buffer, and then do something with it, e.g. use it in a 
   log message. 

   The `stmt' argument can include multiple expressions.
*/

#define GZOCHID_WITH_FORMATTED_BYTES(bytes, var, len, stmt)  \
  do							     \
    {							     \
      char var[len];					     \
      gzochid_util_format_bytes (bytes, var, len);	     \
      stmt;						     \
    }							     \
  while (0)

#endif /* GZOCHID_UTIL_H */
