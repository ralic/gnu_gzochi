/* test-sessionserver.c: Tests for sessionserver.c in gzochi-metad.
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
#include <stddef.h>
#include <stdlib.h>

#include "event.h"
#include "event-meta.h"
#include "resolver.h"
#include "sessionserver.h"
#include "socket.h"

struct _gzochid_client_socket
{
  GByteArray *bytes_written;
};

void
gzochid_client_socket_write (gzochid_client_socket *sock,
			     const unsigned char *data, size_t len)
{
  g_byte_array_append (sock->bytes_written, data, len);
}

static gzochid_client_socket *
gzochid_test_client_socket_new (void)
{
  gzochid_client_socket *socket = malloc (sizeof (gzochid_client_socket));

  socket->bytes_written = g_byte_array_new ();
  
  return socket;
}

static void
gzochid_test_client_socket_free (gzochid_client_socket *socket)
{
  g_byte_array_unref (socket->bytes_written);
  free (socket);
}

struct _sessionserver_fixture
{
  GzochidResolutionContext *resolution_context;
  GzochidEventLoop *event_loop;
  GzochiMetadSessionServer *server;
  gzochid_client_socket *client_socket;
};

typedef struct _sessionserver_fixture sessionserver_fixture;

static void
setup_sessionserver (sessionserver_fixture *fixture, gconstpointer user_data)
{
  fixture->resolution_context =
    g_object_new (GZOCHID_TYPE_RESOLUTION_CONTEXT, NULL);
  fixture->event_loop = gzochid_resolver_require_full
    (fixture->resolution_context, GZOCHID_TYPE_EVENT_LOOP, NULL);
  fixture->server = gzochid_resolver_require_full
    (fixture->resolution_context, GZOCHI_METAD_TYPE_SESSION_SERVER, NULL);

  fixture->client_socket = gzochid_test_client_socket_new ();
}

static void
teardown_sessionserver (sessionserver_fixture *fixture, gconstpointer user_data)
{
  g_object_unref (fixture->server);
  g_object_unref (fixture->event_loop);
  g_object_unref (fixture->resolution_context);
  
  gzochid_test_client_socket_free (fixture->client_socket);
}

static void
test_server_connected (sessionserver_fixture *fixture, gconstpointer user_data)
{
  GError *err = NULL;
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, &err);
  g_assert_no_error (err);
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, &err);
  g_assert_error (err, GZOCHI_METAD_SESSIONSERVER_ERROR,
		  GZOCHI_METAD_SESSIONSERVER_ERROR_ALREADY_CONNECTED);

  g_error_free (err);
}

static void
test_server_disconnected (sessionserver_fixture *fixture,
			  gconstpointer user_data)
{
  GError *err = NULL;

  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);
  
  gzochi_metad_sessionserver_server_disconnected (fixture->server, 1, &err);
  g_assert_no_error (err);

  gzochi_metad_sessionserver_server_disconnected (fixture->server, 1, &err);
  g_assert_error (err, GZOCHI_METAD_SESSIONSERVER_ERROR,
		  GZOCHI_METAD_SESSIONSERVER_ERROR_NOT_CONNECTED);

  g_error_free (err);  
}

struct callback_data
{
  GMutex mutex;
  GCond cond;

  gboolean handled;  
};

static void
init_callback_data (struct callback_data *callback_data)
{
  g_mutex_init (&callback_data->mutex);
  g_cond_init (&callback_data->cond);
  callback_data->handled = FALSE;
}

static void
clear_callback_data (struct callback_data *callback_data)
{
  g_mutex_clear (&callback_data->mutex);
  g_cond_clear (&callback_data->cond);
}

static void
handle_session_connected (GzochidEvent *event, gpointer user_data)
{
  GzochiMetadSessionEvent *client_event = GZOCHI_METAD_SESSION_EVENT (event);
  struct callback_data *callback_data = user_data;
  gzochi_metad_session_event_type type;
  char *app = NULL;

  g_mutex_lock (&callback_data->mutex);
  
  g_object_get (client_event, "type", &type, "application", &app, NULL);
  g_assert_cmpint (type, ==, SESSION_CONNECTED);
  g_assert_cmpstr (app, ==, "test");
  callback_data->handled = TRUE;
  
  g_cond_signal (&callback_data->cond);
  g_mutex_unlock (&callback_data->mutex);

  free (app);
}

static void
test_session_connected (sessionserver_fixture *fixture, gconstpointer user_data)
{
  GError *err = NULL;
  struct callback_data callback_data;
  gzochid_event_source *event_source = NULL;

  init_callback_data (&callback_data);
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);

  g_object_get (fixture->server, "event-source", &event_source, NULL);
  gzochid_event_attach (event_source, handle_session_connected, &callback_data);
  g_source_unref ((GSource *) event_source);

  gzochid_event_loop_start (fixture->event_loop);

  g_mutex_lock (&callback_data.mutex);
  
  gzochi_metad_sessionserver_session_connected
    (fixture->server, 1, "test", 1, &err);
  g_assert_no_error (err);

  g_cond_wait_until
    (&callback_data.cond, &callback_data.mutex,
     g_get_monotonic_time () + 100000);
  g_mutex_unlock (&callback_data.mutex);

  gzochid_event_loop_stop (fixture->event_loop);
  
  gzochi_metad_sessionserver_session_connected
    (fixture->server, 1, "test", 1, &err);  
  g_assert_error (err, GZOCHI_METAD_SESSIONSERVER_ERROR,
		  GZOCHI_METAD_SESSIONSERVER_ERROR_ALREADY_CONNECTED);

  g_error_free (err);

  clear_callback_data (&callback_data);
}

static void
handle_session_disconnected (GzochidEvent *event, gpointer user_data)
{
  GzochiMetadSessionEvent *client_event = GZOCHI_METAD_SESSION_EVENT (event);
  struct callback_data *callback_data = user_data;
  gzochi_metad_session_event_type type;
  char *app = NULL;
  
  g_object_get (client_event, "type", &type, "application", &app, NULL);

  if (type == SESSION_CONNECTED)
    {
      free (app);
      return;
    }

  g_mutex_lock (&callback_data->mutex);
  
  g_assert_cmpint (type, ==, SESSION_DISCONNECTED);
  g_assert_cmpstr (app, ==, "test");
  callback_data->handled = TRUE;
  
  g_cond_signal (&callback_data->cond);
  g_mutex_unlock (&callback_data->mutex);

  free (app);
}

static void
test_session_disconnected (sessionserver_fixture *fixture,
			   gconstpointer user_data)
{
  GError *err = NULL;
  struct callback_data callback_data;
  gzochid_event_source *event_source = NULL;
  
  init_callback_data (&callback_data);

  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);

  gzochid_event_loop_start (fixture->event_loop);

  gzochi_metad_sessionserver_session_connected
    (fixture->server, 1, "test", 1, NULL);

  g_object_get (fixture->server, "event-source", &event_source, NULL);
  gzochid_event_attach
    (event_source, handle_session_disconnected, &callback_data);
  g_source_unref ((GSource *) event_source);

  g_mutex_lock (&callback_data.mutex);
  
  gzochi_metad_sessionserver_session_disconnected
    (fixture->server, "test", 1, &err);
  g_assert_no_error (err);

  g_cond_wait_until
    (&callback_data.cond, &callback_data.mutex,
     g_get_monotonic_time () + 100000);
  g_mutex_unlock (&callback_data.mutex);

  gzochid_event_loop_stop (fixture->event_loop);

  gzochi_metad_sessionserver_session_disconnected
    (fixture->server, "test", 1, &err);  
  g_assert_error (err, GZOCHI_METAD_SESSIONSERVER_ERROR,
		  GZOCHI_METAD_SESSIONSERVER_ERROR_NOT_CONNECTED);

  g_error_free (err);

  clear_callback_data (&callback_data);
}

static void
test_relay_disconnect (sessionserver_fixture *fixture, gconstpointer user_data)
{
  GError *err = NULL;
  GBytes *expected = NULL, *actual = NULL;
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);
  gzochi_metad_sessionserver_session_connected
    (fixture->server, 1, "test", 1, NULL);

  gzochi_metad_sessionserver_relay_disconnect
    (fixture->server, "test", 1, &err);

  g_assert_no_error (err);

  expected = g_bytes_new_static
    ("\x00\x0d\x63test\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16);
  actual = g_bytes_new_static (fixture->client_socket->bytes_written->data,
			       fixture->client_socket->bytes_written->len);
  
  g_assert (g_bytes_equal (expected, actual));

  g_bytes_unref (expected);
  g_bytes_unref (actual);
}

static void
test_relay_disconnect_error (sessionserver_fixture *fixture,
			     gconstpointer user_data)
{
  GError *err = NULL;
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);

  gzochi_metad_sessionserver_relay_disconnect
    (fixture->server, "test", 1, &err);
  
  g_assert_error (err, GZOCHI_METAD_SESSIONSERVER_ERROR,
		  GZOCHI_METAD_SESSIONSERVER_ERROR_NOT_CONNECTED);

  g_error_free (err);
}

static void
test_relay_message (sessionserver_fixture *fixture, gconstpointer user_data)
{
  GError *err = NULL;
  GBytes *msg = g_bytes_new_static ("foo", 4), *expected = NULL, *actual = NULL;
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);
  gzochi_metad_sessionserver_session_connected
    (fixture->server, 1, "test", 1, NULL);

  gzochi_metad_sessionserver_relay_message
    (fixture->server, "test", 1, msg, &err);

  g_assert_no_error (err);

  expected = g_bytes_new_static
    ("\x00\x13\x65test\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x04""foo", 22);
  actual = g_bytes_new_static (fixture->client_socket->bytes_written->data,
			       fixture->client_socket->bytes_written->len);
  
  g_assert (g_bytes_equal (expected, actual));

  g_bytes_unref (msg);
  g_bytes_unref (expected);
  g_bytes_unref (actual);
}

static void
test_relay_message_error (sessionserver_fixture *fixture,
			  gconstpointer user_data)
{
  GError *err = NULL;
  GBytes *msg = g_bytes_new_static ("foo", 4);
  
  gzochi_metad_sessionserver_server_connected
    (fixture->server, 1, fixture->client_socket, NULL);

  gzochi_metad_sessionserver_relay_message
    (fixture->server, "test", 1, msg, &err);
  
  g_assert_error (err, GZOCHI_METAD_SESSIONSERVER_ERROR,
		  GZOCHI_METAD_SESSIONSERVER_ERROR_NOT_CONNECTED);

  g_error_free (err);
  g_bytes_unref (msg);
}

int
main (int argc, char *argv[])
{
#if GLIB_CHECK_VERSION (2, 36, 0)
  /* No need for `g_type_init'. */
#else
  g_type_init ();
#endif /* GLIB_CHECK_VERSION */

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/sessionserver/server-connected", sessionserver_fixture, NULL,
	      setup_sessionserver, test_server_connected,
	      teardown_sessionserver);
  g_test_add ("/sessionserver/server-disconnected", sessionserver_fixture, NULL,
	      setup_sessionserver, test_server_disconnected,
	      teardown_sessionserver);
  g_test_add ("/sessionserver/session-connected", sessionserver_fixture, NULL,
	      setup_sessionserver, test_session_connected,
	      teardown_sessionserver);
  g_test_add ("/sessionserver/session-disconnected", sessionserver_fixture,
	      NULL, setup_sessionserver, test_session_disconnected,
	      teardown_sessionserver);
  g_test_add ("/sessionserver/relay-disconnect", sessionserver_fixture, NULL,
	      setup_sessionserver, test_relay_disconnect,
	      teardown_sessionserver);
  g_test_add ("/sessionserver/relay-disconnect/error", sessionserver_fixture,
	      NULL, setup_sessionserver, test_relay_disconnect_error,
	      teardown_sessionserver);
  g_test_add ("/sessionserver/relay-message", sessionserver_fixture, NULL,
	      setup_sessionserver, test_relay_message, teardown_sessionserver);
  g_test_add ("/sessionserver/relay-message/error", sessionserver_fixture,
	      NULL, setup_sessionserver, test_relay_message_error,
	      teardown_sessionserver);
  
  return g_test_run ();
}
