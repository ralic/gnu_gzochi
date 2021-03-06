/* metaclient.h: Prototypes and declarations for metaclient.c
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

#ifndef GZOCHID_METACLIENT_H
#define GZOCHID_METACLIENT_H

#include <glib.h>
#include <glib-object.h>

/* The core meta client type definitions. */

#define GZOCHID_TYPE_META_CLIENT gzochid_meta_client_get_type ()

/* The following boilerplate can be consolidated once GLib 2.44 makes it into
   Debian stable and `G_DECLARE_FINAL_TYPE' can be used. */

GType gzochid_meta_client_get_type (void);

/*
  The meta client is the root of the hierarchy of components that allow the
  server to communicate with other servers when running in distributed mode. It
  has the following readable properties:
  
  - "channel-client" returns a reference to a `GzochidChannelClient'
  - "connection-description" returns a string giving a description of the 
    current meta server connection, o `NULL' if the client is currently 
    disconnected. 
  - "data-client" returns a reference to a `GzochidDataClient'
  - "session-client" returns a reference to a `GzochidSessionClient'
  
  The meta client is not eligible for injection as a dependency by 
  `gzochid_resolver_require'.
*/

typedef struct _GzochidMetaClient GzochidMetaClient;

struct _GzochidMetaClientClass
{
  GObjectClass parent_class;
};

typedef struct _GzochidMetaClientClass GzochidMetaClientClass;

static inline GzochidMetaClient *
GZOCHID_META_CLIENT (gconstpointer ptr)
{
  return G_TYPE_CHECK_INSTANCE_CAST
    (ptr, gzochid_meta_client_get_type (), GzochidMetaClient);
}

/* The core meta client container type definitions. */

#define GZOCHID_TYPE_META_CLIENT_CONTAINER \
  gzochid_meta_client_container_get_type ()

/* The following boilerplate can be consolidated once GLib 2.44 makes it into
   Debian stable and `G_DECLARE_FINAL_TYPE' can be used. */

GType gzochid_meta_client_container_get_type (void);

/* The meta client container is a "holder" type for the gzochi meta client that
   is eligible for injection as a dependency by `gzochid_resolver_require' and
   which has a single readable property: "metaclient" which returns a reference
   to the meta client if the server is running in distributed mode, and `NULL'
   otherwise. */

typedef struct _GzochidMetaClientContainer GzochidMetaClientContainer;

struct _GzochidMetaClientContainerClass
{
  GObjectClass parent_class;
};

typedef struct _GzochidMetaClientContainerClass GzochidMetaClientContainerClass;

static inline GzochidMetaClientContainer *
GZOCHID_META_CLIENT_CONTAINER (gconstpointer ptr)
{
  return G_TYPE_CHECK_INSTANCE_CAST
    (ptr, gzochid_meta_client_container_get_type (),
     GzochidMetaClientContainer);
}

/* End boilerplate. */

enum
  {
    /* Indicates a malformed metaserver address. */
    
    GZOCHID_META_CLIENT_ERROR_ADDRESS,

    /* Indicates a failure to create a new socket via `socket'. */

    GZOCHID_META_CLIENT_ERROR_SOCKET,

    /* Indicates a failure to resolve the host info (i.e., IP address) of the
       metaserver via `gethostbyname'. */
    
    GZOCHID_META_CLIENT_ERROR_NETWORK, 
    GZOCHID_META_CLIENT_ERROR_FAILED /* Generic meta client failure. */
  };

#define GZOCHID_META_CLIENT_ERROR gzochid_meta_client_error_quark ()

GQuark gzochid_meta_client_error_quark ();

/* Starts the specified meta client's connection maintenance thread and 
   prepares it to begin servicing requests from the configured meta server. */

void gzochid_metaclient_start (GzochidMetaClient *, GError **);

/* Stops the specified client's connection maintenance thread, breaking the 
   client's connection to the meta server and halting request processing. */

void gzochid_metaclient_stop (GzochidMetaClient *);

/* Notifies the specified meta client that its connection to the meta server is
   no longer valid (because it has encountered an error, e.g.) and that it needs
   to be re-established. Intended for use as a callback from the `error' handler
   of the client protocol. */

void gzochid_metaclient_nullify_connection (GzochidMetaClient *);

#endif /* GZOCHID_METACLIENT_H */
