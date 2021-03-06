/* channel.c: Channel management routines for gzochid
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

#include <assert.h>
#include <glib.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "app.h"
#include "app-task.h"
#include "auth_int.h"
#include "channel.h"
#include "channelclient.h"
#include "event.h"
#include "game.h"
#include "game-protocol.h"
#include "gzochid-auth.h"
#include "io.h"
#include "scheme.h"
#include "session.h"
#include "task.h"
#include "tx.h"
#include "util.h"

/*
  The following data structures and functions define the gzochid container's
  channel service. As described in the manual, channels are named groups of
  client sessions that can be addressed en masse for the purpose of broadcast.
  
  The channel service provides transactional semantics for message delivery and
  membership management. There are two different "stages" of transaction 
  executed by this service. The first stage is user-initiated and provides 
  "all-or-nothing" guarantees for the scheduling of channel operations; if this
  transaction commits, all requested operations will be durably scheduled for 
  execution in serial. The second tier of transactions includes transactions
  initiated to execute each of scheduled tasks; when a channel "side effect" 
  transaction commits, the operation is executed against the current state of 
  the channel - a message being sent to all member sessions, for example. The
  initial transaction operates on the channel in the abstract, whereas the side
  effect transaction operates on the concrete state of the channel.

  Despite the transactional nature of channel interactions, channels themselves
  are ephemeral with respect to a gzochid application server node. Channel
  membership, for example, does not survive a container restart. ...Which stands
  to reason, since all members would have been disconnected. 
*/

#define CHANNEL_PREFIX "s.channel."

/* An enumeration of operation types. */

enum gzochid_channel_operation
  {
    GZOCHID_CHANNEL_OP_JOIN, /* Add a session to the channel. */
    GZOCHID_CHANNEL_OP_LEAVE, /* Remove a session from the channel. */
    GZOCHID_CHANNEL_OP_SEND, /* Send a message to all member sessions. */
    GZOCHID_CHANNEL_OP_CLOSE /* Destroy the channel, removing all members. */
  };

/* The channel structure. */

struct _gzochid_channel
{
  char *name; /* The channel name. */
  guint64 oid; /* The object id of the channel in the object store. */
  guint64 scm_oid; /* The object id of the channel's Scheme representation. */
};

gzochid_channel *
gzochid_channel_new (char *name)
{
  gzochid_channel *channel = calloc (1, sizeof (gzochid_channel));

  channel->name = strdup (name);

  return channel;
}

void
gzochid_channel_free (gzochid_channel *channel)
{
  free (channel->name);
  free (channel);
}

guint64
gzochid_channel_scm_oid (gzochid_channel *channel)
{
  return channel->scm_oid;
}

const char *
gzochid_channel_name (gzochid_channel *channel)
{
  return channel->name;
}

/* Serialization routines for channel objects. */

static gpointer 
deserialize_channel (gzochid_application_context *context, GByteArray *in,
		     GError **err)
{
  gzochid_channel *channel = calloc (1, sizeof (gzochid_channel));

  channel->name = gzochid_util_deserialize_string (in);

  channel->oid = gzochid_util_deserialize_oid (in);
  channel->scm_oid = gzochid_util_deserialize_oid (in);

  return channel;
}

static void 
serialize_channel (gzochid_application_context *context, gpointer obj,
		   GByteArray *out, GError **err)
{
  gzochid_channel *channel = obj;
  gzochid_util_serialize_string (channel->name, out);

  gzochid_util_serialize_oid (channel->oid, out);
  gzochid_util_serialize_oid (channel->scm_oid, out);
}

static void finalize_channel
(gzochid_application_context *context, gpointer obj)
{
  gzochid_channel_free (obj);
}

gzochid_io_serialization gzochid_channel_serialization =
  { serialize_channel, deserialize_channel, finalize_channel };

/* The following data structures represent the logical set of channel operations
   via a kind of "lite" polymorphism. */

/* The base operation type. Also represents a channel `close' operation. */

struct _gzochid_channel_pending_operation
{
  enum gzochid_channel_operation type; /* The operation type. */
  guint64 target_channel; /* The target channel oid. */

  /* The operation timestamp, for schedule ordering. */
  
  struct timeval timestamp; 
};

typedef struct _gzochid_channel_pending_operation
gzochid_channel_pending_operation;

/* Represents a channel `send' operation. */

struct _gzochid_channel_pending_send_operation
{
  gzochid_channel_pending_operation base; /* Base channel operation. */
  unsigned char *message; /* The message payload. */
  short len; /* The message payload size. */
};

typedef struct _gzochid_channel_pending_send_operation
gzochid_channel_pending_send_operation;

/* Represents an operation that changes channel membership, i.e., a `join' or
   `leave' operation. */

struct _gzochid_channel_pending_membership_operation
{
  gzochid_channel_pending_operation base; /* Base channel operation. */
  guint64 target_session; /* The target session oid. */
};

typedef struct _gzochid_channel_pending_membership_operation
gzochid_channel_pending_membership_operation;

/* Serialization routines for logical channel operations. */

static gpointer 
deserialize_channel_operation (gzochid_application_context *context,
			       GByteArray *in, GError **err)
{
  enum gzochid_channel_operation type = gzochid_util_deserialize_int (in);
  gzochid_channel_pending_operation *op = NULL;

  /* 
     Figure out what kind of operation this is so we know how to deserialize 
     the rest of the object. 
  */
  
  if (type == GZOCHID_CHANNEL_OP_JOIN || type == GZOCHID_CHANNEL_OP_LEAVE)
    op = malloc (sizeof (gzochid_channel_pending_membership_operation));
  else if (type == GZOCHID_CHANNEL_OP_SEND)
    op = malloc (sizeof (gzochid_channel_pending_send_operation));
  else op = malloc (sizeof (gzochid_channel_pending_operation));

  op->type = type;
  op->target_channel = gzochid_util_deserialize_oid (in);

  if (type == GZOCHID_CHANNEL_OP_JOIN || type == GZOCHID_CHANNEL_OP_LEAVE)
    {
      gzochid_channel_pending_membership_operation *member_op =
	(gzochid_channel_pending_membership_operation *) op;
      
      member_op->target_session = gzochid_util_deserialize_oid (in);
    }
  else if (type == GZOCHID_CHANNEL_OP_SEND)
    {
      gzochid_channel_pending_send_operation *send_op =
	(gzochid_channel_pending_send_operation *) op;
      
      send_op->message = gzochid_util_deserialize_bytes
	(in, (int *) &send_op->len);
    }

  return op;
}

static void 
serialize_channel_operation (gzochid_application_context *context,
			     gpointer data, GByteArray *out, GError **err)
{
  gzochid_channel_pending_operation *op = data;

  /* Important to serialize the operation type first, so that the deserializer
     can read it before reading an operation type-specific fields. */
  
  gzochid_util_serialize_int (op->type, out);
  gzochid_util_serialize_oid (op->target_channel, out);
  
  if (op->type == GZOCHID_CHANNEL_OP_JOIN
      || op->type == GZOCHID_CHANNEL_OP_LEAVE) {

    gzochid_channel_pending_membership_operation *member_op =
      (gzochid_channel_pending_membership_operation *) op;

    gzochid_util_serialize_oid (member_op->target_session, out);
    
  } else if (op->type == GZOCHID_CHANNEL_OP_SEND) {

    gzochid_channel_pending_send_operation *send_op =
      (gzochid_channel_pending_send_operation *) op;

    gzochid_util_serialize_bytes (send_op->message, send_op->len, out);
  } 
}

static void
finalize_channel_operation (gzochid_application_context *context, gpointer data)
{
  gzochid_channel_pending_operation *op = data;

  if (op->type == GZOCHID_CHANNEL_OP_SEND)
    {
      gzochid_channel_pending_send_operation *send_op =
	(gzochid_channel_pending_send_operation *) op;

      free (send_op->message);
    }
  
  free (op);
}

static gzochid_io_serialization gzochid_channel_operation_serialization =
  {
    serialize_channel_operation,
    deserialize_channel_operation,
    finalize_channel_operation
  };

/* The following data structures and functions make up the channel side effects
   transaction system, using the same kind of polymorphism as logical channel
   operations. */

/* The base side effect type. Also represents a channel `close' side effect. */

struct _gzochid_channel_side_effect
{
  enum gzochid_channel_operation op; /* The side effect type. */

  /* The hexadecimal string form of the channel oid; used as a key into the 
     application context's active channel map. */

  guint64 channel_oid;
};

typedef struct _gzochid_channel_side_effect gzochid_channel_side_effect;

/* Represents a side effect that changes channel membership, i.e., a `join' or
   `leave' side effect. */

struct _gzochid_channel_membership_side_effect
{
  gzochid_channel_side_effect base; /* Base channel side effect. */
  
  /* The hexadecimal string form of the session oid; used as a key into the 
     application context's active session map. */

  guint64 session_oid;
};

typedef struct _gzochid_channel_membership_side_effect
gzochid_channel_membership_side_effect;

/* Represents a channel `send' side effect. */

struct _gzochid_channel_message_side_effect
{
  gzochid_channel_side_effect base; /* Base channel side effect. */
  
  /* A snapshot of the members of the channel, in the form of an array of 
     session oids, at the time the side effect transaction was 
     executed. */

  GArray *session_oids;
  
  unsigned char *msg; /* The message to be sent. */
  short len; /* The message payload length. */
};

typedef struct _gzochid_channel_message_side_effect
gzochid_channel_message_side_effect;

/* Frees the memory used by the specified side effect. */

static void
free_side_effect (gzochid_channel_side_effect *side_effect)
{
  if (side_effect->op == GZOCHID_CHANNEL_OP_SEND)
    {
      gzochid_channel_message_side_effect *message_side_effect =
	(gzochid_channel_message_side_effect *) side_effect;

      g_array_unref (message_side_effect->session_oids);
      free (message_side_effect->msg);
    }

  free (side_effect);
}

/*
  Create and return a new channel `close' side effect. Memory should be freed
  via `free_side_effect' when no longer in use.
*/

static gzochid_channel_side_effect *
gzochid_channel_side_effect_new (enum gzochid_channel_operation op,
				 guint64 channel_oid)
{
  gzochid_channel_side_effect *side_effect =
    malloc (sizeof (gzochid_channel_side_effect));

  assert (op == GZOCHID_CHANNEL_OP_CLOSE);
  
  side_effect->op = op;
  side_effect->channel_oid = channel_oid;

  return side_effect;
}

/*
  Create and return a new channel `leave' or `join' side effect with the 
  specified target session oid. Memory should be freed via `free_side_effect' 
  when no longer in use.
*/

static gzochid_channel_side_effect *
gzochid_channel_membership_side_effect_new (enum gzochid_channel_operation op,
					    guint64 channel_oid,
					    guint64 session_oid)
{
  gzochid_channel_membership_side_effect *membership_side_effect =
    malloc (sizeof (gzochid_channel_membership_side_effect));
  gzochid_channel_side_effect *side_effect = (gzochid_channel_side_effect *)
    membership_side_effect;

  assert (op == GZOCHID_CHANNEL_OP_JOIN || op == GZOCHID_CHANNEL_OP_LEAVE);
  
  side_effect->op = op;
  side_effect->channel_oid = channel_oid;

  membership_side_effect->session_oid = session_oid;
  
  return side_effect;
}

/*
  Create and return a new channel `send' side effect with the 
  specified session oid array and message. Memory should be freed via 
  `free_side_effect' when no longer in use.
*/

static gzochid_channel_side_effect *
gzochid_channel_message_side_effect_new (enum gzochid_channel_operation op,
					 guint64 channel_oid,
					 GArray *session_oids,
					 unsigned char *msg, short len)
{
  gzochid_channel_message_side_effect *message_side_effect =
    malloc (sizeof (gzochid_channel_message_side_effect));
  gzochid_channel_side_effect *side_effect = (gzochid_channel_side_effect *)
    message_side_effect;

  assert (op == GZOCHID_CHANNEL_OP_SEND);
  
  side_effect->op = op;
  side_effect->channel_oid = channel_oid;

  message_side_effect->session_oids = g_array_ref (session_oids);
  message_side_effect->msg = malloc (sizeof (char) * len);
  message_side_effect->len = len;
  
  memcpy (message_side_effect->msg, msg, len);
  
  return side_effect;
}

/* The transaction participant context for channel side effect transactions. */

struct _gzochid_channel_side_effect_transaction_context 
{
  /* The gzochi game application context to which the channel belongs. */
  
  gzochid_application_context *app_context;

  /* The channel side effect to be executed on commit. There can only be one
     side effect per side effect transaction. */
  
  gzochid_channel_side_effect *side_effect;
};

typedef struct _gzochid_channel_side_effect_transaction_context 
gzochid_channel_side_effect_transaction_context;

static void
cleanup_side_effect_transaction
(gzochid_channel_side_effect_transaction_context *tx_context)
{
  if (tx_context->side_effect != NULL)
    free_side_effect (tx_context->side_effect);
  free (tx_context);
}

/* The `prepare' function for a side effects transaction. Effectively a 
   no-op. */

static int
channel_side_effect_prepare (gpointer data)
{
  return TRUE;
}

void
gzochid_channel_join_direct (gzochid_application_context *app_context,
			     guint64 channel_oid, guint64 session_oid,
			     GError **err)
{
  GSequence *sessions = NULL;

  g_mutex_lock (&app_context->channel_mapping_lock);
  g_mutex_lock (&app_context->client_mapping_lock);
  
  sessions = g_hash_table_lookup
    (app_context->channel_oids_to_local_session_oids, &channel_oid);

  if (!g_hash_table_contains (app_context->oids_to_clients, &session_oid))
    g_set_error
      (err, GZOCHID_CHANNEL_ERROR, GZOCHID_CHANNEL_ERROR_NOT_MAPPED,
       "No local client bound to oid %" G_GUINT64_FORMAT, session_oid);
  
  else if (sessions == NULL)
    {      
      /* If not, that's okay; it may not have been used on this node. Create
	 an empty sequence to store new members. */
      
      sessions = g_sequence_new (free);
      g_sequence_append (sessions, g_memdup (&session_oid, sizeof (guint64)));
      
      g_hash_table_insert
	(app_context->channel_oids_to_local_session_oids,
	 g_memdup (&channel_oid, sizeof (guint64)), sessions);
    }
  else
    {
      GSequenceIter *iter = g_sequence_lookup
	(sessions, &session_oid, gzochid_util_guint64_data_compare, NULL);

      /* Add or remove the session to or from the channel's session list. */
	  
      if (iter == NULL)
	g_sequence_insert_sorted
	  (sessions, g_memdup (&session_oid, sizeof (guint64)),
	   gzochid_util_guint64_data_compare, NULL);
      else g_set_error
	     (err, GZOCHID_CHANNEL_ERROR, GZOCHID_CHANNEL_ERROR_ALREADY_MEMBER,
	      "Session %" G_GUINT64_FORMAT " already a member of channel %"
	      G_GUINT64_FORMAT, session_oid, channel_oid);
    }

  g_mutex_unlock (&app_context->client_mapping_lock);
  g_mutex_unlock (&app_context->channel_mapping_lock);
}

void
gzochid_channel_leave_direct (gzochid_application_context *app_context,
			      guint64 channel_oid, guint64 session_oid,
			      GError **err)
{  
  g_mutex_lock (&app_context->channel_mapping_lock);
  g_mutex_lock (&app_context->client_mapping_lock);
  
  if (!g_hash_table_contains (app_context->oids_to_clients, &session_oid))
    g_set_error
      (err, GZOCHID_CHANNEL_ERROR, GZOCHID_CHANNEL_ERROR_NOT_MAPPED,
       "No local client bound to oid %" G_GUINT64_FORMAT, session_oid);
  else
    {
      GSequence *sessions = g_hash_table_lookup
	(app_context->channel_oids_to_local_session_oids, &channel_oid);
      gboolean found_session = FALSE;
  
      if (sessions != NULL)
	{      
	  GSequenceIter *iter = g_sequence_lookup
	    (sessions, &session_oid, gzochid_util_guint64_data_compare, NULL);
	  
	  if (iter != NULL)
	    {
	      found_session = TRUE;
	      g_sequence_remove (iter);
	      
	      if (g_sequence_get_begin_iter (sessions) ==
		  g_sequence_get_end_iter (sessions))
		
		g_hash_table_remove
		  (app_context->channel_oids_to_local_session_oids,
		   &channel_oid);
	    }
	}

      if (!found_session)
	g_set_error
	  (err, GZOCHID_CHANNEL_ERROR, GZOCHID_CHANNEL_ERROR_NOT_MEMBER,
	   "Session %" G_GUINT64_FORMAT " is not a member of channel %"
	   G_GUINT64_FORMAT, session_oid, channel_oid);
    }

  g_mutex_unlock (&app_context->client_mapping_lock);
  g_mutex_unlock (&app_context->channel_mapping_lock);
}

void
gzochid_channel_close_direct (gzochid_application_context *app_context,
			      guint64 channel_oid)
{
  g_mutex_lock (&app_context->channel_mapping_lock);

  g_hash_table_remove
    (app_context->channel_oids_to_local_session_oids, &channel_oid);

  g_mutex_unlock (&app_context->channel_mapping_lock);
}

/*
  Does the work of sending the specified message to the sessions in the 
  specified array of `guint64'. If any oid is found to be locally unmapped, it
  is removed from the channel's session list.

  The reason this function is separated from `gzochid_channel_message_direct' is
  that the transactional message sender needs to be able to send messages to
  only those clients who were members of the channel within the scope of the
  transaction. `gzochid_channel_message_direct' just sends the message to all
  current channel members.
*/

static void
send_channel_message_direct (gzochid_application_context *app_context,
			     guint64 channel_oid, GArray *session_oids,
			     const unsigned char *msg, size_t len)
{
  int i = 0;
  GSequence *sessions = g_hash_table_lookup
    (app_context->channel_oids_to_local_session_oids, &channel_oid);
  
  for (; i < session_oids->len; i++)
    { 
      guint64 session_oid = g_array_index (session_oids, guint64, i);
      gzochid_game_client *client = g_hash_table_lookup
	(app_context->oids_to_clients, &session_oid);
      
      /* Grab the client connection to which the session oid corresponds. */
	      
      if (client != NULL)
	{
	  gzochid_event_dispatch
	    (app_context->event_source,
	     g_object_new (GZOCHID_TYPE_EVENT, "type", MESSAGE_SENT, NULL));
	  
	  gzochid_game_client_send (client, msg, len);
	}
      else
	{
	  /* If no client was found for the specified session, remove it from 
	     the set of local session oids so we don't waste time on it again; 
	     a conveniently lazy way of purging old members. */
		  
	  GSequenceIter *iter = g_sequence_lookup
	    (sessions, &session_oid, gzochid_util_guint64_data_compare, NULL);
		  
	  if (iter != NULL)
	    {
	      g_debug 
		("Client not found for messaged channel session '%"
		 G_GUINT64_FORMAT "'; removing.", session_oid);
	      
	      g_sequence_remove (iter);

	      if (g_sequence_get_begin_iter (sessions) ==
		  g_sequence_get_end_iter (sessions))
		
		g_hash_table_remove
		  (app_context->channel_oids_to_local_session_oids,
		   &channel_oid);
	    }
	}
    }
}

/* `GFunc' implementation to pack each oid in the channel's session list into
   the transaction context's `GArray'. */

static void
append_session_oid (gpointer data, gpointer user_data)
{
  guint64 *session_oid_ptr = data;
  g_array_append_val (user_data, *session_oid_ptr);
}

void
gzochid_channel_message_direct (gzochid_application_context *app_context,
				guint64 channel_oid, GBytes *msg_bytes)
{
  size_t data_len = 0;
  const unsigned char *data = g_bytes_get_data (msg_bytes, &data_len);
  GSequence *sessions = NULL;
  
  g_mutex_lock (&app_context->channel_mapping_lock);
  g_mutex_lock (&app_context->client_mapping_lock);

  sessions = g_hash_table_lookup
    (app_context->channel_oids_to_local_session_oids, &channel_oid);

  if (sessions != NULL &&
      g_sequence_get_begin_iter (sessions) !=
      g_sequence_get_end_iter (sessions))
    {
      /* Capture the current state of channel membership. */
      
      GArray *session_oids = g_array_sized_new
	(FALSE, FALSE, sizeof (guint64), g_sequence_get_length (sessions));

      g_sequence_foreach (sessions, append_session_oid, session_oids);      

      send_channel_message_direct
	(app_context, channel_oid, session_oids, data, data_len);

      g_array_free (session_oids, TRUE);
    }

  g_mutex_unlock (&app_context->client_mapping_lock);
  g_mutex_unlock (&app_context->channel_mapping_lock);
}

/* Makes the channel side effect permanent with respect to the current state of
   the channel on this application server node. */

static void
channel_side_effect_commit (gpointer data)
{
  gzochid_channel_side_effect_transaction_context *tx_context = data;

  if (tx_context->side_effect != NULL)
    {
      GzochidChannelClient *channelclient = NULL;

      /* If there is a meta client, get its channel client preemptively, since
	 we're likely to need it below and the `g_object_get' invocation is
	 verbose. */
      
      if (tx_context->app_context->metaclient != NULL)
	g_object_get
	  (tx_context->app_context->metaclient,
	   "channel-client", &channelclient,
	   NULL);
      
      if (tx_context->side_effect->op == GZOCHID_CHANNEL_OP_SEND)
	{
	  gzochid_channel_message_side_effect *message_side_effect =
	    (gzochid_channel_message_side_effect *) tx_context->side_effect;

	  g_mutex_lock (&tx_context->app_context->channel_mapping_lock);
	  g_mutex_lock (&tx_context->app_context->client_mapping_lock);
	  
	  send_channel_message_direct
	    (tx_context->app_context, tx_context->side_effect->channel_oid,
	     message_side_effect->session_oids, message_side_effect->msg,
	     message_side_effect->len);

	  if (channelclient != NULL)
	    {
	      /* If we're in distributed mode, also send the message to the
		 meta server for broadcast to the other nodes. */
	      
	      GBytes *msg = g_bytes_new_static
		(message_side_effect->msg, message_side_effect->len);

	      gzochid_channelclient_relay_message_from
		(channelclient, tx_context->app_context->descriptor->name,
		 tx_context->side_effect->channel_oid, msg);

	      g_bytes_unref (msg);
	    }
	  
	  g_mutex_unlock (&tx_context->app_context->client_mapping_lock);
	  g_mutex_unlock (&tx_context->app_context->channel_mapping_lock);
	}
      else if (tx_context->side_effect->op == GZOCHID_CHANNEL_OP_CLOSE)
	{
	  gzochid_channel_close_direct
	    (tx_context->app_context, tx_context->side_effect->channel_oid);

	  if (channelclient != NULL)

	    /* If we're in distributed mode, also direct other nodes to close
	       the channel locally. */
	    
	    gzochid_channelclient_relay_close_from
	      (channelclient, tx_context->app_context->descriptor->name,
	       tx_context->side_effect->channel_oid);
	}
      else
	{
	  GError *err = NULL;
	  GzochidChannelClient *channelclient = NULL;
	  gzochid_channel_membership_side_effect *membership_side_effect =
	    (gzochid_channel_membership_side_effect *) tx_context->side_effect;

	  if (tx_context->side_effect->op == GZOCHID_CHANNEL_OP_JOIN)
	    {
	      gzochid_channel_join_direct
		(tx_context->app_context, tx_context->side_effect->channel_oid,
		 membership_side_effect->session_oid, &err);

	      if (err != NULL)
		{
		  if (err->code != GZOCHID_CHANNEL_ERROR_NOT_MAPPED &&
		      channelclient != NULL)

		    /* If we're in distributed mode and the channel couldn't
		       be joined because the session doesn't exist locally,
		       forward the join to the meta server so it can relay it to
		       the node to which the session's connected. */
		    
		    gzochid_channelclient_relay_join_from
		      (channelclient, tx_context->app_context->descriptor->name,
		       tx_context->side_effect->channel_oid,
		       membership_side_effect->session_oid);
		  
		  g_error_free (err);		  
		}
	    }
	  else if (tx_context->side_effect->op == GZOCHID_CHANNEL_OP_LEAVE)
	    {
	      gzochid_channel_leave_direct
		(tx_context->app_context, tx_context->side_effect->channel_oid,
		 membership_side_effect->session_oid, NULL);

	      if (err != NULL)
		{
		  if (err->code != GZOCHID_CHANNEL_ERROR_NOT_MAPPED &&
		      channelclient != NULL)

		    /* If we're in distributed mode and the channel couldn't
		       be left because the session doesn't exist locally,
		       forward the removal to the meta server so it can relay 
		       it to the node to which the session's connected. */
		    
		    gzochid_channelclient_relay_leave_from
		      (channelclient, tx_context->app_context->descriptor->name,
		       tx_context->side_effect->channel_oid,
		       membership_side_effect->session_oid);

		  g_error_free (err);
		}
	    }
	}

      /* Unref the client if we had a reference to it. */
      
      if (channelclient != NULL)
	g_object_unref (channelclient);
    }

  cleanup_side_effect_transaction (tx_context);
}

/* Abort the current transaction, discarding any pending side effect. */

static void
channel_side_effect_rollback (gpointer data)
{
  cleanup_side_effect_transaction (data);
}

static gzochid_transaction_participant channel_side_effect_participant =
  { 
    "channel-side-effect", 
    channel_side_effect_prepare, 
    channel_side_effect_commit, 
    channel_side_effect_rollback 
  };

/* Join the current general transaction, initiating one if there isn't one 
   already active, and bind an empty side effect transactional state to it. (Or,
   if we've already joined the transaction, just return the existing 
   transactional state. */

static gzochid_channel_side_effect_transaction_context *
join_side_effect_transaction (gzochid_application_context *app_context)
{
  gzochid_channel_side_effect_transaction_context *tx_context = NULL;

  if (!gzochid_transaction_active()
      || (gzochid_transaction_context 
	  (&channel_side_effect_participant) == NULL))
    {
      tx_context =
	malloc (sizeof (gzochid_channel_side_effect_transaction_context));
      
      tx_context->app_context = app_context;
      tx_context->side_effect = NULL;
  
      gzochid_transaction_join (&channel_side_effect_participant, tx_context);
    }
  else tx_context = gzochid_transaction_context
	 (&channel_side_effect_participant);

  return tx_context;
}

/* Close the target channel as a side effect of the current transaction. */

static void
close_channel (gzochid_application_context *context,
	       gzochid_auth_identity *identity, gpointer data)
{
  gzochid_channel_pending_operation *op = data;

  g_mutex_lock (&context->channel_mapping_lock);

  if (g_hash_table_contains
      (context->channel_oids_to_local_session_oids, &op->target_channel))
    {
      gzochid_channel_side_effect_transaction_context *tx_context =
	join_side_effect_transaction (context);

      tx_context->side_effect = gzochid_channel_side_effect_new
	(GZOCHID_CHANNEL_OP_CLOSE, op->target_channel);
    }
  
  g_mutex_unlock (&context->channel_mapping_lock);
}

/* Send a message to the target channel's members as a side effect of the 
   current transaction. */

static void
send_channel_message (gzochid_application_context *context,
		      gzochid_auth_identity *identity, gpointer data)
{
  gzochid_channel_pending_send_operation *send_op = data;
  gzochid_channel_pending_operation *op =
    (gzochid_channel_pending_operation *) send_op;

  g_mutex_lock (&context->channel_mapping_lock);

  if (g_hash_table_contains
      (context->channel_oids_to_local_session_oids, &op->target_channel)
      || context->metaclient != NULL)
    {
      gzochid_channel_side_effect_transaction_context *tx_context =
	join_side_effect_transaction (context);
      GSequence *sessions = g_hash_table_lookup
	(context->channel_oids_to_local_session_oids, &op->target_channel);
      GArray *session_oids = g_array_new (FALSE, FALSE, sizeof (guint64));

      if (sessions != NULL)
	{
	  GSequenceIter *iter = g_sequence_get_begin_iter (sessions);
	  
	  /* Take a snapshot of the current membership of the channel. */
      
	  while (!g_sequence_iter_is_end (iter))
	    {
	      guint64 *session_oid = g_sequence_get (iter);
	      
	      g_array_append_val (session_oids, *session_oid);
	      iter = g_sequence_iter_next (iter);
	    }
	}

      tx_context->side_effect = gzochid_channel_message_side_effect_new
	(GZOCHID_CHANNEL_OP_SEND, op->target_channel, session_oids,
	 send_op->message, send_op->len);
	  
      g_array_unref (session_oids);
    }
  
  g_mutex_unlock (&context->channel_mapping_lock);
}

/* Add the target session to the target channel as a side effect of the current
   transaction. */

static void
join_channel (gzochid_application_context *context,
	      gzochid_auth_identity *identity, gpointer data)
{
  gzochid_channel_pending_membership_operation *member_op = data;
  gzochid_channel_pending_operation *op = 
    (gzochid_channel_pending_operation *) member_op;

  gboolean do_join = TRUE;

  g_mutex_lock (&context->channel_mapping_lock);

  if (g_hash_table_contains
      (context->channel_oids_to_local_session_oids, &op->target_channel))
    {
      GSequence *sessions = g_hash_table_lookup
	(context->channel_oids_to_local_session_oids, &op->target_channel);
      GSequenceIter *iter = g_sequence_lookup
	(sessions, &member_op->target_session,
	 gzochid_util_guint64_data_compare, NULL);

      do_join = iter == NULL;
    }

  g_mutex_unlock (&context->channel_mapping_lock);

  if (do_join)
    {
      gzochid_channel_side_effect_transaction_context *tx_context =
	join_side_effect_transaction (context);

      tx_context->side_effect = gzochid_channel_membership_side_effect_new
	(GZOCHID_CHANNEL_OP_JOIN, op->target_channel,
	 member_op->target_session);
    }
}

/* Remove the target session to the target channel as a side effect from the 
   current transaction. */

static void
leave_channel (gzochid_application_context *context,
	       gzochid_auth_identity *identity, gpointer data)
{
  gzochid_channel_pending_membership_operation *member_op = data;
  gzochid_channel_pending_operation *op = 
    (gzochid_channel_pending_operation *) member_op;

  gboolean do_leave = FALSE;

  g_mutex_lock (&context->channel_mapping_lock);

  if (g_hash_table_contains
      (context->channel_oids_to_local_session_oids, &op->target_channel))
    {
      GSequence *sessions = g_hash_table_lookup
	(context->channel_oids_to_local_session_oids, &op->target_channel);
      GSequenceIter *iter = g_sequence_lookup
	(sessions, &member_op->target_session,
	 gzochid_util_guint64_data_compare, NULL);

      do_leave = iter != NULL;
    }

  g_mutex_unlock (&context->channel_mapping_lock);

  if (do_leave)
    {
      gzochid_channel_side_effect_transaction_context *tx_context =
	join_side_effect_transaction (context);

      tx_context->side_effect = gzochid_channel_membership_side_effect_new
	(GZOCHID_CHANNEL_OP_LEAVE, op->target_channel,
	 member_op->target_session);
    }
}

/* The following data structures and functions make up the logical channel
   operation transaction system. */

struct _gzochid_channel_transaction_context 
{
  /* The gzochi game application context to which the channel belongs. */
  
  gzochid_application_context *context;

  /* A managed reference to the queue of pending channel operations to be 
     scheduled. */
  
  gzochid_data_managed_reference *queue_ref; 
};

typedef struct _gzochid_channel_transaction_context
gzochid_channel_transaction_context;

static int
channel_prepare (gpointer data)
{
  return TRUE;
}

/* The logical channel operation transaction participant doesn't actually need
   specialized commit and rollback processes; the queue of operations is 
   scheduled and persisted at the beginning of the transaction and can be 
   appended to for the duration of the transaction. If the general transaction
   commits, the queue and its contents will be persisted and scheduled; if the
   general transaction rolls back, it'll be cleaned up and forgotten. To put it
   another way, this transactional service achieves its outcomes via other 
   transational services. */

static gzochid_transaction_participant channel_participant =
  { "channel", channel_prepare, free, free };

static gzochid_channel_transaction_context *
join_transaction (gzochid_application_context *context)
{
  gzochid_channel_transaction_context *tx_context = NULL;
  if (!gzochid_transaction_active()
      || gzochid_transaction_context (&channel_participant) == NULL)
    {
      gzochid_durable_queue *queue = gzochid_durable_queue_new (context);
      gzochid_data_managed_reference *queue_ref = gzochid_data_create_reference
	(context, &gzochid_durable_queue_serialization, queue, NULL);

      /* The reference to the queue may be `NULL' if the data transaction 
	 deadlocks or times out. If that happens, this function shouldn't join
	 the general transaction will return `NULL' itself. */
      
      if (queue_ref != NULL)
	{
	  tx_context = malloc (sizeof (gzochid_channel_transaction_context));

	  tx_context->context = context;
	  tx_context->queue_ref = queue_ref;

	  gzochid_transaction_join (&channel_participant, tx_context);

	  /* Schedule the task chain even though it's currently empty; it'll
	     probably have tasks added to it before the transaction commits. */
	  
	  gzochid_schedule_durable_task_chain
	    (context, gzochid_auth_system_identity (),
	     tx_context->queue_ref->obj, NULL);
	}
      else gzochid_durable_queue_free (queue);
    }
  else tx_context = gzochid_transaction_context (&channel_participant);

  return tx_context;
}

/*
  Returns a newly-allocated string containing the binding name for the
  specified channel name: s.channel.[name]
  
  Free this string with `free' when no longer in use.
*/

static char *
make_channel_binding (char *name)
{
  int prefix_len = strlen (CHANNEL_PREFIX);
  int name_len = strlen (name) + 1;
  char *binding = malloc (sizeof (char) * (prefix_len + name_len));

  strncpy (binding, CHANNEL_PREFIX, prefix_len);
  strncpy (binding + prefix_len, name, name_len);

  return binding;
}

gzochid_channel *
gzochid_channel_create (gzochid_application_context *context, char *name)
{
  GError *err = NULL;
  char *binding = NULL;
  gzochid_channel *channel = gzochid_channel_new (name);
  gzochid_data_managed_reference *reference = gzochid_data_create_reference 
    (context, &gzochid_channel_serialization, channel, NULL);
  gzochid_data_managed_reference *scm_reference = NULL;
  SCM scm_channel = SCM_BOOL_F;

  if (reference == NULL)
    {
      g_debug ("Failed to persist struct for new channel '%s'.", name);
      
      /* If the reference can't be created (because the transaction's in a bad
	 state) free the memory used by the not-yet-persisted channel struct. */
      
      gzochid_channel_free (channel);
      return NULL;
    }

  channel->oid = reference->oid;

  scm_channel = gzochid_scheme_create_channel (channel, reference->oid);
  scm_reference = gzochid_data_create_reference
    (context, &gzochid_scheme_data_serialization, scm_channel, NULL);

  if (scm_reference == NULL)
    {
      g_debug
	("Failed to create Scheme representation of new channel '%s'.", name);
      return NULL;
    }

  channel->scm_oid = scm_reference->oid;
  
  binding = make_channel_binding (name);
  gzochid_data_set_binding_to_oid (context, binding, reference->oid, &err);

  if (err != NULL)
    {
      g_debug ("Failed to create binding for channel: %s", err->message);
      g_error_free (err);

      free (binding);
      return NULL;
    }
  else 
    {
      free (binding);
      return channel;
    }
}

gzochid_channel *
gzochid_channel_get (gzochid_application_context *context, char *name)
{
  char *binding = make_channel_binding (name);
  gzochid_channel *channel = gzochid_data_get_binding
    (context, binding, &gzochid_channel_serialization, NULL);

  free (binding);

  return channel;
}

/* The logical channel operation task worker; a dispatcher for the individual
   per-operation worker functions. */

static void
channel_operation_worker (gzochid_application_context *context,
			  gzochid_auth_identity *identity, gpointer data)
{
  gzochid_channel_pending_operation *op = data;
  gzochid_data_managed_reference *op_reference =
    gzochid_data_create_reference
    (context, &gzochid_channel_operation_serialization, op, NULL);

  if (op_reference == NULL)
    return;
  
  switch (op->type)
    {
    case GZOCHID_CHANNEL_OP_JOIN: join_channel (context, identity, data); break;

    case GZOCHID_CHANNEL_OP_LEAVE:
      leave_channel (context, identity, data);
      break;

    case GZOCHID_CHANNEL_OP_SEND:
      send_channel_message (context, identity, data);
      break;
      
    case GZOCHID_CHANNEL_OP_CLOSE:
      close_channel (context, identity, data);
      break;
      
    default: assert (1 == 0);
    }

  gzochid_data_remove_object (op_reference, NULL);
}

/* Durable task serialization boilerplate for logical channel operation
   transactional tasks. */
			  
static void
serialize_channel_operation_worker (gzochid_application_context *context,
				    gzochid_application_worker worker,
				    GByteArray *out)
{
}

static gzochid_application_worker
deserialize_channel_operation_worker (gzochid_application_context *context,
				      GByteArray *in)
{
  return channel_operation_worker;
}

static gzochid_application_worker_serialization
gzochid_channel_operation_worker_serialization =
  {
    serialize_channel_operation_worker,
    deserialize_channel_operation_worker
  };

gzochid_application_task_serialization
gzochid_channel_operation_task_serialization =
  {
    "channel-operation",
    &gzochid_channel_operation_worker_serialization,
    &gzochid_channel_operation_serialization
  };

void
gzochid_channel_join (gzochid_application_context *context,
		      gzochid_channel *channel, 
		      gzochid_client_session *session)
{
  gzochid_data_managed_reference *channel_reference = NULL;
  gzochid_data_managed_reference *session_reference = NULL;
  gzochid_channel_transaction_context *tx_context = join_transaction (context);
  gzochid_durable_application_task_handle *task_handle = NULL;

  gzochid_channel_pending_membership_operation *join_operation = malloc 
    (sizeof (gzochid_channel_pending_membership_operation));
  gzochid_channel_pending_operation *operation = 
    (gzochid_channel_pending_operation *) join_operation;

  gzochid_application_task *app_task = NULL;
  
  if (tx_context == NULL)
    return;
  
  channel_reference = gzochid_data_create_reference 
    (context, &gzochid_channel_serialization, channel, NULL);

  /* The reference for the channel must already be cached in the current
     transaction context. */
  
  assert (channel_reference != NULL);
  
  session_reference = gzochid_data_create_reference 
    (context, &gzochid_client_session_serialization, session, NULL);

  /* The reference for the session must already be cached in the current
     transaction context. */

  assert (session_reference != NULL);

  operation->type = GZOCHID_CHANNEL_OP_JOIN;
  
  operation->target_channel = channel_reference->oid;
  join_operation->target_session = session_reference->oid;

  /* Create a task to execute the join... */

  app_task = gzochid_application_task_new
     (context, gzochid_auth_system_identity (), channel_operation_worker,
      operation);
  
  task_handle = gzochid_create_durable_application_task_handle
    (app_task, &gzochid_channel_operation_task_serialization,
     (struct timeval) { 0, 0 }, NULL, NULL);

  /* Decrease the ref count, as it's been handed off to the task handle. */
  
  gzochid_application_task_unref (app_task);
  
  /* ...and add it to the task queue. */

  if (task_handle != NULL)
    gzochid_durable_queue_offer
      (tx_context->queue_ref->obj,
       &gzochid_durable_application_task_handle_serialization,
       task_handle, NULL);

  /* If the task handle couldn't be created, we have to clean up the channel
     operation explicitly. */
  
  else finalize_channel_operation (context, operation);
}

void
gzochid_channel_leave (gzochid_application_context *context,
		       gzochid_channel *channel, 
		       gzochid_client_session *session)
{
  gzochid_data_managed_reference *channel_reference = NULL;
  gzochid_data_managed_reference *session_reference = NULL;
  gzochid_channel_transaction_context *tx_context = join_transaction (context);
  gzochid_durable_application_task_handle *task_handle = NULL;

  gzochid_channel_pending_membership_operation *leave_operation = malloc 
    (sizeof (gzochid_channel_pending_membership_operation));
  gzochid_channel_pending_operation *operation = 
    (gzochid_channel_pending_operation *) leave_operation;

  gzochid_application_task *app_task = NULL;

  channel_reference = gzochid_data_create_reference 
    (context, &gzochid_channel_serialization, channel, NULL);

  /* The reference for the channel must already be cached in the current
     transaction context. */

  assert (channel_reference != NULL);

  session_reference =  gzochid_data_create_reference 
    (context, &gzochid_client_session_serialization, session, NULL);

  /* The reference for the session must already be cached in the current
     transaction context. */

  assert (session_reference != NULL);
  
  operation->type = GZOCHID_CHANNEL_OP_LEAVE;
  operation->target_channel = channel_reference->oid;

  leave_operation->target_session = session_reference->oid;

  /* Create a task to execute the exit from the channel... */

  app_task = gzochid_application_task_new
    (context, gzochid_auth_system_identity (), channel_operation_worker,
     operation);
  
  task_handle = gzochid_create_durable_application_task_handle
    (app_task, &gzochid_channel_operation_task_serialization,
     (struct timeval) { 0, 0 }, NULL, NULL);

  /* Decrease the ref count, as it's been handed off to the task handle. */

  gzochid_application_task_unref (app_task);
  
  /* ...and add it to the task queue. */

  if (task_handle != NULL)
    gzochid_durable_queue_offer
      (tx_context->queue_ref->obj,
       &gzochid_durable_application_task_handle_serialization,
       task_handle, NULL);

  /* If the task handle couldn't be created, we have to clean up the channel
     operation explicitly. */
  
  else finalize_channel_operation (context, operation);
}

void
gzochid_channel_send (gzochid_application_context *context,
		      gzochid_channel *channel, unsigned char *message,
		      short len)
{
  gzochid_data_managed_reference *channel_reference = NULL;
  gzochid_channel_transaction_context *tx_context = join_transaction (context);
  gzochid_durable_application_task_handle *task_handle = NULL;

  gzochid_channel_pending_send_operation *send_operation = malloc 
    (sizeof (gzochid_channel_pending_send_operation));
  gzochid_channel_pending_operation *operation = 
    (gzochid_channel_pending_operation *) send_operation;

  gzochid_application_task *app_task = NULL;

  channel_reference = gzochid_data_create_reference 
    (context, &gzochid_channel_serialization, channel, NULL);

  /* The reference for the channel must already be cached in the current
     transaction context. */

  assert (channel_reference != NULL);
  
  operation->type = GZOCHID_CHANNEL_OP_SEND;
  send_operation->message = malloc (sizeof (unsigned char) * len);

  memcpy (send_operation->message, message, len);
  
  send_operation->len = len;

  operation->target_channel = channel_reference->oid;

  /* Create a task to execute the message broadcast... */

  app_task = gzochid_application_task_new
    (context, gzochid_auth_system_identity (), channel_operation_worker,
     operation);    
  
  task_handle = gzochid_create_durable_application_task_handle
    (app_task, &gzochid_channel_operation_task_serialization,
     (struct timeval) { 0, 0 }, NULL, NULL);

  /* Decrease the ref count, as it's been handed off to the task handle. */

  gzochid_application_task_unref (app_task);

  /* ...and add it to the task queue. */

  if (task_handle != NULL)
    gzochid_durable_queue_offer
      (tx_context->queue_ref->obj,
       &gzochid_durable_application_task_handle_serialization,
       task_handle, NULL);

  /* If the task handle couldn't be created, we have to clean up the channel
     operation explicitly. */
  
  else finalize_channel_operation (context, operation);
}

void
gzochid_channel_close (gzochid_application_context *context,
		       gzochid_channel *channel)
{
  gzochid_data_managed_reference *channel_reference = NULL;
  gzochid_channel_transaction_context *tx_context = join_transaction (context);
  gzochid_durable_application_task_handle *task_handle = NULL;

  gzochid_channel_pending_operation *operation = malloc 
    (sizeof (gzochid_channel_pending_operation));

  gzochid_application_task *app_task = NULL;

  channel_reference = gzochid_data_create_reference 
    (context, &gzochid_channel_serialization, channel, NULL);

  /* The reference for the channel must already be cached in the current
     transaction context. */

  assert (channel_reference != NULL);

  operation->type = GZOCHID_CHANNEL_OP_CLOSE;
  operation->target_channel = channel_reference->oid;
  
  /* Create a task to execute the channel shutdown... */

  app_task = gzochid_application_task_new
    (context, gzochid_auth_system_identity (), channel_operation_worker,
     operation);
  
  task_handle = gzochid_create_durable_application_task_handle
    (app_task, &gzochid_channel_operation_task_serialization,
     (struct timeval) { 0, 0 }, NULL, NULL);

  /* Decrease the ref count, as it's been handed off to the task handle. */

  gzochid_application_task_unref (app_task);
  
  /* ...and add it to the task queue. */

  if (task_handle != NULL)
    gzochid_durable_queue_offer
      (tx_context->queue_ref->obj,
       &gzochid_durable_application_task_handle_serialization,
       task_handle, NULL);

  /* If the task handle couldn't be created, we have to clean up the channel
     operation explicitly. */
  
  else finalize_channel_operation (context, operation);
}

GQuark
gzochid_channel_error_quark ()
{
  return g_quark_from_static_string ("gzochid-channel-error-quark");
}
