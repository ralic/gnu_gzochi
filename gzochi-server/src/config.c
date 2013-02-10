/* config.c: Configuration management routines for gzochid
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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "app.h"
#include "config.h"
#include "log.h"

int gzochid_config_to_boolean (char *str, int def)
{
  if (str == NULL)
    return def;
  else if (strcasecmp (str, "true") == 0)
    return TRUE;
  else if (strcasecmp (str, "false") == 0)
    return FALSE;
  return def;
}

int gzochid_config_to_int (char *str, int def)
{
  int ret = 0;
  if (str == NULL)
    return def;
  if (sscanf (str, "%d", &ret) == 1)
    return ret;
  return def;
}

GHashTable *gzochid_config_keyfile_extract_config 
(GKeyFile *key_file, char *group)
{
  unsigned int i = 0, num_keys = 0;
  char **keys = g_key_file_get_keys (key_file, group, &num_keys, NULL);
  GHashTable *config = g_hash_table_new (g_str_hash, g_str_equal);

  for (; i < num_keys; i++) 
    g_hash_table_insert 
      (config, keys[i], g_key_file_get_value (key_file, group, keys[i], NULL));
  
  return config;
}

typedef struct _descriptor_builder_context
{
  GList *hierarchy;
  gzochid_application_descriptor *descriptor;
} descriptor_builder_context;

static const gchar *find_attribute_value
(const gchar *attribute_name, const gchar **attribute_names, 
 const gchar **attribute_values)
{
  int i = 0;
  while (attribute_names[i] != NULL)
    {
      if (strcmp (attribute_names[i], attribute_name) == 0)
	return attribute_values[i];
      i++;
    }
  return NULL;
}

static GList *to_module_name (const gchar *module_name_str)
{
  GList *module_name = NULL;
  gchar **name_components = g_strsplit (module_name_str, " ", 0);
  int i = 0;
 
  while (name_components[i] != NULL)
    module_name = g_list_append (module_name, strdup (name_components[i++]));

  g_strfreev (name_components);
  return module_name;
}

static void descriptor_start_element (GMarkupParseContext *context,
				      const gchar *element_name,
				      const gchar **attribute_names,
				      const gchar **attribute_values,
				      gpointer user_data,
				      GError **error)
{
  gzochid_application_descriptor *descriptor = 
    (gzochid_application_descriptor *) user_data;
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  char *parent = stack->next == NULL ? NULL : (char *) stack->next->data;

  if (strcmp (element_name, "game") == 0)
    {
      if (parent == NULL)
	{
	  const gchar *attribute_value = find_attribute_value 
	    ("name", attribute_names, attribute_values);

	  if (attribute_value == NULL)
	    *error = g_error_new 
	      (G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE, 
	       "'name' must be provided.");
	  else 
	    {
	      descriptor->name = strdup (attribute_value);
	    }
	}
      else *error = g_error_new 
	     (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	      "'game' must be the document element.");
    }
  else if (strcmp (element_name, "description") == 0)
    {
      if (parent == NULL || strcmp (parent, "game") != 0)
	*error = g_error_new 
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	   "Invalid position for 'description' element.");
    }
  else if (strcmp (element_name, "load-paths") == 0)
    {
      if (parent == NULL || strcmp (parent, "game") != 0)
	*error = g_error_new 
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	   "Invalid position for 'load-paths' element.");
    }
  else if (strcmp (element_name, "load-path") == 0)
    {
      if (parent == NULL || strcmp (parent, "load-paths") != 0)
	*error = g_error_new 
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	   "Invalid position for 'load-path' element.");
    }
  else if (strcmp (element_name, "initialized") == 0
	   || strcmp (element_name, "logged-in") == 0)
    {
      if (parent == NULL || strcmp (parent, "game") != 0)
	*error = g_error_new 
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	   "Invalid position for '%s' element.", element_name);
    }
  else if (strcmp (element_name, "callback") == 0)
    {
      if (parent != NULL)
	{
	  if (strcmp (parent, "initialized") == 0)
	    {
	      const gchar *procedure = find_attribute_value 
		("procedure", attribute_names, attribute_values);
	      const gchar *module_name_str = find_attribute_value 
		("module", attribute_names, attribute_values);
	      GList *module_name = to_module_name (module_name_str);

	      mpz_t scm_oid;

	      mpz_init (scm_oid);
	      mpz_set_si (scm_oid, -1);

	      descriptor->initialized = gzochid_application_callback_new 
		(strdup (procedure), module_name, scm_oid);

	      mpz_clear (scm_oid);
	    }
	  else if (strcmp (parent, "logged-in") == 0)
	    {
	      const gchar *procedure = find_attribute_value 
		("procedure", attribute_names, attribute_values);
	      const gchar *module_name_str = find_attribute_value 
		("module", attribute_names, attribute_values);
	      GList *module_name = to_module_name (module_name_str);

	      mpz_t scm_oid;

	      mpz_init (scm_oid);
	      mpz_set_si (scm_oid, -1);

	      descriptor->logged_in = gzochid_application_callback_new 
		(strdup (procedure), module_name, scm_oid);

	      mpz_clear (scm_oid);
	    }
	  else *error = g_error_new 
		 (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
		  "Invalid position for 'callback' element.");
	}
      else;
    }
  else if (strcmp (element_name, "property") == 0)
    {
      if (parent == NULL || strcmp (parent, "game") != 0)
	*error = g_error_new 
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	   "Invalid position for 'property' element.");
      else
	{
	  const gchar *name = find_attribute_value 
	    ("name", attribute_names, attribute_values);
	  const gchar *value = find_attribute_value 
	    ("value", attribute_names, attribute_values);
	  
	  g_hash_table_insert 
	    (descriptor->properties, strdup (name), strdup (value));
	}
    }
  else *error = g_error_new
	 (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
	  "Unknown element '%s'.", element_name);
}

static void descriptor_end_element (GMarkupParseContext *context,
				    const gchar *element_name,
				    gpointer user_data,
				    GError **error)
{
  gzochid_application_descriptor *descriptor =
    (gzochid_application_descriptor *) user_data;

  if (strcmp (element_name, "game") == 0)
    {
      if (descriptor->initialized == NULL)
	*error = g_error_new
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
	   "Initialization callback definition is required.");
      else if (descriptor->logged_in == NULL)
	*error = g_error_new
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
	   "Login callback definition is required.");
    }
  else if (strcmp (element_name, "initialized") == 0)
    {
      if (descriptor->initialized == NULL)
	*error = g_error_new
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
	   "Missing callback definition for element 'initialized'.");
    }
  else if (strcmp (element_name, "logged-in") == 0)
    {
      if (descriptor->logged_in == NULL)
	*error = g_error_new
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
	   "Missing callback definition for element 'logged-in'.");
    }
}

static void descriptor_text (GMarkupParseContext *context,
			     const gchar *text,
			     gsize text_len,
			     gpointer user_data,
			     GError **error)
{
  gzochid_application_descriptor *descriptor =
    (gzochid_application_descriptor *) user_data;
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  char *parent = stack->next == NULL ? NULL : (char *) stack->next->data;

  if (parent != NULL)
    {
      if (strcmp (parent, "description") == 0)
	descriptor->description = strndup (text, text_len);
      else if (strcmp (parent, "load-path") == 0)
	;
      else;
    }
  else;
}

static void descriptor_error 
(GMarkupParseContext *context, GError *error, gpointer user_data)
{
  gzochid_warning (error->message);
}

GMarkupParser descriptor_parser = 
  {
    descriptor_start_element, 
    descriptor_end_element, 
    descriptor_text, 
    NULL, 
    descriptor_error
  };

gzochid_application_descriptor *gzochid_config_parse_application_descriptor
(char *filename)
{
  gzochid_application_descriptor *descriptor = NULL;  
  GMarkupParseContext *context = NULL; 

  char buf[1024];
  int bytes_read = 0;
  int xml_fd = open (filename, O_RDONLY);
  char *filename_copy = strdup (filename);
  char *deployment_root = dirname (filename_copy);

  if (xml_fd < 0)
    return NULL;

  descriptor = calloc (1, sizeof (gzochid_application_descriptor));
  descriptor->properties = g_hash_table_new (g_str_hash, g_str_equal);
  descriptor->deployment_root = strdup (deployment_root);
  descriptor->load_paths = g_list_append (NULL, strdup (deployment_root));

  free (filename_copy);

  context = g_markup_parse_context_new 
    (&descriptor_parser, 0, descriptor, NULL);
 
  while ((bytes_read = read (xml_fd, buf, 1024)) >= 0)
    {
      g_markup_parse_context_parse (context, buf, bytes_read, NULL);      
      if (bytes_read < 1024)
	break;
    }
  
  close (xml_fd);

  g_markup_parse_context_end_parse (context, NULL);
  g_markup_parse_context_free (context);
  
  return descriptor;
}
