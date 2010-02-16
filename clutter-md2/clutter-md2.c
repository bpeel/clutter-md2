/*
 * Clutter-MD2.
 *
 * A Clutter actor to render MD2 models
 *
 * Authored By Neil Roberts  <neil@o-hand.com>
 *
 * Copyright (C) 2008 OpenedHand
 * Copyright (C) 2010 Intel Corporation
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
#include <clutter/clutter.h>
#include <string.h>

#include "clutter-md2.h"
#include "clutter-md2-data.h"

#define CLUTTER_MD2_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_MD2, ClutterMD2Private))

#define CLUTTER_MD2_PREFERRED_SIZE 100

G_DEFINE_TYPE (ClutterMD2, clutter_md2, CLUTTER_TYPE_ACTOR);

static void clutter_md2_paint (ClutterActor *self);
static void clutter_md2_dispose (GObject *self);
static void clutter_md2_get_preferred_width (ClutterActor *self,
                                             gfloat        for_height,
                                             gfloat       *min_width_p,
                                             gfloat       *natural_width_p);
static void clutter_md2_get_preferred_height (ClutterActor *self,
                                              gfloat        for_width,
                                              gfloat       *min_height_p,
                                              gfloat       *natural_height_p);
static void clutter_md2_set_property (GObject      *self,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec);
static void clutter_md2_get_property (GObject    *self,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec);

struct _ClutterMD2Private
{
  int current_frame_a, current_frame_b;
  float current_frame_interval;
  int current_skin;

  guint data_changed_handler;

  ClutterMD2Data *data;
};

enum
  {
    PROP_0,

    PROP_DATA,

    PROP_CURRENT_SKIN,
    PROP_CURRENT_FRAME,
    PROP_SUB_FRAME
  };

static void
clutter_md2_class_init (ClutterMD2Class *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  actor_class->paint = clutter_md2_paint;
  actor_class->get_preferred_width = clutter_md2_get_preferred_width;
  actor_class->get_preferred_height = clutter_md2_get_preferred_height;

  object_class->dispose = clutter_md2_dispose;
  object_class->set_property = clutter_md2_set_property;
  object_class->get_property = clutter_md2_get_property;

  g_type_class_add_private (klass, sizeof (ClutterMD2Private));

  pspec = g_param_spec_object ("data", "Data",
                               "The ClutterMD2Data instance that "
                               "will be renderered",
                               CLUTTER_TYPE_MD2_DATA,
                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DATA, pspec);

  pspec = g_param_spec_int ("current_frame", "Current frame",
                            "The frame number that will be rendered",
                            0, G_MAXINT, 0,
                            G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CURRENT_FRAME, pspec);

  pspec = g_param_spec_float ("sub_frame", "Interpolated frame number",
                              "A floating point number that represents two "
                              "frame numbers and the interval between them "
                              "that will be rendered",
                              0.0f, G_MAXFLOAT, 0.0f,
                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SUB_FRAME, pspec);

  pspec = g_param_spec_int ("current_skin", "Current skin",
                            "The skin number that will be rendered",
                            0, G_MAXINT, 0,
                            G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CURRENT_SKIN, pspec);
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

static void
clutter_md2_set_property (GObject      *self,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);

  switch (property_id)
    {
    case PROP_DATA:
      clutter_md2_set_data (md2, g_value_get_object (value));
      break;

    case PROP_CURRENT_SKIN:
      clutter_md2_set_current_skin (md2, g_value_get_int (value));
      break;

    case PROP_CURRENT_FRAME:
      clutter_md2_set_current_frame (md2, g_value_get_int (value));
      break;

    case PROP_SUB_FRAME:
      {
        float sub_frame = g_value_get_float (value);
        int frame_num = (int) sub_frame;

        if ((float) frame_num == sub_frame)
          clutter_md2_set_current_frame (md2, frame_num);
        else
          clutter_md2_set_sub_frame (md2, frame_num, frame_num + 1,
                                     sub_frame - frame_num);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
    }
}

static void
clutter_md2_get_property (GObject    *self,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);

  switch (property_id)
    {
    case PROP_DATA:
      g_value_set_object (value, clutter_md2_get_data (md2));
      break;

    case PROP_CURRENT_SKIN:
      g_value_set_int (value, clutter_md2_get_current_skin (md2));
      break;

    case PROP_CURRENT_FRAME:
      g_value_set_int (value, clutter_md2_get_current_frame (md2));
      break;

    case PROP_SUB_FRAME:
      {
        int frame_a, frame_b;
        float interval;

        clutter_md2_get_sub_frame (md2, &frame_a, &frame_b, &interval);

        if (frame_a == frame_b)
          g_value_set_float (value, (float) frame_a);
        else
          g_value_set_float (value, frame_a + interval);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
    }
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

  clutter_actor_get_allocation_geometry (self, &geom);

  if (priv->data == NULL)
    return;

  clutter_md2_data_render (priv->data,
                           priv->current_frame_a,
                           priv->current_frame_b,
                           priv->current_frame_interval,
                           priv->current_skin,
                           &geom);
}

static void
clutter_md2_get_preferred_width (ClutterActor *self,
                                 gfloat        for_height,
                                 gfloat       *min_width_p,
                                 gfloat       *natural_width_p)
{
  ClutterMD2Private *priv = CLUTTER_MD2 (self)->priv;

  /* There is no minimum size we can render at */
  if (min_width_p)
    *min_width_p = 0;

  if (natural_width_p)
    {
      /* If this is a width-for-height request then return the width
         that would maintain the aspect ratio */
      if (for_height >= 0 && priv->data)
        {
          ClutterMD2DataExtents extents;

          clutter_md2_data_get_extents (priv->data, &extents);

          *natural_width_p = for_height * (extents.right - extents.left)
            / (extents.bottom - extents.top);
        }
      /* Otherwise just return a constant preferred size */
      else
        *natural_width_p = CLUTTER_MD2_PREFERRED_SIZE;
    }
}

static void
clutter_md2_get_preferred_height (ClutterActor *self,
                                  gfloat        for_width,
                                  gfloat       *min_height_p,
                                  gfloat       *natural_height_p)
{
  ClutterMD2Private *priv = CLUTTER_MD2 (self)->priv;

  /* There is no minimum size we can render at */
  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    {
      /* If this is a height-for-width request then return the height
         that would maintain the aspect ratio */
      if (for_width >= 0 && priv->data)
        {
          ClutterMD2DataExtents extents;

          clutter_md2_data_get_extents (priv->data, &extents);

          *natural_height_p = for_width * (extents.bottom - extents.top)
            / (extents.right - extents.left);
        }
      /* Otherwise just return a constant preferred size */
      else
        *natural_height_p = CLUTTER_MD2_PREFERRED_SIZE;
    }
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

  g_object_notify (G_OBJECT (md2), "current_skin");
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

  g_object_freeze_notify (G_OBJECT (md2));
  g_object_notify (G_OBJECT (md2), "current_frame");
  g_object_notify (G_OBJECT (md2), "sub_frame");
  g_object_thaw_notify (G_OBJECT (md2));
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
clutter_md2_get_sub_frame (ClutterMD2 *md2, gint *frame_a, gint *frame_b,
                           gfloat *interval)
{
  g_return_if_fail (CLUTTER_IS_MD2 (md2));

  if (frame_a)
    *frame_a = md2->priv->current_frame_a;
  if (frame_a)
    *frame_b = md2->priv->current_frame_b;
  if (interval)
    *interval = md2->priv->current_frame_a == md2->priv->current_frame_b
      ? 0.0f : md2->priv->current_frame_interval;
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

  g_object_freeze_notify (G_OBJECT (md2));
  g_object_notify (G_OBJECT (md2), "current_frame");
  g_object_notify (G_OBJECT (md2), "sub_frame");
  g_object_thaw_notify (G_OBJECT (md2));
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
clutter_md2_forget_data (ClutterMD2 *md2)
{
  if (md2->priv->data)
    {
      g_signal_handler_disconnect (md2->priv->data,
                                   md2->priv->data_changed_handler);
      g_object_unref (md2->priv->data);
      md2->priv->data = NULL;
    }
}

static void
clutter_md2_dispose (GObject *self)
{
  ClutterMD2 *md2 = CLUTTER_MD2 (self);

  clutter_md2_forget_data (md2);

  md2->priv->current_frame_a = 0;
  md2->priv->current_frame_b = 0;
  md2->priv->current_frame_interval = 0;
  md2->priv->current_skin = 0;

  G_OBJECT_CLASS (clutter_md2_parent_class)->dispose (self);
}

ClutterMD2Data *
clutter_md2_get_data (ClutterMD2 *md2)
{
  g_return_val_if_fail (CLUTTER_IS_MD2 (md2), NULL);

  return md2->priv->data;
}

static void
clutter_md2_on_data_changed (ClutterMD2 *md2)
{
  ClutterMD2Private *priv = md2->priv;
  int num_frames = clutter_md2_get_n_frames (md2);
  int num_skins = clutter_md2_get_n_skins (md2);

  g_object_freeze_notify (G_OBJECT (md2));

  if (priv->current_frame_a >= num_frames
      || priv->current_frame_b >= num_frames)
    {
      priv->current_frame_b = priv->current_frame_a = 0;

      g_object_notify (G_OBJECT (md2), "current_frame");
      g_object_notify (G_OBJECT (md2), "sub_frame");
    }
  if (priv->current_skin >= num_skins)
    {
      priv->current_skin = 0;

      g_object_notify (G_OBJECT (md2), "current_skin");
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (md2));

  g_object_thaw_notify (G_OBJECT (md2));
}

void
clutter_md2_set_data (ClutterMD2 *md2, ClutterMD2Data *data)
{
  g_return_if_fail (CLUTTER_IS_MD2 (md2));
  g_return_if_fail (data == NULL || CLUTTER_IS_MD2_DATA (data));

  if (data)
    g_object_ref_sink (data);

  clutter_md2_forget_data (md2);

  md2->priv->data = data;

  if (data)
    md2->priv->data_changed_handler
      = g_signal_connect_swapped (data, "data-changed",
                                  G_CALLBACK (clutter_md2_on_data_changed),
                                  md2);

  g_object_freeze_notify (G_OBJECT (md2));

  clutter_md2_on_data_changed (md2);

  g_object_notify (G_OBJECT (md2), "data");

  g_object_thaw_notify (G_OBJECT (md2));
}
