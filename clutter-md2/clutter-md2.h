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

#ifndef __CLUTTER_MD2_H__
#define __CLUTTER_MD2_H__

#include <glib-object.h>
#include <clutter/clutter.h>
#include <clutter-md2/clutter-md2-data.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_MD2 (clutter_md2_get_type ())

#define CLUTTER_MD2(obj)                                                \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_MD2, ClutterMD2))
#define CLUTTER_MD2_CLASS(klass)                                        \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_MD2, ClutterMD2Class))
#define CLUTTER_IS_MD2(obj)                                             \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_MD2))
#define CLUTTER_IS_MD2_CLASS(klass)                                     \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_MD2))
#define CLUTTER_MD2_GET_CLASS(obj)                                      \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_MD2, ClutterMD2Class))

typedef struct _ClutterMD2 ClutterMD2;
typedef struct _ClutterMD2Private ClutterMD2Private;
typedef struct _ClutterMD2Class ClutterMD2Class;

struct _ClutterMD2
{
  ClutterActor parent_instance;

  ClutterMD2Private *priv;
};

struct _ClutterMD2Class
{
  ClutterActorClass parent_class;
};

GType clutter_md2_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_md2_new (void);

ClutterMD2Data *clutter_md2_get_data (ClutterMD2 *md2);
void clutter_md2_set_data (ClutterMD2 *md2, ClutterMD2Data *data);

gint clutter_md2_get_n_skins (ClutterMD2 *md2);
gint clutter_md2_get_current_skin (ClutterMD2 *md2);
void clutter_md2_set_current_skin (ClutterMD2 *md2, gint skin_num);

gint clutter_md2_get_n_frames (ClutterMD2 *md2);
gint clutter_md2_get_current_frame (ClutterMD2 *md2);
void clutter_md2_set_current_frame (ClutterMD2 *md2, gint frame_num);
void clutter_md2_set_current_frame_by_name (ClutterMD2 *md2,
                                            const gchar *frame_name);
void clutter_md2_get_sub_frame (ClutterMD2 *md2,
                                gint *frame_a, gint *frame_b,
                                gfloat *interval);
void clutter_md2_set_sub_frame (ClutterMD2 *md2,
                                gint frame_a, gint frame_b,
                                gfloat interval);
const gchar *clutter_md2_get_frame_name (ClutterMD2 *md2, gint frame_num);

G_END_DECLS


#endif /* __CLUTTER_MD2_H__ */
