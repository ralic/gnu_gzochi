/* dataserver-protocol.c: Implementation of dataserver protocol.
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
#include <gzochi-common.h>
#include <stdlib.h>
#include <string.h>

#include "data-protocol.h"
#include "dataserver.h"
#include "dataserver-protocol.h"
#include "protocol.h"
#include "socket.h"

#define DATASERVER_PROTOCOL_VERSION 0x01

/* Local counter for assigning node ids. In the future, when there is more than
   one metaserver, this will not be a reliable way to assign ids. */

static volatile guint next_node_id = 0;

/* Dataserver client struct. */

struct _gzochi_metad_dataserver_client
{
  guint node_id; /* The client ephemeral node id. */

  /* The client's admin server base URL, if specifeid. */

  char *admin_server_base_url;
  
  GzochiMetadDataServer *dataserver; /* Reference to the data server. */
  gzochid_client_socket *sock; /* The client socket. */
};

const char *
gzochi_metad_dataserver_client_get_admin_server_base_url
(gzochi_metad_dataserver_client *client)
{
  return client->admin_server_base_url;
}

static gzochid_client_socket *
server_accept (GIOChannel *channel, const char *desc, gpointer data)
{
  gzochi_metad_dataserver_client *client = calloc
    (1, sizeof (gzochi_metad_dataserver_client));
  gzochid_client_socket *sock = gzochid_client_socket_new
    (channel, desc, gzochi_metad_dataserver_client_protocol, client);

  /* Assign the node id. */
  
  client->node_id = (guint) g_atomic_int_add (&next_node_id, 1);

  client->sock = sock;
  client->dataserver = data;
  
  return sock;
}

gzochid_server_protocol gzochi_metad_dataserver_server_protocol =
  { server_accept };

/*
  Returns `TRUE' if the specified buffer has a complete message that it is 
  ready (with respect to its length) to be dispatched to a handler. The length
  encoding is similar to the one used for the game protocol: A two-byte, 
  big-endian prefix giving the length of the message payload minus the opcode,
  which is the byte directly following the length prefix. 
  
  (So the smallest possible message would be three bytes, in which the first 
  two bytes were `NULL'.) 
*/

static gboolean
can_dispatch (const GByteArray *buffer, gpointer user_data)
{
  return buffer->len >= 3
    && buffer->len >= gzochi_common_io_read_short (buffer->data, 0) + 3;
}

/*
  Finds the bounds of the `NULL'-terminated string that begins at `bytes', 
  returning a pointer to that string and setting `str_len' appropriately.
  Returns `NULL' if the string is not `NULL'-terminated. 
  
  TODO: This function duplicates a function in `data-protocol.c'. Consider 
  making them available via a shared utility.
*/

static char *
read_str (const unsigned char *bytes, const size_t bytes_len, size_t *str_len)
{
  unsigned char *end = memchr (bytes, 0, bytes_len);

  if (end == NULL)
    return NULL;
  else
    {
      if (str_len != NULL)
        *str_len = end - bytes + 1;
      return (char *) bytes;
    }
}

/* Processes the message payload following the `GZOZCHID_DATA_PROTOCOL_LOGIN'
   opcode. Returns `TRUE' if the message was successfully decoded, `FALSE'
   otherwise. */

static gboolean
dispatch_login (gzochi_metad_dataserver_client *client, unsigned char *data,
		unsigned short len)
{
  size_t str_len = 0;
  unsigned char version = data[0];
  char *admin_server_base_url = NULL;

  if (version != DATASERVER_PROTOCOL_VERSION)
    return FALSE;

  admin_server_base_url = read_str (data + 1, len - 1, &str_len);

  /* The admin server base URL can be empty, but not absent. */
  
  if (admin_server_base_url == NULL)
    return FALSE;

  if (str_len > 1)
    client->admin_server_base_url = strdup (admin_server_base_url);

  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_REQUEST_OIDS' opcode. Returns `TRUE' if the message 
   was successfully decoded, `FALSE' otherwise. 

   If the message was successfully decoded, the bytes encoding a 
   `gzochid_data_reserve_oids_response' structure will be written to the client
   socket's send buffer. 
*/

static gboolean 
dispatch_request_oids (gzochi_metad_dataserver_client *client,
		       unsigned char *data, unsigned short len)
{
  size_t str_len = 0;
  char *app = read_str (data, len, &str_len);
  gzochid_data_reserve_oids_response *response = NULL;
  GByteArray *bytes = NULL;

  if (app == NULL || str_len <= 1)
    return FALSE;
  
  bytes = g_byte_array_new ();
  response = gzochi_metad_dataserver_reserve_oids
    (client->dataserver, client->node_id, app);

  gzochid_data_protocol_reserve_oids_response_write (response, bytes);

  /* Pad with two `NULL' bytes to leave space for the actual length to be 
     encoded. */

  g_byte_array_prepend
    (bytes, (unsigned char *) &(unsigned char[]) { 0, 0, 0x50 }, 3);
  gzochi_common_io_write_short (bytes->len - 3, bytes->data, 0);
  gzochid_client_socket_write (client->sock, bytes->data, bytes->len);
  
  gzochid_data_reserve_oids_response_free (response);
  g_byte_array_unref (bytes);

  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_REQUEST_OBJECT' opcode. Returns `TRUE' if the 
   message was successfully decoded, `FALSE' otherwise. 

   If the message was successfully decoded, the bytes encoding a 
   `gzochid_data_object_response' structure will be written to the client
   socket's send buffer. 
*/

static gboolean
dispatch_request_object (gzochi_metad_dataserver_client *client,
			 unsigned char *data, unsigned short len)
{
  mpz_t oid;
  size_t str_len = 0, offset = 0;
  char *app = read_str (data, len, &str_len);
  gboolean for_write = FALSE;
  GByteArray *bytes = NULL;
  char *oid_str = NULL;

  gzochid_data_object_response *response = NULL;
  
  if (app == NULL || str_len <= 1)
    return FALSE;

  len -= str_len;
  offset += str_len;

  if (len <= 0)
    return FALSE;

  for_write = data[offset] == 1;

  len--;
  offset++;
  
  oid_str = read_str (data + offset, len, &str_len);

  if (oid_str == NULL || str_len <= 1)
    return FALSE;

  if (mpz_init_set_str (oid, oid_str, 16) < 0)
    return FALSE;

  bytes = g_byte_array_new ();
  
  response = gzochi_metad_dataserver_request_object
    (client->dataserver, client->node_id, app, oid, for_write);
  gzochid_data_protocol_object_response_write (response, bytes);

  /* Pad with two `NULL' bytes to leave space for the actual length to be 
     encoded. */

  g_byte_array_prepend
    (bytes, (unsigned char *) &(unsigned char[]) { 0, 0, 0x51 }, 3);
  gzochi_common_io_write_short (bytes->len - 3, bytes->data, 0);
  gzochid_client_socket_write (client->sock, bytes->data, bytes->len);

  gzochid_data_object_response_free (response);
  g_byte_array_unref (bytes);
  
  mpz_clear (oid);
  return TRUE;
}

/*
  Processes the message payload following the 
  `GZOZCHID_DATA_PROTOCOL_REQUEST_BINDING' opcode. Returns `TRUE' if the 
  message was successfully decoded, `FALSE' otherwise. 

  If the message was successfully decoded, the bytes encoding a 
  `gzochid_data_binding_response' structure will be written to the client
  socket's send buffer. 
*/

static gboolean
dispatch_request_binding (gzochi_metad_dataserver_client *client,
			  unsigned char *data, unsigned short len)
{
  size_t str_len = 0, offset = 0;
  char *app = read_str (data, len, &str_len);
  gboolean for_write = FALSE;
  GByteArray *bytes = NULL;
  char *name = NULL;

  gzochid_data_binding_response *response = NULL;

  if (app == NULL || str_len <= 1)
    return FALSE;

  len -= str_len;
  offset += str_len;

  if (len <= 0)
    return FALSE;

  for_write = data[offset] == 1;

  len--;
  offset++;
  
  name = read_str (data + offset, len, &str_len);

  if (name == NULL || str_len <= 1)
    return FALSE;

  bytes = g_byte_array_new ();
  response = gzochi_metad_dataserver_request_binding
    (client->dataserver, client->node_id, app, name, for_write);
  gzochid_data_protocol_binding_response_write (response, bytes);

  /* Pad with two `NULL' bytes to leave space for the actual length to be 
     encoded. */

  g_byte_array_prepend
    (bytes, (unsigned char *) &(unsigned char[]) { 0, 0, 0x52 }, 3);
  gzochi_common_io_write_short (bytes->len - 3, bytes->data, 0);
  gzochid_client_socket_write (client->sock, bytes->data, bytes->len);

  gzochid_data_binding_response_free (response);
  g_byte_array_unref (bytes);

  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_REQUEST_NEXT_BINDING' opcode. Returns `TRUE' if the 
   message was successfully decoded, `FALSE' otherwise. */

static gboolean
dispatch_request_next_binding (gzochi_metad_dataserver_client *client,
			       unsigned char *data, unsigned short len)
{
  size_t str_len = 0;
  char *app = read_str (data, len, &str_len);
  char *name = NULL;
  GByteArray *bytes = NULL;

  gzochid_data_binding_key_response *response = NULL;

  if (app == NULL || str_len <= 1)
    return FALSE;

  name = read_str (data + str_len, len - str_len, &str_len);

  if (name == NULL || str_len == 0)
    return FALSE;
  else if (str_len == 1)
    name = NULL;

  bytes = g_byte_array_new ();
  response = gzochi_metad_dataserver_request_next_binding
    (client->dataserver, client->node_id, app, name);
  gzochid_data_protocol_binding_key_response_write (response, bytes);

  /* Pad with two `NULL' bytes to leave space for the actual length to be 
     encoded. */

  g_byte_array_prepend
    (bytes, (unsigned char *) &(unsigned char[]) { 0, 0, 0x53 }, 3);
  gzochi_common_io_write_short (bytes->len - 3, bytes->data, 0);
  gzochid_client_socket_write (client->sock, bytes->data, bytes->len);

  gzochid_data_binding_key_response_free (response);
  g_byte_array_unref (bytes);

  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_PROCESS_CHANGESET' opcode. Returns `TRUE' if the 
   message was successfully decoded and the changeset ingested, `FALSE' 
   otherwise. */

static gboolean
dispatch_submit_changeset (gzochi_metad_dataserver_client *client,
			   unsigned char *data, unsigned short len)
{
  GError *local_err = NULL;
  GBytes *bytes = g_bytes_new (data, len);
  gzochid_data_changeset *changeset =
    gzochid_data_protocol_changeset_read (bytes);

  if (changeset == NULL)
    {
      g_bytes_unref (bytes);
      return FALSE;
    }
  
  gzochi_metad_dataserver_process_changeset
    (client->dataserver, client->node_id, changeset, &local_err);

  if (local_err != NULL)
    {
      g_bytes_unref (bytes);
      return FALSE;
    }
  
  gzochid_data_changeset_free (changeset);
  g_bytes_unref (bytes);
  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_RELEASE_OBJECT' opcode. Returns `TRUE' if the 
   message was successfully decoded, `FALSE' otherwise. */

static gboolean
dispatch_release_object (gzochi_metad_dataserver_client *client,
			 unsigned char *data, unsigned short len)
{
  size_t str_len = 0;
  char *app = read_str (data, len, &str_len);
  char *oid_str = NULL;
  mpz_t oid;

  if (app == NULL || str_len <= 1)
    return FALSE;

  oid_str = read_str (data + str_len, len - str_len, &str_len);

  if (oid_str == NULL || str_len <= 1)
    return FALSE;

  if (mpz_init_set_str (oid, oid_str, 16) < 0)
    return FALSE;
  
  gzochi_metad_dataserver_release_object
    (client->dataserver, client->node_id, app, oid);
  mpz_clear (oid);

  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_RELEASE_BINDING' opcode. Returns `TRUE' if the 
   message was successfully decoded, `FALSE' otherwise. */

static gboolean
dispatch_release_binding (gzochi_metad_dataserver_client *client,
			  unsigned char *data, unsigned short len)
{
  size_t str_len = 0;
  char *app = read_str (data, len, &str_len);
  char *name = NULL;

  if (app == NULL || str_len <= 1)
    return FALSE;

  name = read_str (data + str_len, len - str_len, &str_len);

  if (name == NULL || str_len <= 1)
    return FALSE;

  gzochi_metad_dataserver_release_binding
    (client->dataserver, client->node_id, app, name);

  return TRUE;
}

/* Processes the message payload following the 
   `GZOZCHID_DATA_PROTOCOL_RELEASE_BINDING_RANGE' opcode. Returns `TRUE' if the 
   message was successfully decoded, `FALSE' otherwise. */

static gboolean
dispatch_release_binding_range (gzochi_metad_dataserver_client *client,
				unsigned char *data, unsigned short len)
{
  size_t str_len = 0, offset = 0;
  char *app = read_str (data, len, &str_len);
  char *from = NULL, *to = NULL;

  if (app == NULL || str_len <= 1)
    return FALSE;

  len -= str_len;
  offset += str_len;
  
  from = read_str (data + offset, len, &str_len);
  
  if (from == NULL || str_len == 0)
    return FALSE;
  else if (str_len == 1)
    from = NULL;

  len -= str_len;
  offset += str_len;

  to = read_str (data + offset, len, &str_len);

  if (to == NULL || str_len == 0)
    return FALSE;
  else if (str_len == 1)
    to = NULL;
  
  gzochi_metad_dataserver_release_binding_range
    (client->dataserver, client->node_id, app, from, to);

  return TRUE;
}

/* Attempt to dispatch a fully-buffered message from the specified client based
   on its opcode. */

static void 
dispatch_message (gzochi_metad_dataserver_client *client,
		  unsigned char *message, unsigned short len)
{
  int opcode = message[0];
  unsigned char *payload = message + 1;
  
  len--;
  
  switch (opcode)
    {
    case GZOCHID_DATA_PROTOCOL_LOGIN:
      dispatch_login (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_REQUEST_OIDS:
      dispatch_request_oids (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_REQUEST_OBJECT:
      dispatch_request_object (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_REQUEST_BINDING:
      dispatch_request_binding (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_REQUEST_NEXT_BINDING:
      dispatch_request_next_binding (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_SUBMIT_CHANGESET:
      dispatch_submit_changeset (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_RELEASE_OBJECT:
      dispatch_release_object (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_RELEASE_BINDING:
      dispatch_release_binding (client, payload, len); break;
    case GZOCHID_DATA_PROTOCOL_RELEASE_BINDING_RANGE:
      dispatch_release_binding_range (client, payload, len); break;
      
    default:
      g_warning ("Unexpected opcode %d received from client", opcode);
    }
  
  return;
}

/* Attempts to dispatch all messages in the specified buffer. Returns the 
   number of successfully dispatched messages. */

static unsigned int
client_dispatch (const GByteArray *buffer, gpointer user_data)
{
  gzochi_metad_dataserver_client *client = user_data;

  int offset = 0, total = 0;
  int remaining = buffer->len;

  while (remaining >= 3)
    {
      short len = gzochi_common_io_read_short
        ((unsigned char *) buffer->data, offset);
      
      if (++len > remaining - 2)
        break;
      
      offset += 2;

      dispatch_message (client, (unsigned char *) buffer->data + offset, len);
      
      offset += len;
      remaining -= len + 2;
      total += len + 2;
    }

  return total;
}

/* The client error handler. Releases all locks held on behalf of the client. */

static void
client_error (gpointer user_data)
{
  gzochi_metad_dataserver_client *client = user_data;
  
  gzochi_metad_dataserver_release_all (client->dataserver, client->node_id);
}

/* Client finalization callback. */

static void
client_free (gpointer user_data)
{
  gzochi_metad_dataserver_client *client = user_data;

  free (client);
}

gzochid_client_protocol gzochi_metad_dataserver_client_protocol =
  { can_dispatch, client_dispatch, client_error, client_free };