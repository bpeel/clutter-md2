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

#ifndef __CLUTTER_BEHAVIOUR_MD2_ANIMATE_H__
#define __CLUTTER_BEHAVIOUR_MD2_ANIMATE_H__

#include <clutter/clutter-alpha.h>
#include <clutter/clutter-behaviour.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE	\
  (clutter_behaviour_md2_animate_get_type ())
#define CLUTTER_BEHAVIOUR_MD2_ANIMATE(obj)				\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE, \
			       ClutterBehaviourMD2Animate))
#define CLUTTER_IS_BEHAVIOUR_MD2_ANIMATE(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE))
#define CLUTTER_BEHAVIOUR_MD2_ANIMATE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE, \
			    ClutterBehaviourMD2AnimateClass))
#define CLUTTER_IS_BEHAVIOUR_MD2_ANIMATE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE))
#define CLUTTER_BEHAVIOUR_MD2_ANIMATE_GET_CLASS(obj)			\
  (G_TYPE_INSTANCE_GET_CLASS ((klass), CLUTTER_TYPE_BEHAVIOUR_MD2_ANIMATE, \
			      ClutterBehaviourMD2AnimateClass))

typedef struct _ClutterBehaviourMD2Animate ClutterBehaviourMD2Animate;
typedef struct _ClutterBehaviourMD2AnimatePrivate
ClutterBehaviourMD2AnimatePrivate;
typedef struct _ClutterBehaviourMD2AnimateClass ClutterBehaviourMD2AnimateClass;

struct _ClutterBehaviourMD2Animate
{
  ClutterBehaviour parent_instance;

  ClutterBehaviourMD2AnimatePrivate *priv;
};

struct _ClutterBehaviourMD2AnimateClass
{
  ClutterBehaviourClass parent_class;
};

GType clutter_behaviour_md2_animate_get_type (void) G_GNUC_CONST;

ClutterBehaviour *clutter_behaviour_md2_animate_new (ClutterAlpha *alpha,
						     gint          frame_start,
						     gint          frame_end);

void clutter_behaviour_md2_animate_get_bounds
(ClutterBehaviourMD2Animate *md2_animate,
 gint                       *frame_start,
 gint                       *frame_end);

void clutter_behaviour_md2_animate_set_bounds
(ClutterBehaviourMD2Animate *md2_animate,
 gint                        frame_start,
 gint                        frame_end);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_MD2_ANIMATE_H__ */
