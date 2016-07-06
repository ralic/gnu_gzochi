/* durable-task.c: Persistent task management routines for gzochid
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

#include <assert.h>
#include <glib.h>
#include <gmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "app.h"
#include "auth_int.h"
#include "durable-task.h"
#include "gzochid-auth.h"
#include "game.h"
#include "txlog.h"
#include "util.h"

#define PENDING_TASK_PREFIX "s.pendingTask."

GHashTable *serialization_registry = NULL;

struct _gzochid_durable_application_task
{
  mpz_t handle_oid;
};

typedef struct _gzochid_durable_application_task
gzochid_durable_application_task;

struct _gzochid_durable_application_task_handle
{
  gzochid_application_task_serialization *serialization;

  /* The named binding for this durable task, or `NULL' if not durably 
     persisted for resubmit on bootstrap. */

  char *binding;

  gzochid_application_worker task_worker;
  gzochid_data_managed_reference *task_data_reference;

  gzochid_auth_identity *identity;
  
  gboolean repeats;
  struct timeval period;
  struct timeval target_execution_time;
};

static int 
task_prepare (gpointer data)
{
  return TRUE;
}

static gzochid_task_transaction_context *
create_transaction_context (gzochid_application_context *app_context)
{
  gzochid_task_transaction_context *tx_context = 
    calloc (1, sizeof (gzochid_task_transaction_context));

  tx_context->context = app_context;

  return tx_context;
}

static void 
cleanup_transaction (gzochid_task_transaction_context *tx_context)
{
  g_list_free_full
    (tx_context->scheduled_tasks, (GDestroyNotify) gzochid_task_free);
  free (tx_context);
}

static void 
commit_scheduled_task (gpointer task, gpointer user_data)
{
  gzochid_task_transaction_context *tx_context = user_data;
  gzochid_application_context *context = tx_context->context;
  gzochid_game_context *game_context = (gzochid_game_context *)
    ((gzochid_context *) context)->parent;

  gzochid_schedule_submit_task (game_context->task_queue, task);
}

static void 
task_commit (gpointer data)
{
  gzochid_task_transaction_context *tx_context = data;

  g_list_foreach (tx_context->scheduled_tasks, commit_scheduled_task, data);

  cleanup_transaction (tx_context);
}

static void 
task_rollback (gpointer data)
{ 
  gzochid_task_transaction_context *tx_context = data;
  cleanup_transaction (tx_context);
}

static gzochid_transaction_participant task_participant = 
  { "task", task_prepare, task_commit, task_rollback };

static gzochid_task_transaction_context *
join_transaction (gzochid_application_context *context)
{
  gzochid_task_transaction_context *tx_context = NULL;

  if (!gzochid_transaction_active ()
      || (tx_context = gzochid_transaction_context (&task_participant)) == NULL)
    {
      tx_context = create_transaction_context (context);
      gzochid_transaction_join (&task_participant, tx_context);
    }

  return tx_context;
}

gzochid_application_task_serialization *
gzochid_lookup_task_serialization (char *name)
{
  return g_hash_table_lookup (serialization_registry, name);
}

void 
gzochid_task_register_serialization 
(gzochid_application_task_serialization *serialization)
{
  g_hash_table_insert 
    (serialization_registry, serialization->name, serialization);
}

void 
gzochid_task_initialize_serialization_registry (void)
{
  serialization_registry = g_hash_table_new (g_str_hash, g_str_equal);
}

gzochid_application_worker_serialization 
received_message_worker_serialization = { NULL, NULL };

gzochid_io_serialization 
received_message_data_serialization = { NULL, NULL, NULL };

gzochid_application_task_serialization 
gzochid_client_received_message_task_serialization = 
  { 
    "received-message",
    &received_message_worker_serialization, 
    &received_message_data_serialization 
  };

void 
gzochid_register_client_received_message_task_serialization (void)
{
  gzochid_task_register_serialization 
    (&gzochid_client_received_message_task_serialization);
}

gzochid_durable_application_task *
gzochid_durable_application_task_new (mpz_t handle_oid)
{
  gzochid_durable_application_task *task = 
    malloc (sizeof (gzochid_durable_application_task));

  mpz_init (task->handle_oid);
  mpz_set (task->handle_oid, handle_oid);

  return task;
}

gzochid_durable_application_task_handle *
create_durable_task_handle 
(gzochid_data_managed_reference *task_data_reference,
 gzochid_application_task_serialization *serialization,
 gzochid_auth_identity *identity, struct timeval target_execution_time)
{
  gzochid_durable_application_task_handle *durable_task_handle = 
    malloc (sizeof (gzochid_durable_application_task_handle));
  struct timeval immediate = { 0, 0 };

  durable_task_handle->task_data_reference = task_data_reference;
  durable_task_handle->serialization = serialization;
  durable_task_handle->binding = NULL;
  durable_task_handle->identity = gzochid_auth_identity_ref (identity);
  durable_task_handle->repeats = FALSE;
  durable_task_handle->period = immediate;
  durable_task_handle->target_execution_time = target_execution_time;

  return durable_task_handle;
}

gzochid_durable_application_task_handle *
create_durable_periodic_task_handle
(gzochid_data_managed_reference *task_data_reference, 
 gzochid_application_task_serialization *serialization, 
 gzochid_auth_identity *identity, struct timeval target_execution_time, 
 struct timeval period)
{
  gzochid_durable_application_task_handle *durable_task_handle = 
    create_durable_task_handle 
    (task_data_reference, serialization, identity, target_execution_time);

  durable_task_handle->repeats = TRUE;
  durable_task_handle->period = period;
  
  return durable_task_handle;
}

void 
gzochid_durable_application_task_free (gzochid_durable_application_task *task)
{
  free (task);
}

gpointer 
deserialize_durable_task_handle
(gzochid_application_context *context, GString *in, GError **err)
{
  gzochid_durable_application_task_handle *handle = 
    malloc (sizeof (gzochid_durable_application_task_handle));
  char *serialization_name = NULL;

  mpz_t oid;

  mpz_init (oid);
  gzochid_util_deserialize_mpz (in, oid);
  
  serialization_name = gzochid_util_deserialize_string (in);
  handle->serialization = gzochid_lookup_task_serialization 
    (serialization_name);

  assert (handle->serialization != NULL);

  if (gzochid_util_deserialize_boolean (in))
    handle->binding = gzochid_util_deserialize_string (in);
  else handle->binding = NULL;
  
  handle->task_worker = 
    handle->serialization->worker_serialization->deserializer (context, in);
  handle->task_data_reference = gzochid_data_create_reference_to_oid 
    (context, handle->serialization->data_serialization, oid);

  mpz_clear (oid);
 
  handle->identity = gzochid_auth_identity_deserializer (context, in, NULL);
  handle->repeats = gzochid_util_deserialize_boolean (in);

  if (handle->repeats)
    handle->period = gzochid_util_deserialize_timeval (in);

  handle->target_execution_time = gzochid_util_deserialize_timeval (in);
  
  free (serialization_name);
  return handle;
}

void 
serialize_durable_task_handle
(gzochid_application_context *context, gpointer data, GString *out, 
 GError **err)
{
  gzochid_durable_application_task_handle *handle = data;

  gzochid_util_serialize_mpz (handle->task_data_reference->oid, out);
  gzochid_util_serialize_string (handle->serialization->name, out);

  gzochid_util_serialize_boolean (handle->binding != NULL, out);
  if (handle->binding != NULL)
    gzochid_util_serialize_string (handle->binding, out);

  handle->serialization->worker_serialization->serializer 
    (context, handle->task_worker, out);

  gzochid_auth_identity_serializer (context, handle->identity, out, NULL);

  gzochid_util_serialize_boolean (handle->repeats, out);
  if (handle->repeats)
    gzochid_util_serialize_timeval (handle->period, out);

  gzochid_util_serialize_timeval (handle->target_execution_time, out);
}

static void
finalize_durable_task_handle (gzochid_application_context *context, 
			      gpointer data)
{
  gzochid_durable_application_task_handle *handle = data;

  if (handle->binding != NULL)
    free (handle->binding);
  
  gzochid_auth_identity_finalizer (context, handle->identity);
  free (handle);
}

gzochid_io_serialization
gzochid_durable_application_task_handle_serialization = 
  { 
    serialize_durable_task_handle, 
    deserialize_durable_task_handle, 
    finalize_durable_task_handle
  };

static void durable_task_application_worker
(gzochid_application_context *, gzochid_auth_identity *, gpointer);
static void durable_task_catch_worker
(gzochid_application_context *, gzochid_auth_identity *, gpointer);
static void durable_task_cleanup_worker
(gzochid_application_context *, gzochid_auth_identity *, gpointer);

static void 
remove_durable_task (gzochid_application_context *context, mpz_t oid, 
		     GError **err)
{
  GError *local_err = NULL;
  gzochid_data_managed_reference *reference =
    gzochid_data_create_reference_to_oid
    (context, &gzochid_durable_application_task_handle_serialization, oid);   
  gzochid_durable_application_task_handle *task_handle =
    gzochid_data_dereference (reference, &local_err);

  if (task_handle == NULL)
    {
      g_propagate_error (err, local_err);
      return;
    }

  if (task_handle->binding != NULL)
    gzochid_data_remove_binding
      (reference->context, task_handle->binding, &local_err);
  
  if (local_err == NULL)
    {
      gzochid_data_remove_object (reference, &local_err);
      if (local_err != NULL)
	g_propagate_error (err, local_err);
    }
  else g_propagate_error (err, local_err);
}

/*
  Returns a newly-allocated string by concatenating "s.pendingTask." with the
  base-16 representation of the specified object id. When written to the `names'
  store, this binding will be used to resubmit the durable task that has the
  specified oid.
  
  This string should be freed via `free' when no longer in use.
*/

static char *
create_pending_task_binding (mpz_t oid)
{
  char *binding = calloc (mpz_sizeinbase (oid, 16) + 15, sizeof (char));
  char *oid_str = mpz_get_str (NULL, 16, oid);

  binding = strcat (binding, PENDING_TASK_PREFIX);
  binding = strcat (binding, oid_str);

  free (oid_str);

  return binding;
}

void 
gzochid_restart_tasks (gzochid_application_context *context)
{
  mpz_t oid;
  GError *err = NULL;
  char *next_binding = NULL;
  int num_tasks = 0;

  mpz_init (oid);
  gzochid_tx_info (context, "Resubmitting durable tasks.");
  next_binding = gzochid_data_next_binding_oid 
    (context, PENDING_TASK_PREFIX, oid, &err);

  assert (err == NULL);

  while (next_binding != NULL 
	 && strncmp (PENDING_TASK_PREFIX, next_binding, 14) == 0)
    {
      char *next_next_binding = NULL;
      gzochid_data_managed_reference *handle_reference = 
	gzochid_data_create_reference_to_oid 
	(context, &gzochid_durable_application_task_handle_serialization, oid);

      gzochid_data_dereference (handle_reference, &err);

      if (err != NULL)
	{
	  assert (err->code == GZOCHID_DATA_ERROR_NOT_FOUND);

	  gzochid_tx_warning 
	    (context, "Task handle not found for resubmitted task.");
	  g_error_free (err);

	  err = NULL;
	  
	  gzochid_data_remove_binding (context, next_binding, &err);
	  assert (err == NULL);
	}
      else 
	{
	  gzochid_schedule_durable_task_handle
	    (context, handle_reference->obj, &err);

	  /* There's no reason this should fail: In this context it's being 
	     scheduled with a single active thread with an infinite transaction
	     timeout. */
	  
	  assert (err == NULL);
	  
	  num_tasks++;
	}

      next_next_binding = 
	gzochid_data_next_binding_oid (context, next_binding, oid, &err);

      assert (err == NULL);

      free (next_binding);
      next_binding = next_next_binding;
    }

  if (num_tasks == 1)
    gzochid_tx_info (context, "Resubmitted 1 task.", num_tasks);
  else if (num_tasks > 1)
    gzochid_tx_info (context, "Resubmitted %d tasks.", num_tasks);
  else gzochid_tx_info (context, "No tasks found to resubmit.");

  mpz_clear (oid);  
}

gzochid_durable_application_task_handle *
gzochid_create_durable_application_task_handle
(gzochid_application_task *task, 
 gzochid_application_task_serialization *serialization, 
 struct timeval delay, struct timeval *period, GError **err)
{
  GError *local_err = NULL;
  struct timeval target;

  gzochid_data_managed_reference *task_data_reference = NULL;

  gettimeofday (&target, NULL);

  target.tv_sec += delay.tv_sec;
  target.tv_usec += delay.tv_usec;

  task_data_reference = gzochid_data_create_reference 
    (task->context, serialization->data_serialization, task->data, &local_err);

  /* Creating a new reference may fail if the transaction's in a bad state. */
  
  if (local_err != NULL)
    {
      g_propagate_error (err, local_err);
      return NULL;
    }
  
  if (period != NULL)
    return create_durable_periodic_task_handle
      (task_data_reference, serialization, task->identity, target, *period);
  return create_durable_task_handle
    (task_data_reference, serialization, task->identity, target);
}
 
static void 
durable_task_application_worker (gzochid_application_context *context, 
				 gzochid_auth_identity *identity, gpointer data)
{
  GError *err = NULL;
  gzochid_durable_application_task *task = data;
  gzochid_data_managed_reference *handle_reference =
    gzochid_data_create_reference_to_oid
    (context, &gzochid_durable_application_task_handle_serialization, 
     task->handle_oid);
  gzochid_durable_application_task_handle *handle = NULL;
  gzochid_application_task *inner_task = NULL;

  gzochid_data_dereference (handle_reference, &err);

  if (err != NULL)
    {
      g_error_free (err);
      return;
    }
      
  handle = handle_reference->obj;
  gzochid_data_dereference (handle->task_data_reference, &err);

  if (err != NULL)
    {
      g_error_free (err);
      return;
    }

  inner_task = gzochid_application_task_new 
    (context, identity, handle->task_worker, handle->task_data_reference->obj);
  inner_task->worker (context, identity, inner_task->data);
  free (inner_task);

  if (gzochid_transaction_rollback_only ())

    /* The body of the task worker may have done something to cause the
       transaction to be rolled back. If that's the case, we should not bother
       to attempt any logic that involves the data manager. */

    return;

  /* The repeats status of the task may have changed while executing the 
     worker, so check again to see if it needs to be removed. */

  if (! handle->repeats)
    {
      remove_durable_task (context, task->handle_oid, &err);

      if (err != NULL)
	{
	  g_error_free (err);
	  return;
	}
    }
  else
    {
      struct timeval now;

      gettimeofday (&now, NULL);
      timeradd (&handle->period, 
		&handle->target_execution_time, 
		&handle->target_execution_time);

      gzochid_data_mark 
	(context, &gzochid_durable_application_task_handle_serialization, 
	 handle, &err);

      if (err == NULL)
	gzochid_schedule_durable_task_handle (context, handle, NULL);
      else g_error_free (err);
    }
}

static void 
durable_task_catch_worker (gzochid_application_context *context, 
			   gzochid_auth_identity *identity, gpointer data)
{
  mpz_t oid;

  mpz_init (oid);
  mpz_set_str (oid, data, 16);
  
  remove_durable_task (context, oid, NULL);

  mpz_clear (oid);
}

static void
durable_task_cleanup_worker (gzochid_application_context *context,
			     gzochid_auth_identity *identity, gpointer data)
{
  free (data);
}

void 
gzochid_schedule_durable_task 
(gzochid_application_context *context, gzochid_auth_identity *identity,
 gzochid_application_task *task, 
 gzochid_application_task_serialization *serialization)
{
  struct timeval immediate = { 0, 0 };
  gzochid_schedule_delayed_durable_task 
    (context, identity, task, serialization, immediate);
}

void 
gzochid_schedule_delayed_durable_task
(gzochid_application_context *context, gzochid_auth_identity *identity,
 gzochid_application_task *task, 
 gzochid_application_task_serialization *serialization, 
 struct timeval delay)
{
  gzochid_durable_application_task_handle *handle = 
    gzochid_create_durable_application_task_handle
    (task, serialization, delay, NULL, NULL);

  if (handle != NULL)
    gzochid_schedule_durable_task_handle (context, handle, NULL);
}

gzochid_periodic_task_handle *
gzochid_schedule_periodic_durable_task 
(gzochid_application_context *context, gzochid_auth_identity *identity, 
 gzochid_application_task *task,
 gzochid_application_task_serialization *serialization, 
 struct timeval delay, struct timeval period)
{
  gzochid_durable_application_task_handle *handle = 
    gzochid_create_durable_application_task_handle
    (task, serialization, delay, &period, NULL);

  if (handle != NULL)
    {
      GError *err = NULL;

      gzochid_schedule_durable_task_handle (context, handle, &err);

      if (err == NULL)
	return handle;
      else
	{
	  g_debug
	    ("Failed to schedule durable task in transaction: %s",
	     err->message);

	  g_error_free (err);
	  return NULL;
	}
    }
  else return NULL;
}

void gzochid_schedule_durable_task_handle
(gzochid_application_context *app_context,
 gzochid_durable_application_task_handle *task_handle, GError **err)
{
  char *oid_str = NULL;
  GError *local_err = NULL;

  gzochid_game_context *game_context = (gzochid_game_context *) 
    ((gzochid_context *) app_context)->parent;
  gzochid_auth_identity *cloned_identity = NULL;

  gzochid_application_task *transactional_task = NULL;
  gzochid_application_task *catch_task = NULL;
  gzochid_application_task *cleanup_task = NULL;
  
  gzochid_transactional_application_task_execution *execution = NULL;

  gzochid_application_task *application_task = NULL;

  gzochid_data_managed_reference *durable_task_handle_reference = 
    gzochid_data_create_reference
    (app_context, &gzochid_durable_application_task_handle_serialization,
     task_handle, NULL);
  
  gzochid_durable_application_task *durable_task = NULL;  
  gzochid_task_transaction_context *tx_context = NULL;

  /* The task handle pointer should already be cached in the current transaction
     since it must have been created and persisted via 
     `gzochid_create_durable_application_task_handle' or we are rescheduling an
     existing task handle. */
  
  assert (durable_task_handle_reference != NULL);

  durable_task = gzochid_durable_application_task_new
    (durable_task_handle_reference->oid);

  /* The memory allocated for this string is freed by the cleanup task. */
  
  oid_str = mpz_get_str (NULL, 16, durable_task_handle_reference->oid);

  task_handle->binding = create_pending_task_binding
    (durable_task_handle_reference->oid);
  
  gzochid_data_set_binding_to_oid 
    (app_context, task_handle->binding, durable_task_handle_reference->oid,
     &local_err);

  if (local_err != NULL)
    {
      free (oid_str);
      gzochid_durable_application_task_free (durable_task);      

      g_propagate_error (err, local_err);
      
      return;
    }
  
  tx_context = join_transaction (app_context);
  
  cloned_identity = gzochid_auth_identity_ref (task_handle->identity);

  transactional_task = gzochid_application_task_new
    (app_context, cloned_identity, durable_task_application_worker,
     durable_task);
  catch_task = gzochid_application_task_new
    (app_context, cloned_identity, durable_task_catch_worker, oid_str);
  cleanup_task = gzochid_application_task_new
    (app_context, cloned_identity, durable_task_cleanup_worker, oid_str);
  execution = gzochid_transactional_application_task_timed_execution_new 
    (transactional_task, catch_task, cleanup_task, game_context->tx_timeout);

  application_task = gzochid_application_task_new
    (app_context, cloned_identity,
     gzochid_application_resubmitting_transactional_task_worker, execution);

  tx_context->scheduled_tasks = g_list_append 
    (tx_context->scheduled_tasks,
     gzochid_task_new (gzochid_application_task_thread_worker,
		       application_task, task_handle->target_execution_time));
}


  if (err == NULL)
    {
      gzochid_data_managed_reference *handle_reference = 
	gzochid_data_create_reference 
	(context, &gzochid_durable_application_task_handle_serialization, 
	 handle, &err);

      /* Creating a new reference may fail if the transaction's in a bad 
	 state. */

      if (err == NULL)
	schedule_durable_task 
	  (context, wrap_durable_task (context, handle_reference->oid, NULL));
    }

  if (err != NULL)
    {
      g_error_free (err);
      return NULL;
    }
  else return handle;
      
    }

}

void 
gzochid_cancel_periodic_task (gzochid_application_context *context, 
			      gzochid_periodic_task_handle *handle)
{
  handle->repeats = FALSE;

  gzochid_data_mark 
    (context, &gzochid_durable_application_task_handle_serialization, 
     handle, NULL);
}
