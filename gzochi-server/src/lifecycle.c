/* lifecycle.c: Handlers for gzochi application lifecycle events
 * Copyright (C) 2015 Julian Graham
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
#include <stddef.h>

#include "app.h"
#include "app-task.h"
#include "auth_int.h"
#include "fsm.h"
#include "game.h"
#include "guile.h"
#include "gzochid-auth.h"
#include "lifecycle.h"
#include "log.h"
#include "protocol.h"
#include "scheme-task.h"
#include "session.h"
#include "task.h"

G_LOCK_DEFINE_STATIC (load_path);

static void 
initialize_async (gpointer data, gpointer user_data)
{
  gzochid_context *context = data;
  gzochid_fsm_to_state (context->fsm, GZOCHID_APPLICATION_STATE_RUNNING);
}

static void 
initialize_auth (int from_state, int to_state, gpointer user_data)
{
  gzochid_context *context = user_data;
  gzochid_application_context *app_context = 
    (gzochid_application_context *) context;
  gzochid_game_context *game_context = (gzochid_game_context *) context->parent;

  if (app_context->descriptor->auth_type != NULL)
    {
      if (g_hash_table_contains 
	  (game_context->auth_plugins, app_context->descriptor->auth_type))
	{
	  GError *error = NULL;
	  gzochid_auth_plugin *plugin = g_hash_table_lookup 
	    (game_context->auth_plugins, app_context->descriptor->auth_type);

	  gzochid_debug 
	    ("Initializing auth plugin '%s' for application '%'.",
	     app_context->descriptor->auth_type,
	     app_context->descriptor->name);

	  app_context->auth_data = plugin->info->initialize 
	    (app_context->descriptor->auth_properties, &error);
	  if (error != NULL)
	    gzochid_err
	      ("Auth plugin '%s' failed to initialize "
	       "for application '%s': %s",
	       app_context->descriptor->auth_type,
	       app_context->descriptor->name,
	       error->message);
	  
	  g_clear_error (&error);
	  app_context->authenticator = plugin->info->authenticate;
	}
      else gzochid_err 
	     ("Unknown auth type '%s' for application '%s'.", 
	      app_context->descriptor->auth_type,
	      app_context->descriptor->name);
    }
  else
    {
      gzochid_info 
	("No auth plugin specified for application '%s'; " 
	 "pass-thru authentication will be used.", 
	 app_context->descriptor->name);
      app_context->authenticator = gzochid_auth_function_pass_thru;
    }
}

static void 
initialize_data (int from_state, int to_state, gpointer user_data)
{
  gzochid_context *context = user_data;
  gzochid_application_context *app_context = 
    (gzochid_application_context *) context;
  gzochid_game_context *game_context = (gzochid_game_context *) context->parent;

  char *data_dir = g_strconcat 
    (game_context->work_dir, "/", app_context->descriptor->name, NULL);
  char *meta_db = g_strconcat (data_dir, "/meta", NULL);
  char *oids_db = g_strconcat (data_dir, "/oids", NULL);
  char *names_db = g_strconcat (data_dir, "/names", NULL);

  gzochid_storage_context *storage_context = NULL;
  gzochid_storage_engine_interface *iface = APP_STORAGE_INTERFACE (app_context);

  if (!g_file_test (data_dir, G_FILE_TEST_EXISTS))
    {
      gzochid_notice 
	("Work directory %s does not exist; creating...", data_dir);
      if (g_mkdir_with_parents (data_dir, 493) != 0)
	gzochid_err ("Unable to create work directory %s.", data_dir);
    }
  else if (!g_file_test (data_dir, G_FILE_TEST_IS_DIR))
    gzochid_err ("%s is not a directory.", data_dir);

  storage_context = iface->initialize (data_dir);

  if (storage_context == NULL)
    {
      gzochid_err ("Unable to initialize storage. Exiting.");
      exit (EXIT_FAILURE);
    }

  app_context->storage_context = storage_context;
  app_context->meta = iface->open 
    (storage_context, meta_db, GZOCHID_STORAGE_CREATE);
  app_context->oids = iface->open 
    (storage_context, oids_db, GZOCHID_STORAGE_CREATE);
  app_context->names = iface->open 
    (storage_context, names_db, GZOCHID_STORAGE_CREATE);

  free (data_dir);
  free (meta_db);
  free (oids_db);
  free (names_db);
}

static void *
initialize_load_paths_guile_worker (void *data)
{
  GList *load_path_ptr = data;

  while (load_path_ptr != NULL)
    {
      gzochid_guile_add_to_load_path (load_path_ptr->data);
      load_path_ptr = load_path_ptr->next;
    }

  return NULL;
}

static void 
initialize_load_paths (int from_state, int to_state, gpointer user_data)
{
  gzochid_context *context = user_data;
  gzochid_application_context *app_context = 
    (gzochid_application_context *) context;
  
  G_LOCK (load_path);
  scm_with_guile 
    (initialize_load_paths_guile_worker, app_context->descriptor->load_paths);
  G_UNLOCK (load_path);
}

static void 
initialize_complete (int from_state, int to_state, gpointer user_data)
{
  gzochid_context *context = user_data;
  gzochid_application_context *app_context = 
    (gzochid_application_context *) context;
  gzochid_game_context *game_context = (gzochid_game_context *) context->parent;

  gzochid_game_context_register_application 
    (game_context, app_context->descriptor->name, app_context);

  gzochid_thread_pool_push
    (game_context->pool, initialize_async, user_data, NULL);
}

static void 
run_async_transactional (gpointer data)
{
  GError *err = NULL;
  gzochid_application_context *context = data;
  gboolean initialized = gzochid_data_binding_exists 
    (context, "s.initializer", &err);

  gzochid_auth_identity *system_identity = calloc 
    (1, sizeof (gzochid_auth_identity));
  system_identity->name = "[SYSTEM]";

  assert (err == NULL);

  if (!initialized)
    {
      gzochid_application_task application_task;
      gzochid_task task;

      application_task.worker = gzochid_scheme_application_initialized_worker;
      application_task.context = context;
      application_task.identity = system_identity;
      application_task.data = context->descriptor->properties;

      task.worker = gzochid_application_task_thread_worker;
      task.data = &application_task;
      gettimeofday (&task.target_execution_time, NULL);

      gzochid_schedule_execute_task (&task);
    }
  else 
    {
      gzochid_sweep_client_sessions (context, system_identity);
      gzochid_restart_tasks (context);
    }
}

static void *
run_async_guile (void *data)
{
  gzochid_application_transaction_execute 
    ((gzochid_application_context *) data, run_async_transactional, data);
  return NULL;
}

static void 
run_async (gpointer data, gpointer user_data)
{
  scm_with_guile (run_async_guile, data);
}

static void 
run (int from_state, int to_state, gpointer user_data)
{
  gzochid_context *context = user_data;
  gzochid_application_context *app_context = 
    (gzochid_application_context *) context;
  gzochid_game_context *game_context = (gzochid_game_context *) context->parent;

  if (from_state != GZOCHID_APPLICATION_STATE_INITIALIZING)
    return;

  gzochid_thread_pool_push (game_context->pool, run_async, app_context, NULL);
}

static void 
stop (int from_state, int to_state, gpointer user_data)
{
  gzochid_application_context *context = user_data;
  gzochid_storage_engine_interface *iface = APP_STORAGE_INTERFACE (context);

  if (context->meta != NULL)
    iface->close_store (context->meta);
  if (context->oids != NULL)
    iface->close_store (context->oids);
  if (context->names != NULL)
    iface->close_store (context->names);  
}

static void 
update_stats (gzochid_application_event *event, gpointer data)
{
  gzochid_stats_update_from_event (data, event);
}

void 
gzochid_application_context_init (gzochid_application_context *context, 
				  gzochid_context *parent, 
				  gzochid_application_descriptor *descriptor)
{
  char *fsm_name = g_strconcat ("app/", descriptor->name, NULL);
  gzochid_fsm *fsm = gzochid_fsm_new 
    (fsm_name, GZOCHID_APPLICATION_STATE_INITIALIZING, "INITIALIZING");

  gzochid_fsm_add_state (fsm, GZOCHID_APPLICATION_STATE_PAUSED, "PAUSED");
  gzochid_fsm_add_state (fsm, GZOCHID_APPLICATION_STATE_RUNNING, "RUNNING");
  gzochid_fsm_add_state (fsm, GZOCHID_APPLICATION_STATE_STOPPED, "STOPPED");

  gzochid_fsm_add_transition 
    (fsm, GZOCHID_APPLICATION_STATE_INITIALIZING, 
     GZOCHID_APPLICATION_STATE_RUNNING);
  gzochid_fsm_add_transition
    (fsm, GZOCHID_APPLICATION_STATE_RUNNING, 
     GZOCHID_APPLICATION_STATE_STOPPED);
  gzochid_fsm_add_transition
    (fsm, GZOCHID_APPLICATION_STATE_RUNNING, GZOCHID_APPLICATION_STATE_PAUSED);
  gzochid_fsm_add_transition
    (fsm, GZOCHID_APPLICATION_STATE_PAUSED, GZOCHID_APPLICATION_STATE_RUNNING);
  gzochid_fsm_add_transition
    (fsm, GZOCHID_APPLICATION_STATE_PAUSED, GZOCHID_APPLICATION_STATE_STOPPED);

  gzochid_fsm_on_enter 
    (fsm, GZOCHID_APPLICATION_STATE_INITIALIZING, initialize_auth, context);
  gzochid_fsm_on_enter 
    (fsm, GZOCHID_APPLICATION_STATE_INITIALIZING, initialize_data, context);
  gzochid_fsm_on_enter 
    (fsm, GZOCHID_APPLICATION_STATE_INITIALIZING, initialize_load_paths, 
     context);
  gzochid_fsm_on_enter 
    (fsm, GZOCHID_APPLICATION_STATE_INITIALIZING, initialize_complete, 
     context);
  gzochid_fsm_on_enter (fsm, GZOCHID_APPLICATION_STATE_RUNNING, run, context);
  gzochid_fsm_on_enter (fsm, GZOCHID_APPLICATION_STATE_STOPPED, stop, context);

  context->authenticator = gzochid_auth_function_pass_thru;
  context->descriptor = descriptor;
  
  gzochid_application_event_attach
    (context->event_source, update_stats, context->stats);

  gzochid_context_init ((gzochid_context *) context, parent, fsm);
}

void 
gzochid_application_client_logged_in (gzochid_application_context *context, 
				      gzochid_protocol_client *client)
{
  gzochid_game_context *game_context = 
    (gzochid_game_context *) ((gzochid_context *) context)->parent;
  gzochid_client_session *session = 
    gzochid_client_session_new (gzochid_protocol_client_get_identity (client));

  gzochid_application_task transactional_task;
  gzochid_application_task application_task;
  gzochid_transactional_application_task_execution *execution =
    gzochid_transactional_application_task_timed_execution_new 
    (&transactional_task, NULL, game_context->tx_timeout);

  gzochid_task task;

  char *session_oid_str = NULL;
  mpz_t session_oid;
  
  mpz_init (session_oid);
  gzochid_client_session_persist (context, session, session_oid);
  session_oid_str = mpz_get_str (NULL, 16, session_oid);
  mpz_clear (session_oid);

  g_mutex_lock (&context->client_mapping_lock);
  g_hash_table_insert (context->oids_to_clients, session_oid_str, client);
  g_hash_table_insert (context->clients_to_oids, client, session_oid_str);
  g_mutex_unlock (&context->client_mapping_lock);

  transactional_task.worker = gzochid_scheme_application_logged_in_worker;
  transactional_task.context = context;
  transactional_task.identity = gzochid_protocol_client_get_identity (client);
  transactional_task.data = session_oid_str;

  application_task.worker = gzochid_application_transactional_task_worker;
  application_task.context = context;
  application_task.identity = gzochid_protocol_client_get_identity (client);
  application_task.data = execution;

  task.worker = gzochid_application_task_thread_worker;
  task.data = &application_task;
  gettimeofday (&task.target_execution_time, NULL);

  while (TRUE)
    {
      gzochid_schedule_run_task (game_context->task_queue, &task);
      if (!gzochid_application_should_retry (execution))
	{
	  if (execution->result != GZOCHID_TRANSACTION_SUCCESS)	
	    {
	      gzochid_info
		("Disconnecting session '%s'; failed login transaction.", 
		 session_oid_str);

	      g_mutex_lock (&context->client_mapping_lock);
	      g_hash_table_remove (context->oids_to_clients, session_oid_str);
	      g_hash_table_remove (context->clients_to_oids, client);
	      free (session_oid_str);
	      g_mutex_unlock (&context->client_mapping_lock);

	      gzochid_protocol_client_disconnect (client);
	      
	      transactional_task.worker = 
		gzochid_client_session_disconnected_worker;
	      gzochid_schedule_run_task (game_context->task_queue, &task);      
	    }
	    break;
	}
    }

  gzochid_transactional_application_task_execution_free (execution);
}

void 
gzochid_application_client_disconnected (gzochid_application_context *context, 
					 gzochid_protocol_client *client)
{
  gzochid_game_context *game_context = 
    (gzochid_game_context *) ((gzochid_context *) context)->parent;
  char *session_oid_str = NULL;

  g_mutex_lock (&context->client_mapping_lock);
  session_oid_str = g_hash_table_lookup (context->clients_to_oids, client);
  if (session_oid_str == NULL)
    {
      g_mutex_unlock (&context->client_mapping_lock);
      return;
    }
  else 
    {
      gzochid_application_task *callback_task = 
	gzochid_application_task_new
	(context, gzochid_protocol_client_get_identity (client),
	 gzochid_scheme_application_disconnected_worker, session_oid_str);
      gzochid_application_task *cleanup_task = 
	gzochid_application_task_new
	(context, gzochid_protocol_client_get_identity (client), 
	 gzochid_client_session_disconnected_worker, session_oid_str);
      gzochid_transactional_application_task_execution *execution = 
	gzochid_transactional_application_task_execution_new 
	(callback_task, cleanup_task);
      gzochid_application_task *application_task = gzochid_application_task_new 
	(context, gzochid_protocol_client_get_identity (client),
	 gzochid_application_resubmitting_transactional_task_worker, execution);

      gzochid_task *task = NULL;      
      struct timeval n;

      gettimeofday (&n, NULL);
      
      task = gzochid_task_new
	(gzochid_application_task_thread_worker, application_task, n);

      g_hash_table_remove (context->clients_to_oids, client);
      g_hash_table_remove (context->oids_to_clients, session_oid_str);
      
      gzochid_schedule_submit_task (game_context->task_queue, task);

      g_mutex_unlock (&context->client_mapping_lock);
    }
}

void 
gzochid_application_session_received_message
(gzochid_application_context *context, gzochid_protocol_client *client, 
 unsigned char *msg, short len)
{
  gzochid_game_context *game_context = 
    (gzochid_game_context *) ((gzochid_context *) context)->parent;

  char *session_oid_str = NULL;
  void *data[3];

  g_mutex_lock (&context->client_mapping_lock);
  session_oid_str = g_hash_table_lookup (context->clients_to_oids, client);
  g_mutex_unlock (&context->client_mapping_lock);

  if (session_oid_str == NULL)
    return;
  else 
    {
      gzochid_application_task transactional_task;
      gzochid_application_task application_task;
      gzochid_transactional_application_task_execution *execution =
	gzochid_transactional_application_task_timed_execution_new
	(&transactional_task, NULL, game_context->tx_timeout);
      gzochid_task task;

      data[0] = session_oid_str;
      data[1] = msg;
      data[2] = &len;
      
      transactional_task.worker = 
	gzochid_scheme_application_received_message_worker;
      transactional_task.context = context;
      transactional_task.identity = 
	gzochid_protocol_client_get_identity (client);
      transactional_task.data = data;
      
      application_task.worker = gzochid_application_transactional_task_worker;
      application_task.context = context;
      application_task.identity = gzochid_protocol_client_get_identity (client);
      application_task.data = execution;
      
      task.worker = gzochid_application_task_thread_worker;
      task.data = &application_task;
      gettimeofday (&task.target_execution_time, NULL);

      while (TRUE)
	{
	  gzochid_schedule_run_task (game_context->task_queue, &task);
	  if (!gzochid_application_should_retry (execution))
	    break;
	}

      gzochid_transactional_application_task_execution_free (execution);
    }
}