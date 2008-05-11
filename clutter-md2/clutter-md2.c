/*
 * Clutter-MD2.
 *
 * A Clutter actor to render MD2 models
 *
 * Authored By Neil Roberts  <neil@o-hand.com>
 *
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <glib/gstdio.h>
#include <clutter/clutter-actor.h>
#include <GL/gl.h>
#include <errno.h>
#include <string.h>

#include "clutter-md2.h"
#include "clutter-md2-norms.h"

#define CLUTTER_MD2_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_MD2, ClutterMD2Private))

G_DEFINE_TYPE (ClutterMD2, clutter_md2, CLUTTER_TYPE_ACTOR);

static void clutter_md2_paint (ClutterActor *self);
static void clutter_md2_finalize (GObject *self);

typedef struct _ClutterMD2Frame ClutterMD2Frame;

#define CLUTTER_MD2_FORMAT_MAGIC   0x32504449 /* IDP2 */
#define CLUTTER_MD2_FORMAT_VERSION 8

/* If the file tries to load some data that occupies more than this
   number of bytes then assume the file is invalid instead of trying
   to allocate large amounts of memory */
#define CLUTTER_MD2_MAX_MEM_SIZE (4 * 1024 * 1024)

#define CLUTTER_MD2_MAX_FRAME_NAME_LEN 15

struct _ClutterMD2Private
{
  guchar *gl_commands;

  int num_frames;
  int current_frame;

  ClutterMD2Frame **frames;
};

struct _ClutterMD2Frame
{
  float scale[3];
  float translate[3];
  char name[CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1];
  guchar vertices[1];
};

enum
  {
    CLUTTER_MD2_HEADER_MAGIC,
    CLUTTER_MD2_HEADER_VERSION,
    CLUTTER_MD2_HEADER_SKIN_WIDTH,
    CLUTTER_MD2_HEADER_SKIN_HEIGHT,
    CLUTTER_MD2_HEADER_FRAME_SIZE,
    CLUTTER_MD2_HEADER_NUM_SKINS,
    CLUTTER_MD2_HEADER_NUM_VERTICES,
    CLUTTER_MD2_HEADER_NUM_TEX_COORDS,
    CLUTTER_MD2_HEADER_NUM_TRIANGLES,
    CLUTTER_MD2_HEADER_NUM_GL_COMMANDS,
    CLUTTER_MD2_HEADER_NUM_FRAMES,
    CLUTTER_MD2_HEADER_OFFSET_SKINS,
    CLUTTER_MD2_HEADER_OFFSET_TEX_COORDS,
    CLUTTER_MD2_HEADER_OFFSET_TRIANGLES,
    CLUTTER_MD2_HEADER_OFFSET_FRAMES,
    CLUTTER_MD2_HEADER_OFFSET_GL_COMMANDS,
    CLUTTER_MD2_HEADER_OFFSET_END,
    CLUTTER_MD2_HEADER_COUNT
  };

GQuark
clutter_md2_error_quark (void)
{
  return g_quark_from_static_string ("clutter-md2-error-quark");
}

static void
clutter_md2_class_init (ClutterMD2Class *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  actor_class->paint = clutter_md2_paint;

  object_class->finalize = clutter_md2_finalize;

  g_type_class_add_private (klass, sizeof (ClutterMD2Private));
}

static void
clutter_md2_init (ClutterMD2 *self)
{
  ClutterMD2Private *priv;

  self->priv = priv = CLUTTER_MD2_GET_PRIVATE (self);

  priv->gl_commands = NULL;
  priv->frames = NULL;
  priv->num_frames = 0;
  priv->current_frame = 0;
}

ClutterActor *
clutter_md2_new (void)
{
  return g_object_new (CLUTTER_TYPE_MD2, NULL);
}

static void
clutter_md2_paint (ClutterActor *self)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);
  ClutterMD2Private *priv = md2->priv;
  ClutterGeometry geom;
  ClutterMD2Frame *frame;
  guchar *vertices;
  guchar *gl_command;
  
  if (priv->gl_commands == NULL || priv->frames == NULL
      || priv->current_frame >= priv->num_frames)
    return;

  frame = priv->frames[priv->current_frame];
  vertices = frame->vertices;
  gl_command = priv->gl_commands;

  clutter_actor_get_geometry (self, &geom);

  glPushAttrib (GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT);
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);
  glColor3f (0.0f, 0.0f, 0.0f);
  glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);

  while (*(gint32 *) gl_command)
    {
      gint32 command_len = *(gint32 *) gl_command;

      gl_command += sizeof (gint32);

      if (command_len < 0)
	{
	  glBegin (GL_TRIANGLE_FAN);
	  command_len = -command_len;
	}
      else
	glBegin (GL_TRIANGLE_STRIP);

      while (command_len-- > 0)
	{
	  float s, t;
	  int vertex_num;
	  guchar *vertex;

	  s = *(float *) gl_command;
	  gl_command += sizeof (float);
	  t = *(float *) gl_command;
	  gl_command += sizeof (float);
	  vertex_num = *(guint32 *) gl_command;
	  gl_command += sizeof (guint32);

	  vertex = vertices + vertex_num * 4;

	  glTexCoord2f (s, t);
	  glNormal3fv (_clutter_md2_norms + vertex[3] * 3);
	  glVertex3f (vertex[0] * frame->scale[0] + frame->translate[0],
		      vertex[1] * frame->scale[1] + frame->translate[1],
		      vertex[2] * frame->scale[2] + frame->translate[2]);
	}

      glEnd ();
    }

  glPopAttrib ();
}

static gboolean
clutter_md2_read (void *data, size_t size, FILE *file,
		  const gchar *display_name, GError **error)
{
  if (fread (data, 1, size, file) < size)
    {
      if (ferror (file))
	{
	  gint save_errno = errno;
	  
	  g_set_error (error,
		       G_FILE_ERROR,
		       g_file_error_from_errno (save_errno),
		       "Error reading '%s': %s",
		       display_name,
		       g_strerror (save_errno));
	}
      else
	g_set_error (error,
		     CLUTTER_MD2_ERROR,
		     CLUTTER_MD2_ERROR_INVALID_FILE,
		     "Invalid MD2 file '%s': file too short",
		     display_name);

      return FALSE;
    }
  else
    return TRUE;
}

static gboolean
clutter_md2_seek (FILE *file, size_t offset,
		  const gchar *display_name, GError **error)
{
  if (fseek (file, offset, SEEK_SET))
    {
      gint save_errno = errno;
      
      g_set_error (error,
		   G_FILE_ERROR,
		   g_file_error_from_errno (save_errno),
		   "Error reading '%s': %s",
		   display_name,
		   g_strerror (save_errno));

      return FALSE;
    }
  else
    return TRUE;
}

static gpointer
clutter_md2_check_malloc (const gchar *display_name,
			  size_t size,
			  GError **error)
{
  if (size > CLUTTER_MD2_MAX_MEM_SIZE)
    {
      g_set_error (error,
		   CLUTTER_MD2_ERROR,
		   CLUTTER_MD2_ERROR_INVALID_FILE,
		   "'%s' is invalid",
		   display_name);

      return NULL;
    }
  else
    return g_malloc (size);
}

static gboolean
clutter_md2_load_gl_commands (ClutterMD2 *md2, FILE *file,
			      const gchar *display_name,
			      guint32 num_vertices,
			      guint32 num_commands,
			      guint32 file_offset,
			      GError **error)
{
  ClutterMD2Private *priv = md2->priv;
  guchar *p;
  int byte_len = num_commands * sizeof (guint32);

  if (!clutter_md2_seek (file, file_offset, display_name, error))
    return FALSE;

  if (priv->gl_commands)
    g_free (priv->gl_commands);

  if ((priv->gl_commands = clutter_md2_check_malloc (display_name,
						     byte_len,
						     error)) == NULL)
    return FALSE;

  if (!clutter_md2_read (priv->gl_commands, byte_len,
			 file, display_name, error))
    return FALSE;

  /* Verify the data and convert to native byte order */
  p = priv->gl_commands;
  while (byte_len > sizeof (guint32))
    {
      /* Convert the command length from little endian and take the
	 absolute value. (If the command length is negative it
	 represents a triangle fan, otherwise it should be a triangle
	 strip) */
      gint32 command_len = ABS (*(gint32 *) p = GINT32_FROM_LE (*(gint32 *) p));
      int i;

      if (command_len * (sizeof (float) * 2 + sizeof (gint32))
	  + sizeof (gint32) > byte_len)
	{
	  g_set_error (error,
		       CLUTTER_MD2_ERROR,
		       CLUTTER_MD2_ERROR_INVALID_FILE,
		       "'%s' is invalid",
		       display_name);

	  return FALSE;
	}

      p += sizeof (gint32);

      for (i = 0; i < command_len; i++)
	{
	  guint32 vertex_num;

	  p += sizeof (float) * 2;
	  *(guint32 *) p = vertex_num = GUINT32_FROM_LE (*(guint32 *) p);
	  p += sizeof (gint32);

	  /* Make sure the vertex number is in range */
	  if (vertex_num >= num_vertices)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_ERROR,
			   CLUTTER_MD2_ERROR_INVALID_FILE,
			   "'%s' is invalid",
			   display_name);

	      return FALSE;
	    }
	}

      byte_len -= command_len * (sizeof (float) * 2 + sizeof (gint32))
	+ sizeof (gint32);
    }

  /* The end of the commands list should be terminated with a 0 */
  if (byte_len != sizeof (guint32) || *(guint32 *) p != 0)
    {
      g_set_error (error,
		   CLUTTER_MD2_ERROR,
		   CLUTTER_MD2_ERROR_INVALID_FILE,
		   "'%s' is invalid",
		   display_name);

      return FALSE;
    }

  return TRUE;
}

static gboolean
clutter_md2_load_frames (ClutterMD2 *md2, FILE *file,
			 const gchar *display_name,
			 guint32 num_frames,
			 guint32 num_vertices,
			 guint32 file_offset,
			 GError **error)
{
  ClutterMD2Private *priv = md2->priv;
  int i;

  if (priv->frames)
    {
      for (i = 0; i < priv->num_frames; i++)
	if (priv->frames[i])
	  g_free (priv->frames[i]);
      g_free (priv->frames);
    }

  priv->num_frames = num_frames;

  if ((priv->frames = clutter_md2_check_malloc (display_name,
						sizeof (ClutterMD2Frame *)
						* num_frames, error)) == NULL)
    return FALSE;

  memset (priv->frames, 0, sizeof (ClutterMD2Frame *) * num_frames);

  if (!clutter_md2_seek (file, file_offset, display_name, error))
    return FALSE;
  
  for (i = 0; i < num_frames; i++)
    {
      guchar data[sizeof (float) * 6 + CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1];
      guchar *p = data;

      if (!clutter_md2_read (data, sizeof (data), file,
			     display_name, error))
	return FALSE;

      if ((priv->frames[i] = clutter_md2_check_malloc (display_name,
						       sizeof (ClutterMD2Frame)
						       + num_vertices
						       * 4, error)) == NULL)
	return FALSE;

      memcpy (priv->frames[i]->scale, p, sizeof (float) * 3);
      p += sizeof (float) * 3;
      memcpy (priv->frames[i]->translate, p, sizeof (float) * 3);
      p += sizeof (float) * 3;
      memcpy (priv->frames[i]->name, p, CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1);
      p += CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1;
      priv->frames[i]->name[CLUTTER_MD2_MAX_FRAME_NAME_LEN] = '\0';

      if (!clutter_md2_read (priv->frames[i]->vertices, num_vertices * 4,
			     file, display_name, error))
	return FALSE;

      /* Check all of the normal indices */
      for (p = (guchar *) priv->frames[i]->vertices + num_vertices * 4;
	   p > (guchar *) priv->frames[i]->vertices;
	   p -= 4)
	if (*(p - 1) >= CLUTTER_MD2_NORMS_COUNT)
	  {
	    g_set_error (error,
			 CLUTTER_MD2_ERROR,
			 CLUTTER_MD2_ERROR_INVALID_FILE,
			 "'%s' is invalid",
			 display_name);

	    return FALSE;
	  }
    }

  return TRUE;
}

static void
clutter_md2_free_data (ClutterMD2 *md2)
{
  ClutterMD2Private *priv = md2->priv;

  if (priv->gl_commands)
    {
      g_free (priv->gl_commands);
      priv->gl_commands = NULL;
    }

  if (priv->frames)
    {
      int i;

      for (i = 0; i < priv->num_frames; i++)
	if (priv->frames[i])
	  g_free (priv->frames[i]);

      g_free (priv->frames);
      
      priv->frames = NULL;
    }

  priv->current_frame = 0;
  priv->num_frames = 0;
}

gboolean
clutter_md2_load (ClutterMD2 *md2, const gchar *filename, GError **error)
{
  gboolean ret = TRUE;
  FILE *file;
  gchar *display_name;

  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  display_name = g_filename_display_name (filename);

  if ((file = g_fopen (filename, "rb")) == NULL)
    {
      gint save_errno = errno;

      g_set_error (error,
		   G_FILE_ERROR,
		   g_file_error_from_errno (save_errno),
		   "Failed to open file '%s': %s",
		   display_name,
		   g_strerror (save_errno));

      ret = FALSE;
    }
  else
    {
      guint32 header[CLUTTER_MD2_HEADER_COUNT];

      if (!clutter_md2_read (header, sizeof (header), file,
			     display_name, error))
	ret = FALSE;
      else
	{
	  int i;

	  for (i = 0; i < CLUTTER_MD2_HEADER_COUNT; i++)
	    header[i] = GUINT32_FROM_LE (header[i]);

	  if (header[CLUTTER_MD2_HEADER_MAGIC] != CLUTTER_MD2_FORMAT_MAGIC)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_ERROR,
			   CLUTTER_MD2_ERROR_INVALID_FILE,
			   "'%s' is not an MD2 file",
			   display_name);
			   
	      ret = FALSE;
	    }
	  else if (header[CLUTTER_MD2_HEADER_VERSION]
		   != CLUTTER_MD2_FORMAT_VERSION)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_ERROR,
			   CLUTTER_MD2_ERROR_BAD_VERSION,
			   "Unsupported MD2 version %i for '%s'",
			   header[CLUTTER_MD2_HEADER_VERSION],
			   display_name);
			   
	      ret = FALSE;
	    }
	  else if (!clutter_md2_load_gl_commands
		   (md2, file, display_name,
		    header[CLUTTER_MD2_HEADER_NUM_VERTICES],
		    header[CLUTTER_MD2_HEADER_NUM_GL_COMMANDS],
		    header[CLUTTER_MD2_HEADER_OFFSET_GL_COMMANDS],
		    error))
	    ret = FALSE;
	  else if (!clutter_md2_load_frames
		   (md2, file, display_name,
		    header[CLUTTER_MD2_HEADER_NUM_FRAMES],
		    header[CLUTTER_MD2_HEADER_NUM_VERTICES],
		    header[CLUTTER_MD2_HEADER_OFFSET_FRAMES],
		    error))
	    ret = FALSE;
	}

      fclose (file);
    }

  g_free (display_name);

  /* If the loading failed then free all of the data so we don't try
     to draw it */
  if (!ret)
    clutter_md2_free_data (md2);

  return ret;
}

static void
clutter_md2_finalize (GObject *self)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);

  clutter_md2_free_data (md2);
}
