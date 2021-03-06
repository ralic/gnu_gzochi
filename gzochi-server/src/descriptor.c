/* descriptor.c: Game application descriptor parsing routines for gzochid
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
#include <libgen.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "descriptor.h"

struct _descriptor_builder_context
{
  GList *hierarchy;
  GzochidApplicationDescriptor *descriptor;
};

typedef struct _descriptor_builder_context  descriptor_builder_context;

static const gchar *
find_attribute_value (const gchar *attribute_name, 
		      const gchar **attribute_names, 
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

static GList *
to_module_name (const gchar *module_name_str)
{
  GList *module_name = NULL;
  gchar **name_components = g_strsplit (module_name_str, " ", 0);
  int i = 0;
 
  while (name_components[i] != NULL)
    module_name = g_list_append (module_name, strdup (name_components[i++]));

  g_strfreev (name_components);
  return module_name;
}

static void 
descriptor_start_element (GMarkupParseContext *context,
			  const gchar *element_name,
			  const gchar **attribute_names,
			  const gchar **attribute_values, gpointer user_data,
			  GError **error)
{
  GzochidApplicationDescriptor *descriptor = user_data;
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  char *parent = stack->next == NULL ? NULL : stack->next->data;

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
	  else descriptor->name = strdup (attribute_value);
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
  else if (strcmp (element_name, "auth") == 0)
    {
      if (parent == NULL || strcmp (parent, "game") != 0)
	*error = g_error_new 
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
	   "Invalid position for 'auth' element.");
      else 
	{
	  const gchar *type = find_attribute_value 
	    ("type", attribute_names, attribute_values);

	  if (type == NULL)
	    *error = g_error_new
	      (G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
	       "Attribute 'type' is required for 'auth' element.");
	  else descriptor->auth_type = strdup (type);
	}
    }
  else if (strcmp (element_name, "initialized") == 0
	   || strcmp (element_name, "logged-in") == 0
	   || strcmp (element_name, "ready") == 0)
    
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
	  const gchar *procedure = find_attribute_value 
	    ("procedure", attribute_names, attribute_values);
	  const gchar *module_name_str = find_attribute_value 
	    ("module", attribute_names, attribute_values);
	  GList *module_name = to_module_name (module_name_str);

	  if (strcmp (parent, "initialized") == 0)
	    descriptor->initialized = gzochid_application_callback_new 
	      (procedure, module_name, 0);
	  else if (strcmp (parent, "logged-in") == 0)
	    descriptor->logged_in = gzochid_application_callback_new 
	      (procedure, module_name, 0);
	  else if (strcmp (parent, "ready") == 0)
	    descriptor->ready = gzochid_application_callback_new
	      (procedure, module_name, 0);
	  else *error = g_error_new 
		 (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
		  "Invalid position for 'callback' element.");

	  g_list_free_full (module_name, (GDestroyNotify) free);
	}
      else;
    }
  else if (strcmp (element_name, "property") == 0)
    {
      if (parent != NULL)
	{
	  const gchar *name = find_attribute_value 
	    ("name", attribute_names, attribute_values);
	  const gchar *value = find_attribute_value 
	    ("value", attribute_names, attribute_values);
	  
	  if (strcmp (parent, "game") == 0)
	    g_hash_table_insert 
	      (descriptor->properties, strdup (name), strdup (value));
	  else if (strcmp (parent, "auth") == 0)
	    g_hash_table_insert 
	      (descriptor->auth_properties, strdup (name), strdup (value));
	  else *error = g_error_new 
		 (G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, 
		  "Invalid position for 'property' element.");
	}	
    }
  else *error = g_error_new
	 (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
	  "Unknown element '%s'.", element_name);
}

static void 
descriptor_end_element (GMarkupParseContext *context, const gchar *element_name,
			gpointer user_data, GError **error)
{
  GzochidApplicationDescriptor *descriptor = user_data;

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
  else if (strcmp (element_name, "ready") == 0)
    {
      if (descriptor->ready == NULL)
	*error = g_error_new
	  (G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
	   "Missing callback definition for element 'ready'.");
    }
}

static void 
descriptor_text (GMarkupParseContext *context, const gchar *text,
		 gsize text_len, gpointer user_data, GError **error)
{
  GzochidApplicationDescriptor *descriptor = user_data;
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  char *parent = stack->data;

  if (parent != NULL)
    {
      if (strcmp (parent, "description") == 0)
	descriptor->description = strndup (text, text_len);
    }
}

GMarkupParser descriptor_parser = 
  {
    descriptor_start_element, 
    descriptor_end_element, 
    descriptor_text, 
    NULL,
    NULL
  };

/* Boilerplate setup for the application descriptor object. */

G_DEFINE_TYPE (GzochidApplicationDescriptor, gzochid_application_descriptor,
	       G_TYPE_OBJECT);

static void
application_descriptor_finalize (GObject *object)
{
  GzochidApplicationDescriptor *self = GZOCHID_APPLICATION_DESCRIPTOR (object);

  if (self->name != NULL)
    free (self->name);
  if (self->description != NULL)
    free (self->description);

  g_list_free_full (self->load_paths, (GDestroyNotify) free);

  if (self->initialized != NULL)
    gzochid_application_callback_free (self->initialized);
  if (self->logged_in != NULL)
    gzochid_application_callback_free (self->logged_in);
  if (self->ready != NULL)
    gzochid_application_callback_free (self->ready);

  if (self->auth_type != NULL)
    free (self->auth_type);
  
  g_hash_table_destroy (self->properties);
  g_hash_table_destroy (self->auth_properties);  

  G_OBJECT_CLASS (gzochid_application_descriptor_parent_class)
    ->finalize (object);
}

static void
gzochid_application_descriptor_class_init
(GzochidApplicationDescriptorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = application_descriptor_finalize;
}

static void
gzochid_application_descriptor_init (GzochidApplicationDescriptor *self)
{
  self->properties = g_hash_table_new_full
    (g_str_hash, g_str_equal, (GDestroyNotify) free, (GDestroyNotify) free);
  self->auth_properties = g_hash_table_new_full
    (g_str_hash, g_str_equal, (GDestroyNotify) free, (GDestroyNotify) free);
}

/* End boilerplate. */

GzochidApplicationDescriptor *
gzochid_config_parse_application_descriptor (FILE *file)
{
  GzochidApplicationDescriptor *descriptor = g_object_new
    (GZOCHID_TYPE_APPLICATION_DESCRIPTOR, NULL);
  GMarkupParseContext *context = NULL; 
  GError *err = NULL;
  
  char buf[1024];
  int bytes_read = 0;

  context = g_markup_parse_context_new 
    (&descriptor_parser, 0, descriptor, NULL);
 
  while ((bytes_read = fread (buf, sizeof (char), 1024, file)) >= 0)
    {
      g_markup_parse_context_parse (context, buf, bytes_read, &err);      
      if (bytes_read < 1024 || err != NULL)
	break;
    }

  if (err == NULL)
    g_markup_parse_context_end_parse (context, &err);

  if (err != NULL)
    {
      g_warning ("%s", err->message);      
      g_error_free (err);

      g_object_unref (descriptor);
      descriptor = NULL;
    }
  
  g_markup_parse_context_free (context);
  
  return descriptor;
}
