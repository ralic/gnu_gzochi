/* auth.c: Authorization management routines for gzochid
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

#include <glib.h>
#include <gmodule.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "app.h"
#include "auth_int.h"
#include "game.h"
#include "gzochid-auth.h"
#include "log.h"
#include "util.h"

#define PLUGIN_INFO_FUNCTION "gzochid_auth_init_plugin"

#ifndef GZOCHID_AUTH_PLUGIN_LOCATION
#define GZOCHID_AUTH_PLUGIN_LOCATION "./auth"
#endif /* GZOCHID_AUTH_PLUGIN_LOCATION */

GQuark gzochid_auth_plugin_error_quark (void)
{
  return g_quark_from_static_string ("gzochid-auth-plugin-error-quark");
}

gzochid_auth_identity *gzochid_auth_identity_new (char *name)
{
  gzochid_auth_identity *identity = calloc (1, sizeof (gzochid_auth_identity));

  identity->name = name;

  return identity;
}

void gzochid_auth_identity_free (gzochid_auth_identity *identity)
{
  free (identity->name);
  free (identity);
}

gzochid_auth_identity *gzochid_auth_function_pass_thru 
(unsigned char *cred, short cred_len, gpointer auth_data, GError **error)
{
  if (cred_len <= 0)
    return NULL;
  return gzochid_auth_identity_new (strndup ((char *) cred, cred_len));
}

gzochid_auth_identity *gzochid_auth_identity_clone
(gzochid_auth_identity *identity)
{
  return gzochid_auth_identity_new (strdup (identity->name));
}

void 
gzochid_auth_identity_serializer 
(gzochid_application_context *context, void *ptr, GString *out, GError **err)
{
  gzochid_auth_identity *identity = (gzochid_auth_identity *) ptr;
  g_string_append_len (out, identity->name, strlen (identity->name) + 1);
}

void *
gzochid_auth_identity_deserializer
(gzochid_application_context *context, GString *in, GError **err)
{
  char *name = strndup (in->str, in->len);
  gzochid_auth_identity *identity = gzochid_auth_identity_new (name);

  g_string_erase (in, 0, strlen (name) + 1);
  return identity;
}

void gzochid_auth_identity_finalizer
(gzochid_application_context *context, void *ptr)
{
  gzochid_auth_identity *identity = (gzochid_auth_identity *) ptr;

  free (identity->name);
  free (identity);
}

static gchar *remove_extension (const gchar *filename)
{
  char *dot = rindex (filename, '.');

  if (dot == NULL)
    return g_strdup (filename);
  else return g_strndup (filename, dot - filename);
}

static void probe_auth_plugin (gpointer data, gpointer user_data)
{
  const gchar *path = (const gchar *) data;
  gzochid_game_context *context = (gzochid_game_context *) user_data;
  GModule *plugin_handle = 
    g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  int (*plugin_info) (gzochid_auth_plugin *);

  if (plugin_handle == NULL)
    gzochid_warning 
      ("Failed to load auth plugin at '%s': %s", path, g_module_error ());
  else if (!g_module_symbol 
      (plugin_handle, PLUGIN_INFO_FUNCTION, (gpointer *) &plugin_info))
    {
      gzochid_warning
	("Missing plugin info at '%s': %s", path, g_module_error ());
      g_module_close (plugin_handle);
    }
  else 
    {
      gzochid_auth_plugin *plugin = malloc (sizeof (gzochid_auth_plugin));
      if (plugin_info (plugin) != 0)
	{
	  gzochid_warning ("Failed to introspect plugin at '%s'.", path);

	  free (plugin);
	  g_module_close (plugin_handle);
	}
      else 
	{
	  gzochid_info ("Loaded auth plugin '%s'", plugin->info->name);
	  g_hash_table_insert 
	    (context->auth_plugins, plugin->info->name, plugin);
	  plugin->handle = plugin_handle;
	}
    }
}

static void probe_auth_plugins 
(gzochid_game_context *context, const char *search_path)
{
  GDir *plugin_dir = g_dir_open (search_path, 0, NULL);
  const gchar *file = NULL;

  if (plugin_dir != NULL)
    {
      GSequence *plugin_queue = g_sequence_new (free);
      while ((file = g_dir_read_name (plugin_dir)) != NULL)
	{
	  gchar *stripped_file = remove_extension (file);
	  gchar *path = g_build_filename (search_path, stripped_file, NULL);

	  if (g_sequence_lookup 
	      (plugin_queue, path, gzochid_util_string_data_compare, NULL) 
	      == NULL)
	    g_sequence_insert_sorted 
	      (plugin_queue, path, gzochid_util_string_data_compare, NULL);
	}
      
      g_sequence_foreach (plugin_queue, probe_auth_plugin, context);
      g_sequence_free (plugin_queue);
    }
}

void gzochid_auth_init (gzochid_game_context *context)
{
  char *plugin_search_path = GZOCHID_AUTH_PLUGIN_LOCATION;

  if (g_module_supported ())
    probe_auth_plugins (context, plugin_search_path);
  else gzochid_info ("Plugins not supported; skipping auth plugin probe.");
}
