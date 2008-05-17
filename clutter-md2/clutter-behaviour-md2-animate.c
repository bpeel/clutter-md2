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

#include <clutter/clutter-behaviour.h>

#include "clutter-md2.h"
#include "clutter-behaviour-md2-animate.h"

G_DEFINE_TYPE (ClutterBehaviourMD2Animate,
               clutter_behaviour_md2_animate,
               CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourMD2AnimatePrivate
{
  gint frame_start;
  gint frame_end;
};

#define CLUTTER_BEHAVIOUR_MD2_ANIMATE_GET_PRIVATE(obj)			\
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),					\
				CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE,	\
				ClutterBehaviourMD2AnimatePrivate))

enum
{
  PROP_0,

  PROP_FRAME_START,
  PROP_FRAME_END
};

struct ForEachData
{
  int frame_a, frame_b;
  gfloat interval;
};

static void
alpha_notify_foreach (ClutterBehaviour *behaviour,
		      ClutterActor     *actor,
		      gpointer          user_data)
{
  struct ForEachData *data = (struct ForEachData *) user_data;

  if (CLUTTER_IS_MD2 (actor))
    clutter_md2_set_sub_frame (CLUTTER_MD2 (actor),
			       data->frame_a, data->frame_b,
			       data->interval);
}

static void
clutter_behaviour_md2_animate_alpha_notify (ClutterBehaviour *behaviour,
					    guint32           alpha_value)
{
  ClutterBehaviourMD2Animate        *animate_behaviour;
  ClutterBehaviourMD2AnimatePrivate *priv;
  struct ForEachData                 data;

  animate_behaviour = CLUTTER_BEHAVIOUR_MD2_ANIMATE (behaviour);
  priv = animate_behaviour->priv;

  if (priv->frame_start == priv->frame_end)
    {
      data.frame_a = priv->frame_start;
      data.frame_b = priv->frame_end;
      data.interval = 0.0f;
    }
  else
    {
      gint frame_start, frame_end, alpha_multiple;

      frame_start = priv->frame_start;
      frame_end = priv->frame_end;

      if (frame_start > frame_end)
	{
	  gint temp = frame_start;
	  frame_start = frame_end;
	  frame_end = temp;
	  alpha_value = CLUTTER_ALPHA_MAX_ALPHA - alpha_value;
	}

      alpha_multiple = alpha_value * (frame_end - frame_start);

      data.frame_a = alpha_multiple / CLUTTER_ALPHA_MAX_ALPHA
	+ frame_start;

      if (data.frame_a == priv->frame_end)
	data.frame_b = data.frame_a;
      else
	data.frame_b = data.frame_a + 1;

      data.interval = alpha_multiple % CLUTTER_ALPHA_MAX_ALPHA
	/ (float) CLUTTER_ALPHA_MAX_ALPHA;
    }

  clutter_behaviour_actors_foreach (behaviour,
				    alpha_notify_foreach,
				    &data);
}

static void
clutter_behaviour_md2_animate_set_property (GObject      *gobject,
					    guint         prop_id,
					    const GValue *value,
					    GParamSpec   *pspec)
{
  ClutterBehaviourMD2Animate *animate;
  ClutterBehaviourMD2AnimatePrivate *priv;

  animate = CLUTTER_BEHAVIOUR_MD2_ANIMATE (gobject);
  priv = animate->priv;

  switch (prop_id)
    {
    case PROP_FRAME_START:
      priv->frame_start = g_value_get_int (value);
      break;
    case PROP_FRAME_END:
      priv->frame_end = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_md2_animate_get_property (GObject    *gobject,
					    guint       prop_id,
					    GValue     *value,
					    GParamSpec *pspec)
{
  ClutterBehaviourMD2AnimatePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_MD2_ANIMATE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_FRAME_START:
      g_value_set_int (value, priv->frame_start);
      break;
    case PROP_FRAME_END:
      g_value_set_int (value, priv->frame_end);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_md2_animate_class_init (ClutterBehaviourMD2AnimateClass
					  *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behaviour_class = CLUTTER_BEHAVIOUR_CLASS (klass);

  gobject_class->set_property = clutter_behaviour_md2_animate_set_property;
  gobject_class->get_property = clutter_behaviour_md2_animate_get_property;

  behaviour_class->alpha_notify = clutter_behaviour_md2_animate_alpha_notify;

  g_object_class_install_property
    (gobject_class, PROP_FRAME_START,
     g_param_spec_int ("frame-start",
		       "First frame",
		       "First frame",
		       0,
		       G_MAXINT,
		       0,
		       G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property
    (gobject_class, PROP_FRAME_END,
     g_param_spec_int ("frame-end",
		       "Last frame",
		       "Last frame",
		       0,
		       G_MAXINT,
		       0,
		       G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_type_class_add_private (klass, sizeof (ClutterBehaviourMD2AnimatePrivate));
}

static void
clutter_behaviour_md2_animate_init (ClutterBehaviourMD2Animate *animate)
{
  ClutterBehaviourMD2AnimatePrivate *priv;

  animate->priv = priv = CLUTTER_BEHAVIOUR_MD2_ANIMATE_GET_PRIVATE (animate);

  priv->frame_start = 0;
  priv->frame_end = 0;
}

ClutterBehaviour *
clutter_behaviour_md2_animate_new (ClutterAlpha *alpha,
				   gint          frame_start,
				   gint          frame_end)
{
  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE,
                       "alpha", alpha,
                       "frame-start", frame_start,
                       "frame-end", frame_end,
                       NULL);
}

void
clutter_behaviour_md2_animate_get_bounds (ClutterBehaviourMD2Animate *animate,
					  gint                     *frame_start,
					  gint                     *frame_end)
{
  ClutterBehaviourMD2AnimatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_MD2_ANIMATE (animate));

  priv = animate->priv;

  if (frame_start)
    *frame_start = priv->frame_start;

  if (frame_end)
    *frame_end = priv->frame_end;
}

void
clutter_behaviour_md2_animate_set_bounds (ClutterBehaviourMD2Animate *animate,
					  gint                      frame_start,
					  gint                      frame_end)
{
  ClutterBehaviourMD2AnimatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_MD2_ANIMATE (animate));

  priv = animate->priv;

  g_object_ref (animate);
  g_object_freeze_notify (G_OBJECT (animate));

  if (priv->frame_start != frame_start)
    {
      priv->frame_start = frame_start;

      g_object_notify (G_OBJECT (animate), "frame-start");
    }

  if (priv->frame_end != frame_end)
    {
      priv->frame_end = frame_end;

      g_object_notify (G_OBJECT (animate), "frame-end");
    }

  g_object_thaw_notify (G_OBJECT (animate));
  g_object_unref (animate);
}
