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

#ifndef __CLUTTER_MD2_DATA_H__
#define __CLUTTER_MD2_DATA_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_MD2_DATA (clutter_md2_data_get_type ())

#define CLUTTER_MD2_DATA(obj)					\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_MD2_DATA,	\
			       ClutterMD2Data))
#define CLUTTER_MD2_DATA_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_MD2_DATA,	\
			    ClutterMD2DataClass))
#define CLUTTER_IS_MD2_DATA(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_MD2_DATA))
#define CLUTTER_IS_MD2_DATA_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_MD2_DATA))
#define CLUTTER_MD2_DATA_GET_CLASS(obj)				\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_MD2_DATA,	\
			      ClutterMD2DataClass))

typedef enum {
  CLUTTER_MD2_DATA_ERROR_INVALID_FILE,
  CLUTTER_MD2_DATA_ERROR_BAD_VERSION
} ClutterMD2DataError;

#define CLUTTER_MD2_DATA_ERROR (clutter_md2_data_error_quark ())
GQuark clutter_md2_data_error_quark (void);

typedef struct _ClutterMD2Data ClutterMD2Data;
typedef struct _ClutterMD2DataPrivate ClutterMD2DataPrivate;
typedef struct _ClutterMD2DataClass ClutterMD2DataClass;

struct _ClutterMD2Data
{
  GInitiallyUnownedClass parent_instance;

  ClutterMD2DataPrivate *priv;
};

struct _ClutterMD2DataClass
{
  GInitiallyUnownedClass parent_class;
};

GType clutter_md2_data_get_type (void) G_GNUC_CONST;

ClutterMD2Data *clutter_md2_data_new (void);

gboolean clutter_md2_data_load (ClutterMD2Data   *md2,
				const gchar      *filename,
				GError          **error);

gboolean clutter_md2_data_add_skin (ClutterMD2Data  *md2,
				    const gchar     *filename,
				    GError         **error);

gint clutter_md2_data_get_n_skins (ClutterMD2Data *md2);

gint clutter_md2_data_get_n_frames (ClutterMD2Data *md2);

const gchar *clutter_md2_data_get_frame_name (ClutterMD2Data *md2,
					      gint            frame_num);

void clutter_md2_data_render (ClutterMD2Data        *data,
			      gint                   frame_num_a,
			      gint                   frame_num_b,
			      gfloat                 interval,
			      gint                   skin_num,
			      const ClutterGeometry *geom);

G_END_DECLS

#endif /* __CLUTTER_MD2_DATA_H__ */
