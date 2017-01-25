/* game.h: Prototypes and declarations for game.c
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

#ifndef GZOCHID_GAME_H
#define GZOCHID_GAME_H

#include <glib.h>
#include <glib-object.h>
#include <sys/time.h>

#include "app.h"
#include "event.h"
#include "gzochid-storage.h"
#include "schedule.h"
#include "socket.h"

struct _gzochid_game_context 
{
  GObject *root_context;
  GThreadPool *pool;
  gzochid_task_queue *task_queue;
  
  int port;
  char *apps_dir;
  char *work_dir;
  struct timeval tx_timeout;

  GHashTable *applications;
  GzochidAuthPluginRegistry *auth_plugin_registry;
  
  /* The storage engine loaded by the game manager. */

  gzochid_storage_engine *storage_engine; 

  gzochid_server_socket *server_socket; /* The game protocol server socket. */
  GzochidSocketServer *socket_server; /* The game server socket server. */
  GzochidEventLoop *event_loop;
};

typedef struct _gzochid_game_context gzochid_game_context;

/* Create a new game application server context that uses the specified socket
   context to listen for and dispatch messages from client connections. This
   may be `NULL' if no connnections are expected. */

gzochid_game_context *gzochid_game_context_new ();
void gzochid_game_context_free (gzochid_game_context *);
void gzochid_game_context_init (gzochid_game_context *, GObject *);

void gzochid_game_context_register_application (gzochid_game_context *, char *,
						gzochid_application_context *);
void gzochid_game_context_unregister_application (gzochid_game_context *, 
						  char *);
gzochid_application_context *gzochid_game_context_lookup_application
(gzochid_game_context *, char *);
GList *gzochid_game_context_get_applications (gzochid_game_context *);

#endif /* GZOCHID_GAME_H */
