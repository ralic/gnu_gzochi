/* mock-app.c: Test-time replacements for app.c routines.
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

#include <glib.h>
#include <stddef.h>
#include <stdlib.h>

#include "../app.h"
#include "../auth.h"
#include "../storage.h"
#include "../tx.h"

gzochid_application_context *gzochid_application_context_new ()
{
  gzochid_application_context *context = 
    calloc (1, sizeof (gzochid_application_context));

  context->free_oids_lock = g_mutex_new ();
  context->client_mapping_lock = g_mutex_new ();

  context->meta = gzochid_storage_open ("/dev/null");
  context->oids = gzochid_storage_open ("/dev/null");
  context->names = gzochid_storage_open ("/dev/null");

  return context;
}

void gzochid_application_task_worker (gpointer data)
{
  gzochid_application_task *task = (gzochid_application_task *) data;
  task->worker (task->context, task->identity, task->data);
}

void gzochid_application_task_thread_worker (gpointer data, gpointer user_data)
{
  gzochid_application_task_worker (data);
}

static gzochid_transactional_application_task_execution *execution_new
(gzochid_application_task *task, struct timeval *timeout)
{
  gzochid_transactional_application_task_execution *execution = 
    malloc (sizeof (gzochid_transactional_application_task_execution));

  execution->task = task;

  if (timeout != NULL)
    {
      execution->timeout = malloc (sizeof (struct timeval));
      execution->timeout->tv_sec = timeout->tv_sec;
      execution->timeout->tv_usec = timeout->tv_usec;
    }
  else execution->timeout = NULL;

  execution->attempts = 0;
  execution->result = GZOCHID_TRANSACTION_PENDING;

  return execution;
}

gzochid_transactional_application_task_execution *
gzochid_transactional_application_task_execution_new 
(gzochid_application_task *task)
{
  return execution_new (task, NULL);
}

gzochid_transactional_application_task_execution *
gzochid_transactional_application_task_timed_execution_new 
(gzochid_application_task *task, struct timeval timeout)
{
  return execution_new (task, &timeout);
}

void gzochid_serialize_application_task 
(gzochid_application_context *context, 
 gzochid_application_task_serialization *serialization, 
 gzochid_application_task *task, GString *out)
{
}

gzochid_application_task *gzochid_deserialize_application_task 
(gzochid_application_context *context, 
 gzochid_application_task_serialization *serialization, GString *in)
{
  return NULL;
}

void gzochid_transactional_application_task_execution_free
(gzochid_transactional_application_task_execution *execution)
{
}

static void transactional_task_worker (gpointer data)
{
  void **args = (void **) data;
  
  gzochid_application_context *context = 
    (gzochid_application_context *) args[0];
  gzochid_auth_identity *identity = (gzochid_auth_identity *) args[1];
  gzochid_application_task *task = (gzochid_application_task *) args[2];
  
  task->worker (context, identity, task->data);
}

void gzochid_application_transactional_task_worker 
(gzochid_application_context *context, gzochid_auth_identity *identity, 
 gpointer data)
{
  void *args[3];
  gzochid_transactional_application_task_execution *execution = 
    (gzochid_transactional_application_task_execution *) data;

  args[0] = context;
  args[1] = identity;
  args[2] = execution->task;

  if (execution->timeout != NULL)
    execution->result = gzochid_transaction_execute_timed 
      (transactional_task_worker, args, *execution->timeout);
  else execution->result = 
         gzochid_transaction_execute (transactional_task_worker, args);

  execution->attempts++;
}

void gzochid_application_resubmitting_transactional_task_worker 
(gzochid_application_context *app_context, gzochid_auth_identity *identity, 
 gpointer data)
{
  gzochid_transactional_application_task_execution *execution = 
    (gzochid_transactional_application_task_execution *) data;
  
  gzochid_application_transactional_task_worker 
    (app_context, identity, execution);
}
