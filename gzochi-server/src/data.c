/* data.c: Application data management routines for gzochid
 * Copyright (C) 2013 Julian Graham
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

#include <assert.h>
#include <glib.h>
#include <gmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "data.h"
#include "game.h"
#include "io.h"
#include "log.h"
#include "storage.h"
#include "tx.h"

#define ALLOCATION_BLOCK_SIZE 100

#define NEXT_OID_KEY 0x00

gzochid_oid_holder *gzochid_oid_holder_new (void)
{
  gzochid_oid_holder *holder = malloc (sizeof (gzochid_oid_holder));

  mpz_init (holder->oid);

  return holder;
}

void gzochid_oid_holder_free (gzochid_oid_holder *holder)
{
  mpz_clear (holder->oid);
  free (holder);
}

static gzochid_data_transaction_context *create_transaction_context 
(gzochid_application_context *app_context, struct timeval *timeout)
{
  gzochid_data_transaction_context *tx_context = 
    calloc (1, sizeof (gzochid_data_transaction_context));

  tx_context->context = app_context;

  if (timeout != NULL)
    tx_context->transaction = gzochid_storage_transaction_begin_timed 
      (app_context->storage_context, *timeout);
  else tx_context->transaction = gzochid_storage_transaction_begin 
	 (app_context->storage_context);

  tx_context->oids_to_references = g_hash_table_new (g_str_hash, g_str_equal);
  tx_context->ptrs_to_references = g_hash_table_new 
    (g_direct_hash, g_direct_equal);

  return tx_context;
}

static void free_oid_block (void *ptr)
{
  gzochid_data_oid_block *block = (gzochid_data_oid_block *) ptr;

  mpz_clear (block->first);
  mpz_clear (block->next);
  mpz_clear (block->last);
} 

static void transaction_context_free (gzochid_data_transaction_context *context)
{  
  if (context->free_oids != NULL)
    free_oid_block (context->free_oids);

  g_list_free_full (context->used_oid_blocks, free_oid_block);

  g_hash_table_destroy (context->oids_to_references);
  g_hash_table_destroy (context->ptrs_to_references);

  free (context);
}

static gboolean flush_reference 
(gzochid_data_managed_reference *reference,
 gzochid_data_transaction_context *context)
{
  gzochid_application_data_event *event = NULL;
  gzochid_application_event *base_event = NULL;

  char *oid_str = NULL;
  GString *out = NULL;

  switch (reference->state)
    {
    case GZOCHID_MANAGED_REFERENCE_STATE_EMPTY:
    case GZOCHID_MANAGED_REFERENCE_STATE_REMOVED_EMPTY:
      break;

    case GZOCHID_MANAGED_REFERENCE_STATE_NEW:
    case GZOCHID_MANAGED_REFERENCE_STATE_MODIFIED:
      
      out = g_string_new (NULL);
      assert (reference->obj != NULL);

      oid_str = mpz_get_str (NULL, 16, reference->oid);
      gzochid_debug ("Flushing new/modified reference '%s'.", oid_str);

      reference->serialization->serializer
	(context->context, reference->obj, out);

      if (gzochid_transaction_rollback_only ())
	{
	  g_string_free (out, FALSE);	  
	  return FALSE;
	}

      gzochid_storage_transaction_put
	(context->transaction, context->context->oids, oid_str, 
	 strlen (oid_str) + 1, out->str, out->len);

      event = malloc (sizeof (gzochid_application_data_event));
      base_event = (gzochid_application_event *) event;

      base_event->type = BYTES_WRITTEN;
      gettimeofday (&base_event->timestamp, NULL);
      event->bytes = out->len;

      gzochid_application_event_dispatch
	(context->context->event_source, base_event);

      g_string_free (out, FALSE);

      break;
    case GZOCHID_MANAGED_REFERENCE_STATE_REMOVED_FETCHED:
    case GZOCHID_MANAGED_REFERENCE_STATE_NOT_MODIFIED: break;
    default:
      assert (1 == 0);
    }

  return TRUE;
}

static int data_prepare (gpointer data)
{
  gzochid_data_transaction_context *context = 
    (gzochid_data_transaction_context *) data;
  GList *references = g_hash_table_get_values (context->oids_to_references);
  GList *reference_ptr = references;
  gboolean flush_failed = FALSE;
  
  gzochid_storage_lock (context->context->oids);
  gzochid_storage_lock (context->context->names);

  while (reference_ptr != NULL)
    {
      if (!flush_reference (reference_ptr->data, context))
	{
	  flush_failed = TRUE;
	  break;
	}

      reference_ptr = reference_ptr->next;
    }
  g_list_free (references);

  if (flush_failed)
    return FALSE;

  gzochid_storage_transaction_prepare (context->transaction);
  if (context->transaction->rollback)
    return FALSE;

  return TRUE;
}

static void finalize_references (gzochid_data_transaction_context *context)
{
  GList *references = g_hash_table_get_values (context->oids_to_references);
  GList *reference_ptr = references;

  while (reference_ptr != NULL)
    {
      gzochid_data_managed_reference *reference =
	(gzochid_data_managed_reference *) reference_ptr->data;

      if (reference->obj != NULL)
	{
	  reference->serialization->finalizer 
	    (context->context, reference->obj);
	  reference->obj = NULL;
	}
      reference_ptr = reference_ptr->next;
    }
  g_list_free (references);
}

static void data_commit (gpointer data)
{
  gzochid_data_transaction_context *context = 
    (gzochid_data_transaction_context *) data;

  gzochid_storage_transaction_commit (context->transaction);

  finalize_references (context);

  gzochid_storage_unlock (context->context->names);
  gzochid_storage_unlock (context->context->oids);

  transaction_context_free (context);
}

static void data_rollback (gpointer data)
{
  gzochid_data_transaction_context *context = 
    (gzochid_data_transaction_context *) data;

  gzochid_storage_transaction_rollback (context->transaction);

  finalize_references (context);

  gzochid_storage_unlock (context->context->names);
  gzochid_storage_unlock (context->context->oids);

  transaction_context_free (context);
}

static gzochid_transaction_participant data_participant = 
  { "data", data_prepare, data_commit, data_rollback };

static gzochid_data_oid_block *create_oid_block 
(gzochid_application_context *context)
{
  char next_oid_key[] = { NEXT_OID_KEY };
  char *next_oid_value = NULL;
  size_t next_oid_value_len = 0;

  gzochid_data_oid_block *block = calloc (1, sizeof (gzochid_data_oid_block));

  mpz_init (block->first);
  mpz_init (block->next);
  mpz_init (block->last);

  gzochid_storage_lock (context->meta);

  next_oid_value = gzochid_storage_get 
    (context->meta, next_oid_key, 1, &next_oid_value_len);

  if (next_oid_value != NULL)
    {
      mpz_set_str (block->first, next_oid_value, 16);

      mpz_set (block->next, block->first);
      mpz_set (block->last, block->first);
      free (next_oid_value);
    }
  
  mpz_add_ui (block->last, block->last, ALLOCATION_BLOCK_SIZE);

  next_oid_value = mpz_get_str (NULL, 16, block->last);
  next_oid_value_len = strlen (next_oid_value) + 1;
  gzochid_storage_put 
    (context->meta, next_oid_key, 1, next_oid_value, next_oid_value_len);

  mpz_sub_ui (block->last, block->last, 1);
  
  gzochid_storage_unlock (context->meta);

  return block;
}

static gzochid_data_oid_block *reserve_oids 
(gzochid_application_context *context)
{
  gzochid_data_oid_block *block = NULL;
  
  g_mutex_lock (context->free_oids_lock);
  
  if (g_list_length (context->free_oid_blocks) > 0)
    {
      block = (gzochid_data_oid_block *) context->free_oid_blocks->data;
      context->free_oid_blocks = g_list_remove_link 
	(context->free_oid_blocks, context->free_oid_blocks);
    }
  else block = create_oid_block (context);

  g_mutex_unlock (context->free_oids_lock);
  return block;
}

static void get_binding 
(gzochid_data_transaction_context *context, char *name, mpz_t oid)
{
  char *oid_str = gzochid_storage_transaction_get 
    (context->transaction, context->context->names, name, strlen (name) + 1, 
     NULL);
  if (context->transaction->rollback)
    gzochid_transaction_mark_for_rollback 
      (&data_participant, context->transaction->should_retry);

  if (oid_str != NULL)
    {
      mpz_set_str (oid, oid_str, 16);
      free (oid_str);
    }
  else mpz_set_si (oid, -1);
}

static int set_binding 
(gzochid_data_transaction_context *context, char *name, mpz_t oid)
{
  char *oid_str = mpz_get_str (NULL, 16, oid);
  gzochid_storage_transaction_put 
    (context->transaction, context->context->names, name, strlen (name) + 1, 
     oid_str, strlen (oid_str) + 1);

  free (oid_str);

  if (context->transaction->rollback)
    {
      gzochid_transaction_mark_for_rollback 
	(&data_participant, context->transaction->should_retry);
      return 1;
    }
  else return 0;
}

static gzochid_data_managed_reference *create_empty_reference
(gzochid_application_context *context, mpz_t oid, 
 gzochid_io_serialization *serialization)
{
  gzochid_data_managed_reference *reference = 
    calloc (1, sizeof (gzochid_data_managed_reference));

  reference->context = context;

  mpz_init (reference->oid);
  mpz_set (reference->oid, oid);
 
  reference->state = GZOCHID_MANAGED_REFERENCE_STATE_EMPTY;
  reference->serialization = serialization;
  
  return reference;
}

static int next_object_id 
(gzochid_data_transaction_context *context, mpz_t oid)
{
  if (context->free_oids == NULL)
    context->free_oids = reserve_oids (context->context);

  mpz_set (oid, context->free_oids->next);
  mpz_add_ui (context->free_oids->next, context->free_oids->next, 1);

  if (mpz_cmp (context->free_oids->next, context->free_oids->last) > 0)
    {
      context->used_oid_blocks = g_list_append 
	(context->used_oid_blocks, context->free_oids);
      context->free_oids = reserve_oids (context->context);
    }

  return 0;
}

gzochid_data_managed_reference *create_new_reference
(gzochid_application_context *context, void *ptr, 
 gzochid_io_serialization *serialization)
{
  gzochid_data_transaction_context *tx_context = 
    gzochid_transaction_context (&data_participant);
  gzochid_data_managed_reference *reference = 
    calloc (1, sizeof (gzochid_data_managed_reference));

  reference->context = context;
  
  mpz_init (reference->oid);
  
  next_object_id (tx_context, reference->oid);
  reference->state = GZOCHID_MANAGED_REFERENCE_STATE_NEW;
  reference->serialization = serialization;
  reference->obj = ptr;

  return reference;
}

static gzochid_data_managed_reference *get_reference_by_oid
(gzochid_application_context *context, mpz_t oid, 
 gzochid_io_serialization *serialization)
{
  gzochid_data_transaction_context *tx_context = 
    gzochid_transaction_context (&data_participant);
  char *key = mpz_get_str (NULL, 16, oid);
  gzochid_data_managed_reference *reference = g_hash_table_lookup 
    (tx_context->oids_to_references, key);

  if (reference == NULL)
    {
      reference = create_empty_reference (context, oid, serialization);
      g_hash_table_insert (tx_context->oids_to_references, key, reference);
    }
  else free (key);

  return reference;
}

static gzochid_data_managed_reference *get_reference_by_ptr
(gzochid_application_context *context, void *ptr, 
 gzochid_io_serialization *serialization)
{
  gzochid_data_transaction_context *tx_context = 
    gzochid_transaction_context (&data_participant);
  gzochid_data_managed_reference *reference = g_hash_table_lookup 
    (tx_context->ptrs_to_references, ptr);

  if (reference == NULL)
    {
      char *key = NULL;

      reference = create_new_reference (context, ptr, serialization);
      key = mpz_get_str (NULL, 16, reference->oid);

      assert (g_hash_table_lookup 
	      (tx_context->oids_to_references, key) == NULL);
      assert (g_hash_table_lookup 
	      (tx_context->ptrs_to_references, ptr) == NULL);
      
      g_hash_table_insert (tx_context->oids_to_references, key, reference);
      g_hash_table_insert (tx_context->ptrs_to_references, ptr, reference);
    }
  
  return reference;
}

int dereference 
(gzochid_data_transaction_context *context, 
 gzochid_data_managed_reference *reference)
{
  size_t data_len = 0;
  char *oid_str = NULL; 
  char *data = NULL; 
  GString *in = NULL;
  
  if (reference->obj != NULL 
      || reference->state == GZOCHID_MANAGED_REFERENCE_STATE_REMOVED_EMPTY)
    return 0;

  oid_str = mpz_get_str (NULL, 16, reference->oid);
  gzochid_debug ("Retrieving data for reference '%s'.", oid_str);

  data = gzochid_storage_transaction_get
    (context->transaction, context->context->oids, oid_str, 
     strlen (oid_str) + 1, &data_len);

  if (context->transaction->rollback)
    {
      gzochid_transaction_mark_for_rollback 
	(&data_participant, context->transaction->should_retry);
      free (oid_str);
      return 1;
    }

  if (data == NULL)
    {
      gzochid_info ("No data found for reference '%s'.", oid_str);
      free (oid_str);
    }
  else 
    {
      gzochid_application_data_event *event = 
	malloc (sizeof (gzochid_application_data_event));
      gzochid_application_event *base_event = 
	(gzochid_application_event *) event;

      base_event->type = BYTES_READ;
      gettimeofday (&base_event->timestamp, NULL);
      event->bytes = data_len;

      gzochid_application_event_dispatch
	(context->context->event_source, base_event);

      in = g_string_new_len (data, data_len);
      reference->obj = reference->serialization->deserializer 
	(context->context, in);
      reference->state = GZOCHID_MANAGED_REFERENCE_STATE_NOT_MODIFIED;
      
      g_hash_table_insert (context->oids_to_references, oid_str, reference);
      g_hash_table_insert 
	(context->ptrs_to_references, reference->obj, reference);

      g_string_free (in, TRUE);
    }

  return 0;
}

static int remove_object (gzochid_data_transaction_context *context, mpz_t oid)
{
  char *oid_str = mpz_get_str (NULL, 16, oid);

  /* The transaction is presumed to have already been joined at this point. */
  
  gzochid_data_transaction_context *tx_context =
    gzochid_transaction_context (&data_participant);

  gzochid_debug ("Removing reference '%s'.", oid_str);      
  gzochid_storage_transaction_delete 
    (context->transaction, context->context->oids, oid_str, 
     strlen (oid_str) + 1);

  if (tx_context->transaction->rollback)
    {
      gzochid_transaction_mark_for_rollback 
	(&data_participant, tx_context->transaction->should_retry);
      return 1;
    }
  else return 0;
}

static int remove_reference 
(gzochid_data_transaction_context *context, 
 gzochid_data_managed_reference *reference)
{
  switch (reference->state)
    {
    case GZOCHID_MANAGED_REFERENCE_STATE_EMPTY:
      
      if (remove_object (context, reference->oid) != 0)
	return 1;

      reference->state = GZOCHID_MANAGED_REFERENCE_STATE_REMOVED_EMPTY;
      break;

    case GZOCHID_MANAGED_REFERENCE_STATE_NOT_MODIFIED:
    case GZOCHID_MANAGED_REFERENCE_STATE_MODIFIED:

      if (remove_object (context, reference->oid) != 0)
	return 1;

    case GZOCHID_MANAGED_REFERENCE_STATE_NEW:
      reference->state = GZOCHID_MANAGED_REFERENCE_STATE_REMOVED_FETCHED;
    default: break;
    }

  return 0;
}

static void join_transaction (gzochid_application_context *context)
{
  if (!gzochid_transaction_active ()
      || gzochid_transaction_context (&data_participant) == NULL)
    {
      gzochid_data_transaction_context *tx_context = NULL;
      if (gzochid_transaction_timed ())
	{
	  struct timeval remaining = gzochid_transaction_time_remaining ();
	  tx_context = create_transaction_context (context, &remaining);
	}
      else tx_context = create_transaction_context (context, NULL); 
      gzochid_transaction_join (&data_participant, tx_context);
    }
}

void *gzochid_data_get_binding
(gzochid_application_context *context, char *name, 
 gzochid_io_serialization *serialization)
{
  mpz_t oid;
  gzochid_data_managed_reference *reference = NULL;
  gzochid_data_transaction_context *tx_context = NULL;

  join_transaction (context);
  tx_context = gzochid_transaction_context (&data_participant);

  mpz_init (oid);
  get_binding (tx_context, name, oid);

  if (mpz_cmp_si (oid, -1) == 0)
    {
      mpz_clear (oid);
      return NULL;
    }

  reference = get_reference_by_oid (context, oid, serialization);  
  mpz_clear (oid);

  dereference (tx_context, reference);
  return reference->obj;
}

int gzochid_data_set_binding_to_oid
(gzochid_application_context *context, char *name, mpz_t oid)
{
  gzochid_data_transaction_context *tx_context = NULL;
 
  join_transaction (context);
  tx_context = gzochid_transaction_context (&data_participant);
  return set_binding (tx_context, name, oid);
}

char *gzochid_data_next_binding_oid
(gzochid_application_context *context, char *key, mpz_t oid)
{
  char *next_key = NULL;
  gzochid_data_transaction_context *tx_context = NULL;
 
  join_transaction (context);
  tx_context = gzochid_transaction_context (&data_participant);
  next_key = gzochid_storage_transaction_next_key 
    (tx_context->transaction, tx_context->context->names, key, 
     strlen (key) + 1, NULL);
  if (tx_context->transaction->rollback)
    gzochid_transaction_mark_for_rollback 
      (&data_participant, tx_context->transaction->should_retry);

  if (next_key != NULL)
    {
      char *next_value = gzochid_storage_transaction_get 
	(tx_context->transaction, tx_context->context->names, next_key, 
	 strlen (next_key) + 1, NULL);
      if (tx_context->transaction->rollback)
	gzochid_transaction_mark_for_rollback 
	  (&data_participant, tx_context->transaction->should_retry);

      if (next_value != NULL)
	{
	  mpz_set_str (oid, next_value, 16);
	  free (next_value);
	  return next_key;
	}
      else return NULL;
    }
  else return NULL;
}

int gzochid_data_set_binding 
(gzochid_application_context *context, char *name, 
 gzochid_io_serialization *serialization, void *data)
{
  gzochid_data_managed_reference *reference = NULL;
  gzochid_data_transaction_context *tx_context = NULL;
 
  join_transaction (context);
  tx_context = gzochid_transaction_context (&data_participant);

  reference = get_reference_by_ptr (context, data, serialization);
  return set_binding (tx_context, name, reference->oid);
}

int gzochid_data_remove_binding
(gzochid_application_context *context, char *name)
{
  mpz_t oid;
  char *oid_str = NULL;
  gzochid_data_transaction_context *tx_context = NULL;

  mpz_init (oid);

  join_transaction (context);
  tx_context = gzochid_transaction_context (&data_participant);

  get_binding (tx_context, name, oid);
  oid_str = mpz_get_str (NULL, 16, oid);
  mpz_clear (oid);

  gzochid_storage_transaction_delete 
    (tx_context->transaction, tx_context->context->names, name, 
     strlen (name) + 1);

  free (oid_str);

  if (tx_context->transaction->rollback)
    {
      gzochid_transaction_mark_for_rollback 
	(&data_participant, tx_context->transaction->should_retry);
      return 1;
    }
  else return 0;
}

gboolean gzochid_data_binding_exists
(gzochid_application_context *context, char *name)
{
  mpz_t oid;
  gboolean ret = TRUE;

  gzochid_data_transaction_context *tx_context = NULL;

  mpz_init (oid);

  join_transaction (context);
  tx_context = gzochid_transaction_context (&data_participant);

  get_binding (tx_context, name, oid);
  
  if (mpz_cmp_si (oid, -1) == 0)
    ret = FALSE;

  mpz_clear (oid);
  return ret;
}

gzochid_data_managed_reference *gzochid_data_create_reference
(gzochid_application_context *context, 
 gzochid_io_serialization *serialization, void *ptr)
{
  join_transaction (context);
  return get_reference_by_ptr (context, ptr, serialization);
}

static void persist_data (gpointer data)
{
  gzochid_data_managed_reference_holder *reference_holder = 
    (gzochid_data_managed_reference_holder *) data;

  reference_holder->reference = gzochid_data_create_reference 
    (reference_holder->context, 
     reference_holder->serialization, 
     reference_holder->data);
}

static void application_persist_data 
(gzochid_application_context *context, gzochid_auth_identity *identity, 
 gpointer data)
{
  if (gzochid_transaction_active ())
    persist_data (data);
  else 
    {
      gzochid_data_managed_reference_holder *reference_holder = 
	(gzochid_data_managed_reference_holder *) data;
      
      gzochid_data_managed_reference_holder sub_reference_holder;
      
      sub_reference_holder.context = reference_holder->context;
      sub_reference_holder.data = reference_holder->data;
      sub_reference_holder.reference = NULL;
      sub_reference_holder.serialization = reference_holder->serialization;

      gzochid_application_transaction_execute 
	(context, persist_data, &sub_reference_holder);
      reference_holder->reference = sub_reference_holder.reference;
    }
}

gzochid_data_managed_reference *gzochid_data_create_reference_sync
(gzochid_application_context *context, gzochid_auth_identity *identity,
 gzochid_io_serialization *serialization, void *ptr)
{
  gzochid_data_managed_reference_holder reference_holder;

  reference_holder.context = context;
  reference_holder.data = ptr;
  reference_holder.reference = NULL;
  reference_holder.serialization = serialization;

  if (gzochid_transaction_active ())
    {
      gzochid_game_context *game_context = 
	(gzochid_game_context *) ((gzochid_context *) context)->parent;

      gzochid_application_task application_task;
      gzochid_task task;

      application_task.worker = application_persist_data;
      application_task.context = context;
      application_task.identity = identity;
      application_task.data = &reference_holder;

      task.worker = gzochid_application_task_thread_worker;
      task.data = &application_task;

      gzochid_schedule_run_task (game_context->task_queue, &task);

      /* Clone the reference in to the current transaction. */
      
      reference_holder.reference = gzochid_data_create_reference_to_oid 
	(context, serialization, reference_holder.reference->oid);
    }
  else gzochid_application_transaction_execute 
	 (context, persist_data, &reference_holder);
  return reference_holder.reference;
}

gzochid_data_managed_reference *gzochid_data_create_reference_to_oid
(gzochid_application_context *context, 
 gzochid_io_serialization *serialization, mpz_t oid)
{
  join_transaction (context);
  return get_reference_by_oid (context, oid, serialization);
}

int gzochid_data_dereference (gzochid_data_managed_reference *reference)
{
  join_transaction (reference->context);
  return dereference 
    (gzochid_transaction_context (&data_participant), reference);
}

int gzochid_data_remove_object (gzochid_data_managed_reference *reference)
{
  join_transaction (reference->context);
  return remove_reference 
    (gzochid_transaction_context (&data_participant), reference);
}

int gzochid_data_mark 
(gzochid_application_context *context, gzochid_io_serialization *serialization,
 void *ptr)
{
  gzochid_data_managed_reference *reference = NULL;
  join_transaction (context);

  reference = get_reference_by_ptr (context, ptr, serialization);

  if (reference->state != GZOCHID_MANAGED_REFERENCE_STATE_NEW)
    {
      char *data = NULL;
      char *oid_str = mpz_get_str (NULL, 16, reference->oid); 
      gzochid_data_transaction_context *tx_context = 
	gzochid_transaction_context (&data_participant);

      reference->state = GZOCHID_MANAGED_REFERENCE_STATE_MODIFIED;
      gzochid_debug ("Marking reference '%s' for update.", oid_str);

      data = gzochid_storage_transaction_get_for_update
	(tx_context->transaction, tx_context->context->oids, oid_str, 
	 strlen (oid_str) + 1, NULL);
   
      free (oid_str);

      if (tx_context->transaction->rollback)
	{
	  gzochid_transaction_mark_for_rollback 
	    (&data_participant, tx_context->transaction->should_retry);
	  return 1;
	}
      else 
	{
	  free (data);
	  return 0;
	}
    }
  return 0;
}
