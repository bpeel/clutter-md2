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
#include <GL/gl.h>

#include "clutter-md2.h"

#define CLUTTER_MD2_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_MD2, ClutterMD2Private))

G_DEFINE_TYPE (ClutterMD2, clutter_md2, CLUTTER_TYPE_ACTOR);

static void clutter_md2_paint (ClutterActor *self);

struct _ClutterMD2Private
{
  int stub;
};

static void
clutter_md2_class_init (ClutterMD2Class *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = clutter_md2_paint;

  g_type_class_add_private (klass, sizeof (ClutterMD2Private));
}

static void
clutter_md2_init (ClutterMD2 *self)
{
  ClutterMD2Private *priv;

  self->priv = priv = CLUTTER_MD2_GET_PRIVATE (self);
}

ClutterActor *
clutter_md2_new (void)
{
  return g_object_new (CLUTTER_TYPE_MD2, NULL);
}

void
clutter_md2_paint (ClutterActor *self)
{
  ClutterGeometry geom;

  clutter_actor_get_geometry (self, &geom);

  glPushAttrib (GL_ENABLE_BIT);
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  glBegin (GL_TRIANGLE_STRIP);
  glColor3f (1.0f, 0.0f, 0.0f);
  glVertex2i (0, 0);
  glVertex2i (0, geom.height);
  glVertex2i (geom.width, 0);
  glVertex2i (geom.width, geom.height);
  glEnd ();

  glPopAttrib ();
}
