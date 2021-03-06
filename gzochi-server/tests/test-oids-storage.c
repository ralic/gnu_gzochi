/* test-oids-storage.c: Test routines for oids.c in gzochid.
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "oids.h"
#include "oids-storage.h"
#include "storage-mem.h"
#include "util.h"

struct _test_oids_storage
{
  gzochid_storage_engine_interface *interface;
  gzochid_storage_context *context;
  gzochid_storage_store *meta;

  gzochid_oid_allocation_strategy *strategy;
};

typedef struct _test_oids_storage test_oids_storage;

static void
setup_storage (test_oids_storage *fixture, gconstpointer user_data)
{
  fixture->interface = &gzochid_storage_engine_interface_mem;
  fixture->context = fixture->interface->initialize ("test");
  fixture->meta = fixture->interface->open (fixture->context, "meta", 0);

  fixture->strategy = gzochid_storage_oid_strategy_new
    (fixture->interface, fixture->context, fixture->meta);
}

static void
teardown_storage (test_oids_storage *fixture, gconstpointer user_data)
{
  fixture->interface->close_store (fixture->meta);
  fixture->interface->close_context (fixture->context);

  gzochid_oid_allocation_strategy_free (fixture->strategy);
}

static guint64
get_oid_block_counter (test_oids_storage *fixture)
{
  char key[] = { 0 };
  guint64 value = 0;
  gzochid_storage_transaction *transaction =
    fixture->interface->transaction_begin (fixture->context);
  
  char *value_bytes = fixture->interface->transaction_get
    (transaction, fixture->meta, key, 1, NULL);

  memcpy (&value, value_bytes, sizeof (guint64));
  free (value_bytes);
  
  fixture->interface->transaction_rollback (transaction);

  return gzochid_util_decode_oid (value);
}

static void
test_oids_storage_reserve_first (test_oids_storage *fixture,
				 gconstpointer user_data)
{
  GError *err = NULL;
  gzochid_data_oids_block oids_block;
  guint64 new_counter_value = 0;
  
  g_assert
    (gzochid_oids_reserve_block (fixture->strategy, &oids_block, &err));
  g_assert_no_error (err);

  new_counter_value = get_oid_block_counter (fixture);
  g_assert_cmpint (new_counter_value, ==, 100);
}

static void
test_oids_storage_reserve_next (test_oids_storage *fixture,
				gconstpointer user_data)
{
  GError *err = NULL;
  gzochid_data_oids_block oids_block;
  guint64 new_counter_value = 0;

  char key[] = { 0 };
  guint64 value = gzochid_util_encode_oid (1);
  gzochid_storage_transaction *transaction =
    fixture->interface->transaction_begin (fixture->context);

  fixture->interface->transaction_put
    (transaction, fixture->meta, key, 1, (char *) &value, sizeof (guint64));

  fixture->interface->transaction_prepare (transaction);
  fixture->interface->transaction_commit (transaction);
  
  g_assert
    (gzochid_oids_reserve_block (fixture->strategy, &oids_block, &err));
  g_assert_no_error (err);

  new_counter_value = get_oid_block_counter (fixture);
  g_assert_cmpint (new_counter_value, ==, 101);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add
    ("/oids-storage/reserve/first", test_oids_storage, NULL, setup_storage,
     test_oids_storage_reserve_first, teardown_storage);
  g_test_add
    ("/oids-storage/reserve/next", test_oids_storage, NULL, setup_storage,
     test_oids_storage_reserve_next, teardown_storage);

  return g_test_run ();
}
