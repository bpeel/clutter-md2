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
#include <clutter/clutter-util.h>
#include <errno.h>
#include <string.h>
/* Include cogl to get the right GL header for this platform */
#include <cogl/cogl.h>

#include "clutter-md2-data.h"
#include "clutter-md2-norms.h"

#define CLUTTER_MD2_DATA_GET_PRIVATE(obj)			\
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_MD2_DATA,	\
				ClutterMD2DataPrivate))

G_DEFINE_TYPE (ClutterMD2Data, clutter_md2_data, G_TYPE_INITIALLY_UNOWNED);

static void clutter_md2_data_finalize (GObject *self);
static void clutter_md2_data_get_property (GObject    *self,
					   guint       property_id,
					   GValue     *value,
					   GParamSpec *pspec);

typedef struct _ClutterMD2DataFrame ClutterMD2DataFrame;
typedef struct _ClutterMD2DataState ClutterMD2DataState;

#define CLUTTER_MD2_DATA_FORMAT_MAGIC       0x32504449 /* IDP2 */
#define CLUTTER_MD2_DATA_FORMAT_VERSION     8

/* If the file tries to load some data that occupies more than this
   number of bytes then assume the file is invalid instead of trying
   to allocate large amounts of memory */
#define CLUTTER_MD2_DATA_MAX_MEM_SIZE       (4 * 1024 * 1024)

/* If the skin size is bigger than this then assume the file is
   invalid */
#define CLUTTER_MD2_DATA_MAX_SKIN_SIZE      65536

#define CLUTTER_MD2_DATA_MAX_FRAME_NAME_LEN 15
#define CLUTTER_MD2_DATA_MAX_SKIN_NAME_LEN  63

#define CLUTTER_MD2_DATA_FLOATS_PER_VERTEX  (3 + 3 + 2)

struct _ClutterMD2DataPrivate
{
  guchar *gl_commands;

  int num_frames;
  ClutterMD2DataFrame **frames;

  int skin_width, skin_height;
  int num_skins;
  GLuint *textures;
  int textures_size;

  /* Buffer for vertices to pass to OpenGL */
  GLfloat *vertices;
  guint vertices_size;

  /* Maximum extents of all frames */
  ClutterMD2DataExtents extents;
};

struct _ClutterMD2DataFrame
{
  float scale[3];
  float translate[3];
  char name[CLUTTER_MD2_DATA_MAX_FRAME_NAME_LEN + 1];

  /* Extents of the model in this frame */
  ClutterMD2DataExtents extents;

  guchar vertices[1];
};

/* Variables for some of the OpenGL state so that it can be preserved
   across paint calls */
struct _ClutterMD2DataState
{
  GLboolean depth_test      : 1;
  GLboolean blend           : 1;
  GLboolean texture_2d      : 1;
  GLboolean rectangle       : 1;
  GLboolean tex_coord_array : 1;
  GLboolean normal_array    : 1;
  GLboolean vertex_array    : 1;
  GLboolean color_array     : 1;
  GLint     depth_func;
  GLint     texture_num;
  GLint     tex_env_mode;
};

enum
  {
    CLUTTER_MD2_DATA_HEADER_MAGIC,
    CLUTTER_MD2_DATA_HEADER_VERSION,
    CLUTTER_MD2_DATA_HEADER_SKIN_WIDTH,
    CLUTTER_MD2_DATA_HEADER_SKIN_HEIGHT,
    CLUTTER_MD2_DATA_HEADER_FRAME_SIZE,
    CLUTTER_MD2_DATA_HEADER_NUM_SKINS,
    CLUTTER_MD2_DATA_HEADER_NUM_VERTICES,
    CLUTTER_MD2_DATA_HEADER_NUM_TEX_COORDS,
    CLUTTER_MD2_DATA_HEADER_NUM_TRIANGLES,
    CLUTTER_MD2_DATA_HEADER_NUM_GL_COMMANDS,
    CLUTTER_MD2_DATA_HEADER_NUM_FRAMES,
    CLUTTER_MD2_DATA_HEADER_OFFSET_SKINS,
    CLUTTER_MD2_DATA_HEADER_OFFSET_TEX_COORDS,
    CLUTTER_MD2_DATA_HEADER_OFFSET_TRIANGLES,
    CLUTTER_MD2_DATA_HEADER_OFFSET_FRAMES,
    CLUTTER_MD2_DATA_HEADER_OFFSET_GL_COMMANDS,
    CLUTTER_MD2_DATA_HEADER_OFFSET_END,
    CLUTTER_MD2_DATA_HEADER_COUNT
  };

enum
  {
    DATA_CHANGED,
    LAST_SIGNAL
  };

static guint data_signals[LAST_SIGNAL] = { 0, };

enum
  {
    PROP_0,

    PROP_N_SKINS,
    PROP_N_FRAMES,
    PROP_EXTENTS
  };

GQuark
clutter_md2_data_error_quark (void)
{
  return g_quark_from_static_string ("clutter-md2-data-error-quark");
}

static void
clutter_md2_data_class_init (ClutterMD2DataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->finalize = clutter_md2_data_finalize;
  object_class->get_property = clutter_md2_data_get_property;

  g_type_class_add_private (klass, sizeof (ClutterMD2DataPrivate));

  data_signals[DATA_CHANGED] =
    g_signal_new ("data-changed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterMD2DataClass, data_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  pspec = g_param_spec_int ("n_skins", "Number of skins loaded",
			    "The current number of skins loaded",
			    0, G_MAXINT, 0, G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_N_SKINS, pspec);

  pspec = g_param_spec_int ("n_frames", "Number of frames loaded",
			    "The current number of frames loaded",
			    0, G_MAXINT, 0, G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_N_FRAMES, pspec);

  pspec = g_param_spec_boxed ("extents", "Extents for all frames in the model",
			      "The combined extents for all frames "
			      "in the model",
			      CLUTTER_TYPE_MD2_DATA_EXTENTS,
			      G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_EXTENTS, pspec);
}

static void
clutter_md2_data_init (ClutterMD2Data *self)
{
  ClutterMD2DataPrivate *priv;

  self->priv = priv = CLUTTER_MD2_DATA_GET_PRIVATE (self);

  priv->gl_commands = NULL;
  priv->num_frames = 0;
  priv->frames = NULL;
  priv->num_skins = 0;
  priv->textures = NULL;
  priv->vertices = g_malloc (sizeof (GLfloat)
			     * CLUTTER_MD2_DATA_FLOATS_PER_VERTEX
			     * (priv->vertices_size = 1));
}

static void
clutter_md2_data_get_property (GObject    *self,
			       guint       property_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  ClutterMD2Data *data = CLUTTER_MD2_DATA (self);

  switch (property_id)
    {
    case PROP_N_SKINS:
      g_value_set_int (value, clutter_md2_data_get_n_skins (data));
      break;

    case PROP_N_FRAMES:
      g_value_set_int (value, clutter_md2_data_get_n_frames (data));
      break;

    case PROP_EXTENTS:
      {
	ClutterMD2DataExtents extents;
	clutter_md2_data_get_extents (data, &extents);
	g_value_set_boxed (value, &extents);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
    }
}

ClutterMD2Data *
clutter_md2_data_new (void)
{
  return g_object_new (CLUTTER_TYPE_MD2_DATA, NULL);
}

static void
clutter_md2_data_set_vertex_buffer (ClutterMD2Data *self)
{
  glTexCoordPointer (2, GL_FLOAT,
		     CLUTTER_MD2_DATA_FLOATS_PER_VERTEX * sizeof (GLfloat),
		     self->priv->vertices);
  glNormalPointer (GL_FLOAT,
		   CLUTTER_MD2_DATA_FLOATS_PER_VERTEX * sizeof (GLfloat),
		   self->priv->vertices + 2);
  glVertexPointer (3, GL_FLOAT,
		   CLUTTER_MD2_DATA_FLOATS_PER_VERTEX * sizeof (GLfloat),
		   self->priv->vertices + 5);
}

static void
clutter_md2_data_save_state (ClutterMD2DataState *state)
{
  state->depth_test      = glIsEnabled (GL_DEPTH_TEST) ? TRUE : FALSE;
  state->blend           = glIsEnabled (GL_BLEND) ? TRUE : FALSE;
  state->texture_2d      = glIsEnabled (GL_TEXTURE_2D) ? TRUE : FALSE;
#ifdef GL_TEXTURE_RECTANGLE_ARB
  state->rectangle       = glIsEnabled (GL_TEXTURE_RECTANGLE_ARB)
    ? TRUE : FALSE;
#endif
  state->tex_coord_array = glIsEnabled (GL_TEXTURE_COORD_ARRAY) ? TRUE : FALSE;
  state->normal_array    = glIsEnabled (GL_NORMAL_ARRAY) ? TRUE : FALSE;
  state->vertex_array    = glIsEnabled (GL_VERTEX_ARRAY) ? TRUE : FALSE;
  state->color_array     = glIsEnabled (GL_COLOR_ARRAY) ? TRUE : FALSE;

  glGetIntegerv (GL_DEPTH_FUNC, &state->depth_func);
  glGetIntegerv (GL_TEXTURE_BINDING_2D, &state->texture_num);
  glGetTexEnviv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &state->tex_env_mode);  
}

static void
clutter_md2_data_set_enabled (GLenum flag, GLboolean value)
{
  if (value)
    glEnable (flag);
  else
    glDisable (flag);
}

static void
clutter_md2_data_set_client_enabled (GLenum flag, GLboolean value)
{
  if (value)
    glEnableClientState (flag);
  else
    glDisableClientState (flag);
}

static void
clutter_md2_data_restore_state (const ClutterMD2DataState *state)
{
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, state->tex_env_mode);
  glBindTexture (GL_TEXTURE_2D, state->texture_num);
  glDepthFunc (state->depth_func);

  clutter_md2_data_set_client_enabled (GL_COLOR_ARRAY,
				       state->color_array);
  clutter_md2_data_set_client_enabled (GL_VERTEX_ARRAY,
				       state->vertex_array);
  clutter_md2_data_set_client_enabled (GL_NORMAL_ARRAY,
				       state->normal_array);
  clutter_md2_data_set_client_enabled (GL_TEXTURE_COORD_ARRAY,
				       state->tex_coord_array);
#ifdef GL_TEXTURE_RECTANGLE_ARB
  clutter_md2_data_set_enabled (GL_TEXTURE_RECTANGLE_ARB, state->rectangle);
#endif
  clutter_md2_data_set_enabled (GL_TEXTURE_2D,            state->texture_2d);
  clutter_md2_data_set_enabled (GL_BLEND,                 state->blend);
  clutter_md2_data_set_enabled (GL_DEPTH_TEST,            state->depth_test);
}

void
clutter_md2_data_render (ClutterMD2Data        *data,
			 gint                   frame_num_a,
			 gint                   frame_num_b,
			 gfloat                 interval,
			 gint                   skin_num,
			 const ClutterGeometry *geom)
{
  ClutterMD2DataPrivate *priv = data->priv;
  ClutterMD2DataFrame *frame_a, *frame_b;
  guchar *vertices_a, *vertices_b;
  guchar *gl_command;
  float scale;
  ClutterMD2DataState state;
  
  if (priv->gl_commands == NULL
      || priv->frames == NULL
      || priv->textures == NULL
      || frame_num_a >= priv->num_frames
      || frame_num_b >= priv->num_frames
      || skin_num >= priv->num_skins
      || geom->width == 0
      || geom->height == 0
      || priv->extents.top == priv->extents.bottom)
    return;

  frame_a = priv->frames[frame_num_a];
  frame_b = priv->frames[frame_num_b];
  vertices_a = frame_a->vertices;
  vertices_b = frame_b->vertices;
  gl_command = priv->gl_commands;

  clutter_md2_data_save_state (&state);

  glEnable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);
  glDepthFunc (GL_LEQUAL);
  glBindTexture (GL_TEXTURE_2D, priv->textures[skin_num]);
  glEnable (GL_TEXTURE_2D);
#ifdef GL_TEXTURE_RECTANGLE_ARB
  glDisable (GL_TEXTURE_RECTANGLE_ARB);
#endif
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glEnableClientState (GL_NORMAL_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_COLOR_ARRAY);

  clutter_md2_data_set_vertex_buffer (data);

  glPushMatrix ();

  /* Scale so that the model fits in either the width or the height of
     the actor, whichever makes the model bigger */
  if (((priv->extents.right - priv->extents.left)
       / (priv->extents.bottom - priv->extents.top))
      > geom->width / (float) geom->height)
    /* Fit width */
    scale = geom->width / (priv->extents.right - priv->extents.left);
  else
    /* Fit height */
    scale = geom->height / (priv->extents.bottom - priv->extents.top);

  /* Scale about the center of the model and move to the center of the actor */
  glTranslatef (geom->width / 2,
		geom->height / 2,
		0);
  glScalef (scale, scale, scale);
  glTranslatef (-(priv->extents.left + priv->extents.right) / 2,
		-(priv->extents.top + priv->extents.bottom) / 2,
		-(priv->extents.back + priv->extents.front) / 2);

  while (*(gint32 *) gl_command)
    {
      gint32 command_len = *(gint32 *) gl_command;
      GLfloat *vp;
      GLenum draw_mode;
      int i;

      gl_command += sizeof (gint32);

      if (command_len < 0)
	{
	  draw_mode = GL_TRIANGLE_FAN;
	  command_len = -command_len;
	}
      else
	draw_mode = GL_TRIANGLE_STRIP;

      /* Make sure there's enough space in the vertex buffer */
      if (command_len > priv->vertices_size)
	{
	  guint nsize = priv->vertices_size;
	  do
	    nsize *= 2;
	  while (nsize < command_len);
	  priv->vertices = g_realloc (priv->vertices,
				      (priv->vertices_size = nsize)
				      * CLUTTER_MD2_DATA_FLOATS_PER_VERTEX
				      * sizeof (GLfloat));

	  clutter_md2_data_set_vertex_buffer (data);
	}

      vp = priv->vertices;

      /* Fill the vertex buffer */
      for (i = 0; i < command_len; i++)
	{
	  float s, t;
	  int vertex_num;

	  s = *(float *) gl_command;
	  gl_command += sizeof (float);
	  t = *(float *) gl_command;
	  gl_command += sizeof (float);
	  vertex_num = *(guint32 *) gl_command;
	  gl_command += sizeof (guint32);

	  *(vp++) = s;
	  *(vp++) = t;

	  if (frame_a == frame_b)
	    {
	      guchar *vertex = vertices_a + vertex_num * 4;

	      memcpy (vp, _clutter_md2_norms + vertex[3] * 3,
		      sizeof (float) * 3);
	      vp += 3;

	      *(vp++) = vertex[0] * frame_a->scale[0] + frame_a->translate[0];
	      *(vp++) = vertex[1] * frame_a->scale[1] + frame_a->translate[1];
	      *(vp++) = vertex[2] * frame_a->scale[2] + frame_a->translate[2];
	    }
	  else
	    {
	      guchar *vertex_ap = vertices_a + vertex_num * 4;
	      guchar *vertex_bp = vertices_b + vertex_num * 4;
	      const float *norm_a = _clutter_md2_norms + vertex_ap[3] * 3;
	      const float *norm_b = _clutter_md2_norms + vertex_bp[3] * 3;
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
	      
	      *(vp++) = norm_a[0] + (norm_b[0] - norm_a[0]) * interval;
	      *(vp++) = norm_a[1] + (norm_b[1] - norm_a[1]) * interval;
	      *(vp++) = norm_a[2] + (norm_b[2] - norm_a[2]) * interval;

	      *(vp++) = vert_a[0] + (vert_b[0] - vert_a[0]) * interval;
	      *(vp++) = vert_a[1] + (vert_b[1] - vert_a[1]) * interval;
	      *(vp++) = vert_a[2] + (vert_b[2] - vert_a[2]) * interval;
	    }
	}

      glDrawArrays (draw_mode, 0, command_len);
    }

  glPopMatrix ();

  clutter_md2_data_restore_state (&state);
}

gint
clutter_md2_data_get_n_skins (ClutterMD2Data *data)
{
  g_return_val_if_fail (CLUTTER_IS_MD2_DATA (data), 0);

  return data->priv->num_skins;
}

gint
clutter_md2_data_get_n_frames (ClutterMD2Data *data)
{
  g_return_val_if_fail (CLUTTER_IS_MD2_DATA (data), 0);

  return data->priv->num_frames;
}

const gchar *
clutter_md2_data_get_frame_name (ClutterMD2Data *data, gint frame_num)
{
  g_return_val_if_fail (CLUTTER_IS_MD2_DATA (data), NULL);
  g_return_val_if_fail (frame_num >= 0
			&& frame_num < data->priv->num_frames,
			NULL);

  return data->priv->frames[frame_num]->name;
}

void
clutter_md2_data_get_extents (ClutterMD2Data *data,
			      ClutterMD2DataExtents *extents)
{
  g_return_if_fail (CLUTTER_IS_MD2_DATA (data));
  g_return_if_fail (extents != NULL);

  *extents = data->priv->extents;
}

void
clutter_md2_data_get_frame_extents (ClutterMD2Data *data,
				    gint frame_num,
				    ClutterMD2DataExtents *extents)
{
  g_return_if_fail (CLUTTER_IS_MD2_DATA (data));
  g_return_if_fail (extents != NULL);
  g_return_if_fail (frame_num >= 0
		    && frame_num < data->priv->num_frames);

  *extents = data->priv->frames[frame_num]->extents;
}

static gboolean
clutter_md2_data_read (void *data, size_t size, FILE *file,
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
		     CLUTTER_MD2_DATA_ERROR,
		     CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
		     "Invalid MD2 file '%s': file too short",
		     display_name);

      return FALSE;
    }
  else
    return TRUE;
}

static gboolean
clutter_md2_data_seek (FILE *file, size_t offset,
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
clutter_md2_data_check_malloc (const gchar *display_name,
			       size_t size,
			       GError **error)
{
  if (size == 0)
    size = 1;

  if (size > CLUTTER_MD2_DATA_MAX_MEM_SIZE)
    {
      g_set_error (error,
		   CLUTTER_MD2_DATA_ERROR,
		   CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
		   "'%s' is invalid",
		   display_name);

      return NULL;
    }
  else
    return g_malloc (size);
}

static gboolean
clutter_md2_data_load_gl_commands (ClutterMD2Data *data, FILE *file,
				   const gchar *display_name,
				   guint32 num_vertices,
				   guint32 num_commands,
				   guint32 file_offset,
				   GError **error)
{
  ClutterMD2DataPrivate *priv = data->priv;
  guchar *p;
  int byte_len = num_commands * sizeof (guint32);
  guint32 texture_width, texture_height;

  /* The textures are always power of two sized so we may need to
     scale the texture coordinates */
  texture_width = clutter_util_next_p2 (priv->skin_width);
  texture_height = clutter_util_next_p2 (priv->skin_height);

  if (!clutter_md2_data_seek (file, file_offset, display_name, error))
    return FALSE;

  if (priv->gl_commands)
    g_free (priv->gl_commands);

  if ((priv->gl_commands = clutter_md2_data_check_malloc (display_name,
							  byte_len,
							  error)) == NULL)
    return FALSE;

  if (!clutter_md2_data_read (priv->gl_commands, byte_len,
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
		       CLUTTER_MD2_DATA_ERROR,
		       CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
		       "'%s' is invalid",
		       display_name);

	  return FALSE;
	}

      p += sizeof (gint32);

      for (i = 0; i < command_len; i++)
	{
	  guint32 vertex_num;

	  /* Scale the texture coordinates */
	  *(float *) p *= priv->skin_width / (float) texture_width;
	  p += sizeof (float);
	  *(float *) p *= priv->skin_height / (float) texture_height;
	  p += sizeof (float);

	  *(guint32 *) p = vertex_num = GUINT32_FROM_LE (*(guint32 *) p);
	  p += sizeof (guint32);

	  /* Make sure the vertex number is in range */
	  if (vertex_num >= num_vertices)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_DATA_ERROR,
			   CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
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
		   CLUTTER_MD2_DATA_ERROR,
		   CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
		   "'%s' is invalid",
		   display_name);

      return FALSE;
    }

  return TRUE;
}

static gboolean
clutter_md2_data_load_frames (ClutterMD2Data *data, FILE *file,
			      const gchar *display_name,
			      guint32 num_frames,
			      guint32 num_vertices,
			      guint32 file_offset,
			      GError **error)
{
  ClutterMD2DataPrivate *priv = data->priv;
  int i;

  if (priv->frames)
    {
      for (i = 0; i < priv->num_frames; i++)
	if (priv->frames[i])
	  g_free (priv->frames[i]);
      g_free (priv->frames);
    }

  priv->num_frames = num_frames;
  priv->frames = clutter_md2_data_check_malloc (display_name,
						sizeof (ClutterMD2DataFrame *)
						* num_frames, error);

  if (priv->frames == NULL)
    return FALSE;

  memset (priv->frames, 0, sizeof (ClutterMD2DataFrame *) * num_frames);

  if (!clutter_md2_data_seek (file, file_offset, display_name, error))
    return FALSE;

  priv->extents.left = priv->extents.top = priv->extents.back = FLT_MAX;
  priv->extents.right = priv->extents.bottom = priv->extents.front = -FLT_MAX;

  for (i = 0; i < num_frames; i++)
    {
      guchar data[sizeof (float) * 6 + CLUTTER_MD2_DATA_MAX_FRAME_NAME_LEN + 1];
      guchar *p = data;
      ClutterMD2DataFrame *frame;

      if (!clutter_md2_data_read (data, sizeof (data), file,
			     display_name, error))
	return FALSE;

      priv->frames[i] = clutter_md2_data_check_malloc
	(display_name, sizeof (ClutterMD2DataFrame) + num_vertices * 4, error);

      if (priv->frames[i] == NULL)
	return FALSE;

      frame = priv->frames[i];

      memcpy (frame->scale, p, sizeof (float) * 3);
      p += sizeof (float) * 3;
      memcpy (frame->translate, p, sizeof (float) * 3);
      p += sizeof (float) * 3;
      memcpy (frame->name, p, CLUTTER_MD2_DATA_MAX_FRAME_NAME_LEN + 1);
      p += CLUTTER_MD2_DATA_MAX_FRAME_NAME_LEN + 1;
      frame->name[CLUTTER_MD2_DATA_MAX_FRAME_NAME_LEN] = '\0';

      if (!clutter_md2_data_read (frame->vertices, num_vertices * 4,
				  file, display_name, error))
	return FALSE;

      frame->extents.left = FLT_MAX;
      frame->extents.top = FLT_MAX;
      frame->extents.back = FLT_MAX;
      frame->extents.right = -FLT_MAX;
      frame->extents.bottom = -FLT_MAX;
      frame->extents.front = -FLT_MAX;

      /* Check all of the normal indices and calculate the extents */
      for (p = frame->vertices + num_vertices * 4; p > frame->vertices;)
	{
	  float x, y, z;

	  p -= 4;

	  if (p[3] >= CLUTTER_MD2_NORMS_COUNT)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_DATA_ERROR,
			   CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
			   "'%s' is invalid",
			   display_name);

	      return FALSE;
	    }

	  x = p[0] * frame->scale[0] + frame->translate[0];
	  y = p[1] * frame->scale[1] + frame->translate[1];
	  z = p[2] * frame->scale[2] + frame->translate[2];

	  if (x < frame->extents.left)
	    frame->extents.left = x;
	  if (x > frame->extents.right)
	    frame->extents.right = x;
	  if (y < frame->extents.top)
	    frame->extents.top = y;
	  if (y > frame->extents.bottom)
	    frame->extents.bottom = y;
	  if (z < frame->extents.back)
	    frame->extents.back = z;
	  if (z > frame->extents.front)
	    frame->extents.front = z;
	}

      if (frame->extents.left < priv->extents.left)
	priv->extents.left = frame->extents.left;
      if (frame->extents.right > priv->extents.right)
	priv->extents.right = frame->extents.right;
      if (frame->extents.top < priv->extents.top)
	priv->extents.top = frame->extents.top;
      if (frame->extents.bottom > priv->extents.bottom)
	priv->extents.bottom = frame->extents.bottom;
      if (frame->extents.back < priv->extents.back)
	priv->extents.back = frame->extents.back;
      if (frame->extents.front > priv->extents.front)
	priv->extents.front = frame->extents.front;
    }

  return TRUE;
}

static gboolean
clutter_md2_data_real_add_skin (ClutterMD2Data *data,
				const gchar *filename,
				GError **error)
{
  ClutterMD2DataPrivate *priv;
  GdkPixbuf *pixbuf;
  int bpp, rowstride, alignment = 1;
  guint texture_width, texture_height;
  int image_width, image_height;

  g_return_val_if_fail (CLUTTER_IS_MD2_DATA (data), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  priv = data->priv;

  /* The textures should always be a power of two */
  texture_width = clutter_util_next_p2 (priv->skin_width);
  texture_height = clutter_util_next_p2 (priv->skin_height);

  pixbuf = gdk_pixbuf_new_from_file (filename, error);

  if (pixbuf == NULL)
    return FALSE;

  image_width = gdk_pixbuf_get_width (pixbuf);
  image_height = gdk_pixbuf_get_height (pixbuf);
  bpp = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  /* If the pixmap isn't the same size as the texture then create a
     new pixbuf and set a subregion of it */
  if (image_width != texture_width || image_height != texture_height)
    {
      GdkPixbuf *pixbuf_tmp;

      pixbuf_tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				   gdk_pixbuf_get_has_alpha (pixbuf),
				   gdk_pixbuf_get_bits_per_sample (pixbuf),
				   texture_width, texture_height);

      gdk_pixbuf_copy_area (pixbuf, 0, 0,
			    MIN (texture_width, image_width),
			    MIN (texture_height, image_height),
			    pixbuf_tmp, 0, 0);

      g_object_unref (pixbuf);
      pixbuf = pixbuf_tmp;
      rowstride = gdk_pixbuf_get_rowstride (pixbuf);

      /* If the new pixbuf is bigger than the old one then copy in the
	 pixels at the edges so there won't be artifacts if the
	 texture is linear filtered */
      if (image_height < texture_height)
	{
	  int row;
	  guchar *dst = gdk_pixbuf_get_pixels (pixbuf)
	    + rowstride * image_height;
	  const guchar *src = dst - rowstride;

	  for (row = image_height; row < texture_height; row++)
	    {
	      memcpy (dst, src, image_width * bpp);
	      dst += rowstride;
	    }
	}
      if (image_width < texture_width)
	{
	  int row, col;
	  guchar *dst = gdk_pixbuf_get_pixels (pixbuf) + bpp * image_width;
	  const guchar *src = dst - bpp;

	  for (row = 0; row < texture_height; row++)
	    {
	      for (col = texture_width - image_width; col > 0; col--)
		{
		  memcpy (dst, src, bpp);
		  dst += bpp;
		}
	      dst += rowstride - (texture_width - image_width) * bpp;
	      src += rowstride;
	    }
	}
    }

  while ((rowstride & 1) == 0 && alignment < 8)
    {
      rowstride >>= 1;
      alignment <<= 1;
    }

  if (priv->num_skins >= priv->textures_size)
    {
      if (priv->textures_size == 0)
	priv->textures = g_malloc (++priv->textures_size * sizeof (GLuint));
      else
	priv->textures = g_realloc (priv->textures,
				    (priv->textures_size *= 2)
				    * sizeof (GLuint));
    }
      
  glGenTextures (1, priv->textures + priv->num_skins);
  glBindTexture (GL_TEXTURE_2D, priv->textures[priv->num_skins++]);
#ifdef GL_UNPACK_ROW_LENGTH
  glPixelStorei (GL_UNPACK_ROW_LENGTH,
		 gdk_pixbuf_get_rowstride (pixbuf) / bpp);
#endif
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

  return TRUE;
}

gboolean
clutter_md2_data_add_skin (ClutterMD2Data *data,
			   const gchar *filename,
			   GError **error)
{
  if (clutter_md2_data_real_add_skin (data, filename, error))
    {
      g_object_notify (G_OBJECT (data), "n_skins");

      return TRUE;
    }

  return FALSE;
}

static gboolean
clutter_md2_data_load_skins (ClutterMD2Data *data, FILE *file,
			     const gchar *file_name,
			     const gchar *display_name,
			     guint32 num_skins,
			     guint32 file_offset,
			     GError **error)
{
  ClutterMD2DataPrivate *priv = data->priv;
  int i;

  if (priv->textures)
      glDeleteTextures (priv->num_skins, priv->textures);

  priv->num_skins = 0;

  if (!clutter_md2_data_seek (file, file_offset, display_name, error))
    return FALSE;

  for (i = 0; i < num_skins; i++)
    {
      gchar skin_name[CLUTTER_MD2_DATA_MAX_SKIN_NAME_LEN + 1];
      gchar *dir_name, *full_skin_path;
      gboolean add_ret;

      if (!clutter_md2_data_read (skin_name,
				  CLUTTER_MD2_DATA_MAX_SKIN_NAME_LEN + 1,
				  file, display_name, error))
	return FALSE;

      skin_name[CLUTTER_MD2_DATA_MAX_SKIN_NAME_LEN] = '\0';

      /* Assume the file name of the skin is relative to the directory
	 containing the MD2 file */
      dir_name = g_path_get_dirname (file_name);
      full_skin_path = g_build_filename (dir_name, skin_name, NULL);
      g_free (dir_name);

      add_ret = clutter_md2_data_real_add_skin (data, full_skin_path, error);
      g_free (full_skin_path);
      if (!add_ret)
	return FALSE;
    }

  return TRUE;
}

static void
clutter_md2_data_free_data (ClutterMD2Data *data)
{
  ClutterMD2DataPrivate *priv = data->priv;

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

  priv->num_frames = 0;

  if (priv->textures)
    {
      glDeleteTextures (priv->num_skins, priv->textures);

      g_free (priv->textures);

      priv->textures = NULL;
      priv->textures_size = 0;
    }

  priv->num_skins = 0;
}

gboolean
clutter_md2_data_load (ClutterMD2Data *data,
		       const gchar *filename,
		       GError **error)
{
  gboolean ret = TRUE;
  FILE *file;
  gchar *display_name;
  ClutterMD2DataPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_MD2_DATA (data), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  priv = data->priv;

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
      guint32 header[CLUTTER_MD2_DATA_HEADER_COUNT];

      if (!clutter_md2_data_read (header, sizeof (header), file,
				  display_name, error))
	ret = FALSE;
      else
	{
	  int i;

	  for (i = 0; i < CLUTTER_MD2_DATA_HEADER_COUNT; i++)
	    header[i] = GUINT32_FROM_LE (header[i]);

	  priv->skin_width = header[CLUTTER_MD2_DATA_HEADER_SKIN_WIDTH];
	  priv->skin_height = header[CLUTTER_MD2_DATA_HEADER_SKIN_HEIGHT];

	  if (header[CLUTTER_MD2_DATA_HEADER_MAGIC]
	      != CLUTTER_MD2_DATA_FORMAT_MAGIC)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_DATA_ERROR,
			   CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
			   "'%s' is not an MD2 file",
			   display_name);
			   
	      ret = FALSE;
	    }
	  else if (header[CLUTTER_MD2_DATA_HEADER_VERSION]
		   != CLUTTER_MD2_DATA_FORMAT_VERSION)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_DATA_ERROR,
			   CLUTTER_MD2_DATA_ERROR_BAD_VERSION,
			   "Unsupported MD2 version %i for '%s'",
			   header[CLUTTER_MD2_DATA_HEADER_VERSION],
			   display_name);
			   
	      ret = FALSE;
	    }
	  else if (priv->skin_width > CLUTTER_MD2_DATA_MAX_SKIN_SIZE
		   || priv->skin_height > CLUTTER_MD2_DATA_MAX_SKIN_SIZE)
	    {
	      g_set_error (error,
			   CLUTTER_MD2_DATA_ERROR,
			   CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
			   "'%s' has an invalid skin size",
			   display_name);
	      ret = FALSE;
	    }
	  else if (!clutter_md2_data_load_gl_commands
		   (data, file, display_name,
		    header[CLUTTER_MD2_DATA_HEADER_NUM_VERTICES],
		    header[CLUTTER_MD2_DATA_HEADER_NUM_GL_COMMANDS],
		    header[CLUTTER_MD2_DATA_HEADER_OFFSET_GL_COMMANDS],
		    error))
	    ret = FALSE;
	  else if (!clutter_md2_data_load_frames
		   (data, file, display_name,
		    header[CLUTTER_MD2_DATA_HEADER_NUM_FRAMES],
		    header[CLUTTER_MD2_DATA_HEADER_NUM_VERTICES],
		    header[CLUTTER_MD2_DATA_HEADER_OFFSET_FRAMES],
		    error))
	    ret = FALSE;
	  else if (!clutter_md2_data_load_skins
		   (data, file, filename, display_name,
		    header[CLUTTER_MD2_DATA_HEADER_NUM_SKINS],
		    header[CLUTTER_MD2_DATA_HEADER_OFFSET_SKINS],
		    error))
	    ret = FALSE;
	}

      fclose (file);
    }

  g_free (display_name);

  /* If the loading failed then free all of the data so we don't try
     to draw it */
  if (!ret)
    clutter_md2_data_free_data (data);

  g_signal_emit (data, data_signals[DATA_CHANGED], 0);
  g_object_notify (G_OBJECT (data), "n_skins");
  g_object_notify (G_OBJECT (data), "n_frames");
  g_object_notify (G_OBJECT (data), "extents");

  return ret;
}

static void
clutter_md2_data_finalize (GObject *self)
{
  ClutterMD2Data *data = CLUTTER_MD2_DATA (self);

  g_free (data->priv->vertices);

  clutter_md2_data_free_data (data);

  G_OBJECT_CLASS (clutter_md2_data_parent_class)->finalize (self);
}

static gpointer
clutter_md2_data_extents_copy (gpointer data)
{
  return g_slice_dup (ClutterMD2DataExtents, data);
}

static void
clutter_md2_data_extents_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterMD2DataExtents, data);
}

GType
clutter_md2_data_extents_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type = g_boxed_type_register_static
      (g_intern_static_string ("ClutterMD2DataExtents"),
       clutter_md2_data_extents_copy, clutter_md2_data_extents_free);

  return our_type;
}
