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
#include <clutter/clutter-actor.h>
#include <string.h>

#include "clutter-md2.h"
#include "clutter-md2-data.h"

#define CLUTTER_MD2_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_MD2, ClutterMD2Private))

G_DEFINE_TYPE (ClutterMD2, clutter_md2, CLUTTER_TYPE_ACTOR);

static void clutter_md2_paint (ClutterActor *self);
static void clutter_md2_dispose (GObject *self);

struct _ClutterMD2Private
{
  int current_frame_a, current_frame_b;
  float current_frame_interval;
  int current_skin;
  
  ClutterMD2Data *data;
};

static void
clutter_md2_class_init (ClutterMD2Class *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  actor_class->paint = clutter_md2_paint;

  object_class->dispose = clutter_md2_dispose;

  g_type_class_add_private (klass, sizeof (ClutterMD2Private));
}

static void
clutter_md2_init (ClutterMD2 *self)
{
  ClutterMD2Private *priv;

  self->priv = priv = CLUTTER_MD2_GET_PRIVATE (self);

  priv->current_frame_a = 0;
  priv->current_frame_b = 0;
  priv->current_skin = 0;
  priv->data = NULL;
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
  
  clutter_actor_get_geometry (self, &geom);

  if (priv->data == NULL)
    return;

  clutter_md2_data_render (priv->data,
			   priv->current_frame_a,
			   priv->current_frame_b,
			   priv->current_frame_interval,
			   priv->current_skin,
			   &geom);
}

gint
clutter_md2_get_n_skins (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), 0);

  if (md2->priv->data == NULL)
    return 0;
  else
    return clutter_md2_data_get_n_skins (md2->priv->data);
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
  g_return_if_fail (skin_num >= 0 && skin_num < clutter_md2_get_n_skins (md2));

  md2->priv->current_skin = skin_num;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));
}

gint
clutter_md2_get_n_frames (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), 0);

  if (md2->priv->data == NULL)
    return 0;
  else
    return clutter_md2_data_get_n_frames (md2->priv->data);
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
  g_return_if_fail (frame_num >= 0
		    && frame_num < clutter_md2_get_n_frames (md2));

  md2->priv->current_frame_a = frame_num;
  md2->priv->current_frame_b = frame_num;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));
}

void
clutter_md2_set_current_frame_by_name (ClutterMD2 *md2, const gchar *frame_name)
{
  ClutterMD2Private *priv;
  int i, num_frames;

  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (frame_name != NULL);

  priv = md2->priv;

  if (priv->data == NULL)
    return;

  num_frames = clutter_md2_data_get_n_frames (priv->data);

  for (i = 0; i <num_frames; i++)
    if (!strcmp (clutter_md2_data_get_frame_name (priv->data, i), frame_name))
      {
	clutter_md2_set_current_frame (md2, i);
	break;
      }
}

void
clutter_md2_set_sub_frame (ClutterMD2 *md2, gint frame_a, gint frame_b,
			   gfloat interval)
{
  int num_frames;

  g_return_if_fail (CLUTTER_IS_MD2 (md2));

  num_frames = clutter_md2_get_n_frames (md2);

  g_return_if_fail (frame_a >= 0 && frame_a < num_frames);
  g_return_if_fail (frame_b >= 0 && frame_b < num_frames);

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
			&& frame_num < clutter_md2_get_n_frames (md2),
			NULL);

  return clutter_md2_data_get_frame_name (md2->priv->data, frame_num);
}

static void
clutter_md2_dispose (GObject *self)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);

  if (md2->priv->data)
    {
      g_object_unref (md2->priv->data);
      md2->priv->data = NULL;
    }

  md2->priv->current_frame_a = 0;
  md2->priv->current_frame_b = 0;
  md2->priv->current_frame_interval = 0;
  md2->priv->current_skin = 0;
}

ClutterMD2Data *
clutter_md2_get_data (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), NULL);

  return md2->priv->data;
}

void
clutter_md2_set_data (ClutterMD2 *md2, ClutterMD2Data *data)
{
  ClutterMD2Data *old_data;

  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (data == NULL || CLUTTER_IS_MD2_DATA (data));

  old_data = md2->priv->data;

  md2->priv->data = data;
  if (data)
    g_object_ref_sink (data);

  if (old_data)
    g_object_unref (old_data);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (md2));
}
