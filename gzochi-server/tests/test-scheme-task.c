/* test-scheme-task.c: Test routines for scheme-task.c in gzochid.
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

#include <glib.h>
#include <glib-object.h>
#include <libguile.h>
#include <stddef.h>
#include <stdlib.h>

#include "app.h"
#include "app-task.h"
#include "config.h"
#include "descriptor.h"
#include "game.h"
#include "guile.h"
#include "gzochid-auth.h"
#include "oids.h"
#include "oids-storage.h"
#include "resolver.h"
#include "scheme.h"
#include "scheme-task.h"
#include "storage-mem.h"
#include "session.h"
#include "tx.h"
#include "util.h"

struct test_scheme_task_fixture
{
  GzochidResolutionContext *resolution_context;
  GzochidGameServer *game_server;
  
  gzochid_application_context *app_context;
  gzochid_auth_identity *identity;
  gzochid_storage_engine_interface *storage_interface;
};

static gboolean
ignore_warnings (const gchar *log_domain, GLogLevelFlags log_level,
		 const gchar *message, gpointer user_data)
{
  if (log_level & G_LOG_LEVEL_CRITICAL
      || log_level & G_LOG_LEVEL_WARNING)
    return FALSE;
  else return log_level & G_LOG_FLAG_FATAL;
}

static void
test_scheme_task_fixture_setup (struct test_scheme_task_fixture *fixture,
				gconstpointer user_data)
{
  GKeyFile *key_file = g_key_file_new ();
  GzochidConfiguration *configuration = g_object_new
    (GZOCHID_TYPE_CONFIGURATION, "key_file", key_file, NULL);

  fixture->app_context = gzochid_application_context_new ();
  fixture->app_context->deployment_root = "/tmp";
  fixture->app_context->descriptor = g_object_new
    (GZOCHID_TYPE_APPLICATION_DESCRIPTOR, NULL);
  fixture->app_context->identity_cache = gzochid_auth_identity_cache_new ();

  fixture->identity = gzochid_auth_identity_new ("[TEST]");

  fixture->resolution_context = g_object_new
    (GZOCHID_TYPE_RESOLUTION_CONTEXT, NULL);

  gzochid_resolver_provide
    (fixture->resolution_context, G_OBJECT (configuration), NULL);

  fixture->game_server = gzochid_resolver_require_full
    (fixture->resolution_context, GZOCHID_TYPE_GAME_SERVER, NULL);
  
  fixture->storage_interface = &gzochid_storage_engine_interface_mem;
  
  fixture->app_context->storage_engine_interface = fixture->storage_interface;
  fixture->app_context->storage_context =
    fixture->storage_interface->initialize ("/tmp");
  fixture->app_context->meta = fixture->storage_interface->open
    (fixture->app_context->storage_context, "/tmp/meta",
     GZOCHID_STORAGE_CREATE);
  fixture->app_context->oids = fixture->storage_interface->open
    (fixture->app_context->storage_context, "/tmp/oids",
     GZOCHID_STORAGE_CREATE);
  fixture->app_context->names = fixture->storage_interface->open
    (fixture->app_context->storage_context, "/tmp/names",
     GZOCHID_STORAGE_CREATE);

  fixture->app_context->oid_strategy = gzochid_storage_oid_strategy_new
    (fixture->storage_interface, fixture->app_context->storage_context,
     fixture->app_context->meta);

  g_key_file_unref (key_file);
  g_object_unref (configuration);
}

static void
test_scheme_task_fixture_teardown (struct test_scheme_task_fixture *fixture,
				   gconstpointer user_data)
{
  g_object_unref (fixture->app_context->descriptor);
  gzochid_auth_identity_cache_destroy (fixture->app_context->identity_cache);

  fixture->storage_interface->close_store (fixture->app_context->meta);
  fixture->storage_interface->close_store (fixture->app_context->oids);
  fixture->storage_interface->close_store (fixture->app_context->names);
  fixture->storage_interface->close_context
    (fixture->app_context->storage_context);

  gzochid_oid_allocation_strategy_free (fixture->app_context->oid_strategy);

  g_object_unref (fixture->game_server);
  g_object_unref (fixture->resolution_context);

  gzochid_application_context_free (fixture->app_context);
  gzochid_auth_identity_unref (fixture->identity);
}

static void
persist_client_session (struct test_scheme_task_fixture *fixture,
			gzochid_client_session *session)
{
  GByteArray *out = g_byte_array_new ();

  gzochid_storage_transaction *tx = fixture->storage_interface
    ->transaction_begin (fixture->app_context->storage_context);
  
  gzochid_client_session_serialization.serializer 
    (fixture->app_context, session, out, NULL);
  
  fixture->storage_interface->transaction_put
    (tx, fixture->app_context->oids, "1", 2, out->data, out->len);
  fixture->storage_interface->transaction_put
    (tx, fixture->app_context->names, "s.session.1", 12, "1", 2);

  fixture->storage_interface->transaction_prepare (tx);
  fixture->storage_interface->transaction_commit (tx);

  g_byte_array_unref (out);
}

static void 
test_disconnected_worker_no_handler_inner (gpointer data)
{
  struct test_scheme_task_fixture *fixture = data;
  
  g_test_log_set_fatal_handler (ignore_warnings, NULL);
  
  gzochid_scheme_application_disconnected_worker
    (fixture->app_context, fixture->identity, "1");
}

static void 
test_disconnected_worker_no_handler (struct test_scheme_task_fixture *fixture,
				     gconstpointer user_data)
{
  guint64 one = gzochid_util_encode_oid (1);
  gzochid_storage_transaction *tx = NULL;
  gzochid_client_session *session =
    gzochid_client_session_new (fixture->identity);
  
  persist_client_session (fixture, session);

  gzochid_transaction_execute
    (test_disconnected_worker_no_handler_inner, fixture);

  tx = fixture->storage_interface->transaction_begin
    (fixture->app_context->storage_context);

  g_assert
    (fixture->storage_interface->transaction_get
     (tx, fixture->app_context->oids, (char *) &one, sizeof (guint64), NULL) ==
     NULL);

  fixture->storage_interface->transaction_rollback (tx);

  gzochid_client_session_free (session);
}

static SCM 
raise_condition (SCM session)
{
  scm_throw (scm_from_locale_symbol ("test-tag"), SCM_EOL);
  return SCM_BOOL_F;
}

static SCM 
return_unspecified (SCM session)
{
  return SCM_UNSPECIFIED;
}

static gzochid_application_callback *
make_callback (char *name, GList *module)
{
  return gzochid_application_callback_new (name, module, -1);
}

static void 
test_logged_in_worker_throws_exception_inner (gpointer data)
{
  struct test_scheme_task_fixture *fixture = data;
  
  SCM module = scm_c_resolve_module ("test");
  SCM logged_in = scm_c_make_gsubr 
    ("logged-in-exception", 1, 0, 0, raise_condition);
  GList *test_module = g_list_append (NULL, "test");

  scm_c_module_define (module, "logged-in-exception", logged_in);

  fixture->app_context->descriptor->logged_in = make_callback 
    ("logged-in-exception", test_module);
  
  gzochid_scheme_application_logged_in_worker
    (fixture->app_context, fixture->identity, "1");

  g_list_free (test_module);
}

static void 
test_logged_in_worker_throws_exception
(struct test_scheme_task_fixture *fixture, gconstpointer user_data)
{
  gzochid_client_session *session =
    gzochid_client_session_new (fixture->identity);

  persist_client_session (fixture, session);

  gzochid_transaction_execute 
    (test_logged_in_worker_throws_exception_inner, fixture);

  gzochid_client_session_free (session);
}

static void 
test_logged_in_worker_returns_unspecified_inner (gpointer data)
{
  struct test_scheme_task_fixture *fixture = data;
    
  SCM module = scm_c_resolve_module ("test");
  SCM logged_in = scm_c_make_gsubr 
    ("logged-in-unspecified", 1, 0, 0, return_unspecified);
  GList *test_module = g_list_append (NULL, "test");

  scm_c_module_define (module, "logged-in-unspecified", logged_in);

  fixture->app_context->descriptor->logged_in = make_callback 
    ("logged-in-unspecified", test_module);

  gzochid_scheme_application_logged_in_worker
    (fixture->app_context, fixture->identity, "1");

  g_list_free (test_module);
}

static void 
test_logged_in_worker_returns_unspecified
(struct test_scheme_task_fixture *fixture, gconstpointer data)
{
  gzochid_client_session *session =
    gzochid_client_session_new (fixture->identity);
  persist_client_session (fixture, session);

  gzochid_transaction_execute 
    (test_logged_in_worker_returns_unspecified_inner, fixture);

  gzochid_client_session_free (session);
}

static void 
test_ready_throws_exception ()
{
  gzochid_application_context *context = gzochid_application_context_new ();
  GzochidApplicationDescriptor *descriptor = g_object_new
    (GZOCHID_TYPE_APPLICATION_DESCRIPTOR, NULL);
  gzochid_auth_identity *identity = gzochid_auth_identity_new ("[TEST]");
  
  SCM module = scm_c_resolve_module ("test");
  SCM ready = scm_c_make_gsubr ("ready", 1, 0, 0, raise_condition);
  GList *test_module = g_list_append (NULL, "test");

  GError *tmp_err = NULL;

  scm_c_module_define (module, "ready", ready);

  descriptor->ready = make_callback ("ready", test_module);

  context->deployment_root = "/";
  context->descriptor = descriptor;

  gzochid_scheme_application_ready (context, identity, &tmp_err);

  g_assert_error (tmp_err, GZOCHID_SCHEME_ERROR, GZOCHID_SCHEME_ERROR_FAILED);

  g_error_free (tmp_err);
  g_list_free (test_module);
  gzochid_auth_identity_unref (identity);
  g_object_unref (descriptor);
  gzochid_application_context_free (context);
}

void gzochid_api_channel_init () { }
void gzochid_api_data_init () { }
void gzochid_api_log_init () { }
void gzochid_api_session_init () { }
void gzochid_api_task_init () { }
void gzochid_api_tx_init () { }
void gzochid_api_util_init () { }

static void
inner_main (void *data, int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add
    ("/scheme/worker/disconnected/no-handler", struct test_scheme_task_fixture,
     NULL, test_scheme_task_fixture_setup, test_disconnected_worker_no_handler,
     test_scheme_task_fixture_teardown);
  g_test_add
    ("/scheme/worker/logged-in/exception", struct test_scheme_task_fixture,
     NULL, test_scheme_task_fixture_setup,
     test_logged_in_worker_throws_exception, test_scheme_task_fixture_teardown);
  g_test_add
    ("/scheme/worker/logged-in/unspecified", struct test_scheme_task_fixture,
     NULL, test_scheme_task_fixture_setup,
     test_logged_in_worker_returns_unspecified,
     test_scheme_task_fixture_teardown);

  g_test_add_func ("/scheme/ready/exception", test_ready_throws_exception);

  gzochid_guile_init ();
  gzochid_scheme_initialize_bindings ();
  gzochid_scheme_task_initialize_bindings ();
  exit (g_test_run ());
}

int
main (int argc, char *argv[])
{
  scm_boot_guile (argc, argv, inner_main, NULL);

  return 0;
}
