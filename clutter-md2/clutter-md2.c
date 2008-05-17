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
#include <gdk-pixbuf/gdk-pixbuf.h>
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
#define CLUTTER_MD2_MAX_SKIN_NAME_LEN  63

struct _ClutterMD2Private
{
  guchar *gl_commands;

  int num_frames;
  int current_frame_a, current_frame_b;
  float current_frame_interval;
  ClutterMD2Frame **frames;

  int num_skins;
  int current_skin;
  GLuint *textures;

  /* Maximum extents of all frames */
  float left, right;
  float top, bottom;
  float back, front;
};

struct _ClutterMD2Frame
{
  float scale[3];
  float translate[3];
  char name[CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1];

  /* Extents of the model in this frame */
  float left, right;
  float top, bottom;
  float back, front;

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
  priv->num_frames = 0;
  priv->current_frame_a = 0;
  priv->current_frame_b = 0;
  priv->frames = NULL;
  priv->num_skins = 0;
  priv->current_skin = 0;
  priv->textures = NULL;
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
  ClutterMD2Frame *frame_a, *frame_b;
  guchar *vertices_a, *vertices_b;
  guchar *gl_command;
  float scale;
  
  clutter_actor_get_geometry (self, &geom);

  if (priv->gl_commands == NULL
      || priv->frames == NULL
      || priv->textures == NULL
      || priv->current_frame_a >= priv->num_frames
      || priv->current_frame_b >= priv->num_frames
      || priv->current_skin >= priv->num_skins
      || geom.width == 0
      || geom.height == 0
      || priv->top == priv->bottom)
    return;

  frame_a = priv->frames[priv->current_frame_a];
  frame_b = priv->frames[priv->current_frame_b];
  vertices_a = frame_a->vertices;
  vertices_b = frame_b->vertices;
  gl_command = priv->gl_commands;

  glPushAttrib (GL_ENABLE_BIT | GL_CURRENT_BIT
		| GL_POLYGON_BIT | GL_TEXTURE_BIT);
  glEnable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);
  glDepthFunc (GL_LEQUAL);
  glBindTexture (GL_TEXTURE_2D, priv->textures[priv->current_skin]);
  glEnable (GL_TEXTURE_2D);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glPushMatrix ();

  /* Scale so that the model fits in either the width or the height of
     the actor, whichever makes the model bigger */
  if ((priv->right - priv->left) / (priv->bottom - priv->top)
      > geom.width / (float) geom.height)
    /* Fit width */
    scale = geom.width / (priv->right - priv->left);
  else
    /* Fit height */
    scale = geom.height / (priv->bottom - priv->top);

  /* Scale about the center of the model and move to the center of the actor */
  glTranslatef (geom.width / 2,
		geom.height / 2,
		0);
  glScalef (scale, scale, scale);
  glTranslatef (-(priv->left + priv->right) / 2,
		-(priv->top + priv->bottom) / 2,
		-(priv->back + priv->front) / 2);

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

	  s = *(float *) gl_command;
	  gl_command += sizeof (float);
	  t = *(float *) gl_command;
	  gl_command += sizeof (float);
	  vertex_num = *(guint32 *) gl_command;
	  gl_command += sizeof (guint32);

	  glTexCoord2f (s, t);

	  if (frame_a == frame_b)
	    {
	      guchar *vertex = vertices_a + vertex_num * 4;

	      glNormal3fv (_clutter_md2_norms + vertex[3] * 3);
	      glVertex3f (vertex[0] * frame_a->scale[0]
			  + frame_a->translate[0],
			  vertex[1] * frame_a->scale[1]
			  + frame_a->translate[1],
			  vertex[2] * frame_a->scale[2]
			  + frame_a->translate[2]);
	    }
	  else
	    {
	      guchar *vertex_ap = vertices_a + vertex_num * 4;
	      guchar *vertex_bp = vertices_b + vertex_num * 4;
	      const float *norm_a = _clutter_md2_norms + vertex_ap[3] * 3;
	      const float *norm_b = _clutter_md2_norms + vertex_bp[3] * 3;
	      float interval = priv->current_frame_interval;
	      float vert_a[] = {
		vertex_ap[0] * frame_a->scale[0] + frame_a->translate[0],
		vertex_ap[1] * frame_a->scale[1] + frame_a->translate[1],
		vertex_ap[2] * frame_a->scale[2] + frame_a->translate[2]
	      };
	      float vert_b[] = {
		vertex_bp[0] * frame_b->scale[0] + frame_b->translate[0],
		vertex_bp[1] * frame_b->scale[1] + frame_b->translate[1],
		vertex_bp[2] * frame_b->scale[2] + frame_b->translate[2]
	      };
	      
	      glNormal3f (norm_a[0] + (norm_b[0] - norm_a[0]) * interval,
			  norm_a[1] + (norm_b[1] - norm_a[1]) * interval,
			  norm_a[2] + (norm_b[2] - norm_a[2]) * interval);
	      glVertex3f (vert_a[0] + (vert_b[0] - vert_a[0]) * interval,
			  vert_a[1] + (vert_b[1] - vert_a[1]) * interval,
			  vert_a[2] + (vert_b[2] - vert_a[2]) * interval);
	    }
	}

      glEnd ();
    }

  glPopMatrix ();

  glPopAttrib ();
}

gint
clutter_md2_get_n_skins (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), 0);

  return md2->priv->num_skins;
}

gint
clutter_md2_get_current_skin (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), 0);

  return md2->priv->current_skin;
}

void
clutter_md2_set_current_skin (ClutterMD2 *md2, gint skin_num)
{
  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (skin_num >= 0 && skin_num < md2->priv->num_skins);

  md2->priv->current_skin = skin_num;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));
}

gint
clutter_md2_get_n_frames (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), 0);

  return md2->priv->num_frames;
}

gint
clutter_md2_get_current_frame (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), 0);

  return md2->priv->current_frame_a;
}

void
clutter_md2_set_current_frame (ClutterMD2 *md2, gint frame_num)
{
  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (frame_num >= 0 && frame_num < md2->priv->num_frames);

  md2->priv->current_frame_a = frame_num;
  md2->priv->current_frame_b = frame_num;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));
}

void
clutter_md2_set_current_frame_by_name (ClutterMD2 *md2, const gchar *frame_name)
{
  ClutterMD2Private *priv;
  int i;

  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (frame_name != NULL);

  priv = md2->priv;

  for (i = 0; i < priv->num_frames; i++)
    if (!strcmp (priv->frames[i]->name, frame_name))
      {
	clutter_md2_set_current_frame (md2, i);
	break;
      }
}

void
clutter_md2_set_sub_frame (ClutterMD2 *md2, gint frame_a, gint frame_b,
			   gfloat interval)
{
  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (frame_a >= 0 && frame_a < md2->priv->num_frames);
  g_return_if_fail (frame_b >= 0 && frame_b < md2->priv->num_frames);

  md2->priv->current_frame_a = frame_a;
  md2->priv->current_frame_b = frame_b;
  md2->priv->current_frame_interval = interval;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));
}

const gchar *
clutter_md2_get_frame_name (ClutterMD2 *md2, gint frame_num)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), NULL);
  g_return_val_if_fail (frame_num >= 0
			&& frame_num < md2->priv->num_frames,
			NULL);

  return md2->priv->frames[frame_num]->name;
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
  if (size == 0)
    size = 1;

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

  priv->left = priv->top = priv->back = FLT_MAX;
  priv->right = priv->bottom = priv->front = -FLT_MAX;

  for (i = 0; i < num_frames; i++)
    {
      guchar data[sizeof (float) * 6 + CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1];
      guchar *p = data;
      ClutterMD2Frame *frame;

      if (!clutter_md2_read (data, sizeof (data), file,
			     display_name, error))
	return FALSE;

      if ((priv->frames[i] = clutter_md2_check_malloc (display_name,
						       sizeof (ClutterMD2Frame)
						       + num_vertices
						       * 4, error)) == NULL)
	return FALSE;

      frame = priv->frames[i];

      memcpy (frame->scale, p, sizeof (float) * 3);
      p += sizeof (float) * 3;
      memcpy (frame->translate, p, sizeof (float) * 3);
      p += sizeof (float) * 3;
      memcpy (frame->name, p, CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1);
      p += CLUTTER_MD2_MAX_FRAME_NAME_LEN + 1;
      frame->name[CLUTTER_MD2_MAX_FRAME_NAME_LEN] = '\0';

      if (!clutter_md2_read (frame->vertices, num_vertices * 4,
			     file, display_name, error))
	return FALSE;

      frame->left = frame->top = frame->back = FLT_MAX;
      frame->right = frame->bottom = frame->front = -FLT_MAX;

      /* Check all of the normal indices and calculate the extents */
      for (p = frame->vertices + num_vertices * 4; p > frame->vertices;)
	{
	  float x, y, z;

	  p -= 4;

	  if (p[3] >= CLUTTER_MD2_NORMS_COUNT)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_ERROR,
			   CLUTTER_MD2_ERROR_INVALID_FILE,
			   "'%s' is invalid",
			   display_name);

	      return FALSE;
	    }

	  x = p[0] * frame->scale[0] + frame->translate[0];
	  y = p[1] * frame->scale[1] + frame->translate[1];
	  z = p[2] * frame->scale[2] + frame->translate[2];

	  if (x < frame->left)
	    frame->left = x;
	  if (x > frame->right)
	    frame->right = x;
	  if (y < frame->top)
	    frame->top = y;
	  if (y > frame->bottom)
	    frame->bottom = y;
	  if (z < frame->back)
	    frame->back = z;
	  if (z > frame->front)
	    frame->front = z;
	}

      if (frame->left < priv->left)
	priv->left = frame->left;
      if (frame->right > priv->right)
	priv->right = frame->right;
      if (frame->top < priv->top)
	priv->top = frame->top;
      if (frame->bottom > priv->bottom)
	priv->bottom = frame->bottom;
      if (frame->back < priv->back)
	priv->back = frame->back;
      if (frame->front > priv->front)
	priv->front = frame->front;
    }

  return TRUE;
}

static gboolean
clutter_md2_load_skins (ClutterMD2 *md2, FILE *file,
			const gchar *file_name,
			const gchar *display_name,
			guint32 num_skins,
			guint32 file_offset,
			GError **error)
{
  ClutterMD2Private *priv = md2->priv;
  int i;

  if (priv->textures)
    {
      glDeleteTextures (priv->num_skins, priv->textures);
      g_free (priv->textures);
    }

  priv->num_skins = num_skins;

  if ((priv->textures = clutter_md2_check_malloc (display_name,
						  sizeof (GLuint)
						  * num_skins, error)) == NULL)
    return FALSE;

  memset (priv->textures, 0, sizeof (GLuint) * num_skins);
  glGenTextures (priv->num_skins, priv->textures);

  if (!clutter_md2_seek (file, file_offset, display_name, error))
    return FALSE;

  for (i = 0; i < num_skins; i++)
    {
      GdkPixbuf *pixbuf;
      gchar skin_name[CLUTTER_MD2_MAX_SKIN_NAME_LEN + 1];
      gchar *dir_name, *full_skin_path;
      int bpp, rowstride, alignment = 1;

      if (!clutter_md2_read (skin_name, CLUTTER_MD2_MAX_SKIN_NAME_LEN + 1,
			     file, display_name, error))
	return FALSE;

      skin_name[CLUTTER_MD2_MAX_SKIN_NAME_LEN] = '\0';

      /* Assume the file name of the skin is relative to the directory
	 containing the MD2 file */
      dir_name = g_path_get_dirname (file_name);
      full_skin_path = g_build_filename (dir_name, skin_name, NULL);
      g_free (dir_name);

      pixbuf = gdk_pixbuf_new_from_file (full_skin_path, error);
      
      g_free (full_skin_path);

      if (pixbuf == NULL)
	return FALSE;

      bpp = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
      rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      while ((rowstride & 1) == 0 && alignment < 8)
	{
	  rowstride >>= 1;
	  alignment <<= 1;
	}
      
      glBindTexture (GL_TEXTURE_2D, priv->textures[i]);
      glPixelStorei (GL_UNPACK_ROW_LENGTH,
		     gdk_pixbuf_get_rowstride (pixbuf) / bpp);
      glPixelStorei (GL_UNPACK_ALIGNMENT, alignment);

      glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

      glTexImage2D (GL_TEXTURE_2D, 0,
		    gdk_pixbuf_get_has_alpha (pixbuf) ? GL_RGBA : GL_RGB,
		    gdk_pixbuf_get_width (pixbuf),
		    gdk_pixbuf_get_height (pixbuf),
		    0,
		    gdk_pixbuf_get_has_alpha (pixbuf) ? GL_RGBA : GL_RGB,
		    GL_UNSIGNED_BYTE,
		    gdk_pixbuf_get_pixels (pixbuf));

      g_object_unref (pixbuf);
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

  priv->current_frame_a = 0;
  priv->current_frame_b = 0;
  priv->num_frames = 0;

  if (priv->textures)
    {
      glDeleteTextures (priv->num_skins, priv->textures);

      g_free (priv->textures);

      priv->textures = NULL;
    }

  priv->current_skin = 0;
  priv->num_skins = 0;
}

gboolean
clutter_md2_load (ClutterMD2 *md2, const gchar *filename, GError **error)
{
  gboolean ret = TRUE;
  FILE *file;
  gchar *display_name;
  ClutterMD2Private *priv;

  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  priv = md2->priv;

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
	  else if (!clutter_md2_load_skins
		   (md2, file, filename, display_name,
		    header[CLUTTER_MD2_HEADER_NUM_SKINS],
		    header[CLUTTER_MD2_HEADER_OFFSET_SKINS],
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
  else
    {
      if (priv->current_frame_a >= priv->num_frames
	  || priv->current_frame_b >= priv->num_frames)
	priv->current_frame_b = priv->current_frame_a = 0;
      if (priv->current_skin >= priv->num_skins)
	priv->current_skin = 0;
    }

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));

  return ret;
}

static void
clutter_md2_finalize (GObject *self)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);

  clutter_md2_free_data (md2);
}
