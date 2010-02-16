#include <clutter/clutter.h>
#include <cogl/cogl-pango.h>
#include <clutter-md2/clutter-md2.h>
#include <clutter-md2/clutter-behaviour-md2-animate.h>
#include <stdlib.h>
#include <string.h>

#define BUTTON_WIDTH   200
#define BUTTON_HEIGHT  30
#define BUTTON_GAP     5
#define GRABBER_WIDTH  15
#define GRABBER_HEIGHT 20

#define ANGLE_CONTROL_WIDTH  40
#define ANGLE_CONTROL_HEIGHT 25

static const char * const angle_labels[]
= { "0", "90", "180", "270", "Spin" };
#define ANGLE_LABEL_COUNT G_N_ELEMENTS (angle_labels)

#define ANGLE_FONT "Sans 10"

typedef struct _CallbackData CallbackData;
typedef struct _DisplayState DisplayState;
typedef struct _GrabberData GrabberData;

struct _DisplayState
{
  ClutterActor *md2;
  ClutterBehaviour *anim;

  int axis_angles[3];
  ClutterActor *angle_markers[3];
  ClutterActor *angle_labels[ANGLE_LABEL_COUNT * 3];

  ClutterActor *stage;
  gboolean is_grabbed;
  ClutterActor *frame_display;
  int grab_x, grab_y;
  int grabber_pos, max_grabber_pos;

  PangoContext *pango_context;
};

static const ClutterColor bg_color = { 5, 139, 192, 255 };

static gint
get_frame_list_start (const DisplayState *data)
{
  int n_frames = clutter_md2_get_n_frames (CLUTTER_MD2 (data->md2));
  int stage_height = clutter_actor_get_height (data->stage);
  int max_start;

  max_start = n_frames * (BUTTON_HEIGHT + BUTTON_GAP) - stage_height;
  if (max_start < 0)
    max_start = 0;

  return data->grabber_pos * max_start / data->max_grabber_pos;
}

static gboolean
on_frame_list_click (ClutterActor *frame_list, ClutterButtonEvent *event,
                     DisplayState *data)
{
  int n_frames = clutter_md2_get_n_frames (CLUTTER_MD2 (data->md2));
  int start = get_frame_list_start (data);
  int ypos, frame_num;
  int frame_start, frame_end;
  ClutterBehaviourMD2Animate *anim_md2
    = CLUTTER_BEHAVIOUR_MD2_ANIMATE (data->anim);
  ClutterAlpha *alpha = clutter_behaviour_get_alpha (data->anim);
  ClutterTimeline *tl = clutter_alpha_get_timeline (alpha);

  ypos = start + event->y;

  if (ypos % (BUTTON_HEIGHT + BUTTON_GAP) >= BUTTON_HEIGHT)
    return FALSE;

  frame_num = ypos / (BUTTON_HEIGHT + BUTTON_GAP);

  if (frame_num < 0 || frame_num >= n_frames)
    return FALSE;

  clutter_behaviour_md2_animate_get_bounds (anim_md2, &frame_start, &frame_end);

  if (event->button == 3)
    frame_end = frame_num;
  else
    {
      if (frame_start == frame_end)
        frame_start = frame_end = frame_num;
      else
        frame_start = frame_num;
    }

  clutter_behaviour_md2_animate_set_bounds (anim_md2, frame_start, frame_end);

  if (frame_start == frame_end)
    clutter_timeline_stop (tl);
  else
    {
      clutter_timeline_set_n_frames (tl, 16 * (ABS (frame_start
                                                    - frame_end) + 1));
      clutter_timeline_rewind (tl);
      clutter_timeline_start (tl);
    }

  clutter_md2_set_current_frame (CLUTTER_MD2 (data->md2), frame_start);

  return TRUE;
}

static void
on_frame_list_paint (ClutterActor *actor, DisplayState *data)
{
  int n_frames = clutter_md2_get_n_frames (CLUTTER_MD2 (data->md2));
  int stage_height = clutter_actor_get_height (data->stage);
  int start, ypos, frame_num;
  PangoFontDescription *font_description;
  CoglColor black;

  cogl_color_set_from_4ub (&black, 0x00, 0x00, 0x00, 0xff);

  start = get_frame_list_start (data);
  frame_num = start / (BUTTON_HEIGHT + BUTTON_GAP);
  ypos = -(start % (BUTTON_HEIGHT + BUTTON_GAP));

  font_description = pango_font_description_new ();
  pango_font_description_set_family (font_description, "Sans 10");

  while (frame_num < n_frames && ypos < stage_height)
    {
      PangoLayout *layout;
      const gchar *frame_name
        = clutter_md2_get_frame_name (CLUTTER_MD2 (data->md2), frame_num);
      PangoRectangle extents;

      cogl_set_source_color4ub (bg_color.red, bg_color.blue, bg_color.blue,
                                bg_color.alpha);
      cogl_rectangle (0, ypos, BUTTON_WIDTH, BUTTON_HEIGHT);

      layout = pango_layout_new (data->pango_context);

      pango_layout_set_text (layout, frame_name, -1);
      pango_layout_set_font_description (layout, font_description);
      pango_layout_get_pixel_extents (layout, NULL, &extents);
      cogl_pango_render_layout (layout,
                                BUTTON_WIDTH / 2 - extents.width / 2,
                                ypos + BUTTON_HEIGHT / 2
                                - extents.height / 2,
                                &black, 0);

      g_object_unref (layout);

      ypos += BUTTON_HEIGHT + BUTTON_GAP;
      frame_num++;
    }

  pango_font_description_free (font_description);
}

static gboolean
on_grabber_button_press (ClutterActor *actor, ClutterButtonEvent *event,
                         DisplayState *data)
{
  if (!data->is_grabbed)
    {
      data->is_grabbed = TRUE;
      clutter_grab_pointer (actor);
      data->grab_x = event->x - clutter_actor_get_x (actor);
      data->grab_y = event->y - clutter_actor_get_y (actor);
    }

  return TRUE;
}

static gboolean
on_grabber_button_release (ClutterActor *actor, ClutterButtonEvent *event,
                           DisplayState *data)
{
  if (data->is_grabbed)
    {
      data->is_grabbed = FALSE;
      clutter_ungrab_pointer ();
    }

  return FALSE;
}

static gboolean
on_grabber_motion (ClutterActor *actor, ClutterMotionEvent *event,
                   DisplayState *data)
{
  if (data->is_grabbed)
    {
      data->grabber_pos = event->y - data->grab_y;

      if (data->grabber_pos < 0)
        data->grabber_pos = 0;
      else if (data->grabber_pos > data->max_grabber_pos)
        data->grabber_pos = data->max_grabber_pos;

      clutter_actor_set_y (actor, data->grabber_pos);

      return TRUE;
    }

  return FALSE;
}

static gboolean
on_key_press (ClutterActor *stage, ClutterKeyEvent *event, gpointer data)
{
  if (event->keyval == CLUTTER_q || event->keyval == CLUTTER_Q
      || event->keyval == CLUTTER_Escape)
    clutter_main_quit ();

  return FALSE;
}

static gboolean
on_angle_button (ClutterActor *button, ClutterButtonEvent *event,
                 DisplayState *state)
{
  int axis, angle;
  int md2_width = clutter_actor_get_width (state->md2);
  int md2_height = clutter_actor_get_height (state->md2);

  for (axis = 0; axis < 3; axis++)
    for (angle = 0; angle < ANGLE_LABEL_COUNT; angle++)
      if (state->angle_labels[axis * ANGLE_LABEL_COUNT + angle] == button)
        {
          state->axis_angles[axis] = angle;
          clutter_actor_set_position (state->angle_markers[axis],
                                      (angle + 1) * ANGLE_CONTROL_WIDTH,
                                      axis * ANGLE_CONTROL_HEIGHT);

          if (angle < ANGLE_LABEL_COUNT - 1)
            clutter_actor_set_rotation (state->md2,
                                        CLUTTER_X_AXIS + axis,
                                        angle * 90.0,
                                        md2_width / 2, md2_height / 2, 0);

          return TRUE;
        }

  return FALSE;
}

static ClutterActor *
make_angle_buttons (DisplayState *state)
{
  ClutterActor *group, *background;
  static const ClutterColor background_color = { 5, 139, 192, 255 };
  static const ClutterColor marker_color = { 0, 0, 255, 255 };
  int axis;

  group = clutter_group_new ();

  /* Make a blue background for the angle buttons */
  background = clutter_rectangle_new_with_color (&background_color);
  clutter_actor_set_size (background,
                          ANGLE_CONTROL_WIDTH * (ANGLE_LABEL_COUNT + 1),
                          ANGLE_CONTROL_HEIGHT * 3);
  clutter_container_add (CLUTTER_CONTAINER (group), background, NULL);

  /* Make a button to the set angle of rotation for each axis */
  for (axis = 0; axis < 3; axis++)
    {
      ClutterActor *label;
      char axis_text[2] = { axis + 'X', 0 };
      int angle;

      label = clutter_text_new_with_text (ANGLE_FONT, axis_text);
      clutter_text_set_line_alignment (CLUTTER_TEXT (label),
                                       PANGO_ALIGN_CENTER);
      clutter_actor_set_position (label, 0, axis * ANGLE_CONTROL_HEIGHT);
      clutter_actor_set_size (label, ANGLE_CONTROL_WIDTH, ANGLE_CONTROL_HEIGHT);

      state->angle_markers[axis]
        = clutter_rectangle_new_with_color (&marker_color);
      clutter_actor_set_position (state->angle_markers[axis],
                                  ANGLE_CONTROL_WIDTH,
                                  ANGLE_CONTROL_HEIGHT * axis);
      clutter_actor_set_size (state->angle_markers[axis],
                              ANGLE_CONTROL_WIDTH,
                              ANGLE_CONTROL_HEIGHT);

      clutter_container_add (CLUTTER_CONTAINER (group),
                             state->angle_markers[axis],
                             label,
                             NULL);

      for (angle = 0; angle < ANGLE_LABEL_COUNT; angle++)
        {
          label = clutter_text_new_with_text (ANGLE_FONT, angle_labels[angle]);
          clutter_text_set_line_alignment (CLUTTER_TEXT (label),
                                           PANGO_ALIGN_CENTER);
          clutter_actor_set_position (label, ANGLE_CONTROL_WIDTH * (angle + 1),
                                      ANGLE_CONTROL_HEIGHT * axis);
          clutter_actor_set_size (label, ANGLE_CONTROL_WIDTH,
                                  ANGLE_CONTROL_HEIGHT);
          clutter_container_add (CLUTTER_CONTAINER (group), label, NULL);

          clutter_actor_set_reactive (label, TRUE);

          g_signal_connect (label, "button-press-event",
                            G_CALLBACK (on_angle_button), state);

          state->angle_labels[axis * ANGLE_LABEL_COUNT + angle] = label;
        }
    }

  return group;
}

static void
on_rotate_frame (ClutterTimeline *tl, gint frame_num, DisplayState *state)
{
  int axis;
  int md2_width = clutter_actor_get_width (state->md2);
  int md2_height = clutter_actor_get_height (state->md2);

  for (axis = 0; axis < 3; axis++)
    if (state->axis_angles[axis] == ANGLE_LABEL_COUNT - 1)
      clutter_actor_set_rotation (state->md2,
                                  CLUTTER_X_AXIS + axis,
                                  frame_num,
                                  md2_width / 2, md2_height / 2, 0);
}

int
main (int argc, char **argv)
{
  ClutterActor *stage, *md2, *grabber, *angle_buttons;
  ClutterMD2Data *data;
  GError *error = NULL;
  ClutterAlpha *alpha;
  ClutterTimeline *tl;
  DisplayState state;
  int i;
  PangoFontMap *font_map;
  static const ClutterColor transparent = { 0, 0, 0, 0 };

  clutter_init (&argc, &argv);

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <md2file> [skin]...\n", argv[0]);
      exit (1);
    }

  stage = clutter_stage_get_default ();

  if (getenv ("FULLSCREEN"))
    clutter_stage_fullscreen (CLUTTER_STAGE (stage));

  clutter_stage_set_color (CLUTTER_STAGE (stage), &transparent);

  md2 = clutter_md2_new ();
  clutter_actor_set_size (md2, clutter_actor_get_width (stage)
                          - GRABBER_WIDTH - BUTTON_WIDTH - BUTTON_GAP * 2,
                          clutter_actor_get_height (stage)
                          - ANGLE_CONTROL_HEIGHT * 3);

  data = clutter_md2_data_new ();

  if (!clutter_md2_data_load (data, argv[1], &error))
    {
      fprintf (stderr, "%s\n", error->message);
      g_error_free (error);
      error = NULL;
    }

  for (i = 2; i < argc; i++)
    if (!clutter_md2_data_add_skin (data, argv[i], &error))
      {
        fprintf (stderr, "%s\n", error->message);
        g_error_free (error);
        error = NULL;
      }

  clutter_md2_set_data (CLUTTER_MD2 (md2), data);

  tl = clutter_timeline_new (360, 60);
  clutter_timeline_start (tl);
  clutter_timeline_set_loop (tl, TRUE);

  g_signal_connect (tl, "new-frame", G_CALLBACK (on_rotate_frame), &state);

  clutter_container_add (CLUTTER_CONTAINER (stage), md2, NULL);

  tl = clutter_timeline_new (1, 60);
  clutter_timeline_set_loop (tl, TRUE);
  alpha = clutter_alpha_new_full (tl, CLUTTER_LINEAR);
  state.anim = clutter_behaviour_md2_animate_new (alpha, 0, 0);
  clutter_behaviour_apply (state.anim, md2);
  state.md2 = md2;

  memset (state.axis_angles, 0, sizeof (int) * 3);

  state.frame_display = clutter_rectangle_new_with_color (&transparent);
  clutter_actor_set_position (state.frame_display,
                              clutter_actor_get_width (stage) - GRABBER_WIDTH
                              - BUTTON_GAP - BUTTON_WIDTH, 0);
  clutter_actor_set_size (state.frame_display, BUTTON_WIDTH,
                          clutter_actor_get_height (stage));
  clutter_actor_set_reactive (state.frame_display, TRUE);

  g_signal_connect (state.frame_display, "button-press-event",
                    G_CALLBACK (on_frame_list_click), &state);
  g_signal_connect (state.frame_display, "paint",
                    G_CALLBACK (on_frame_list_paint), &state);

  grabber = clutter_rectangle_new_with_color (&bg_color);
  clutter_actor_set_size (grabber, GRABBER_WIDTH, GRABBER_HEIGHT);
  clutter_actor_set_position (grabber, clutter_actor_get_width (stage)
                              - GRABBER_WIDTH, 0);
  clutter_actor_set_reactive (grabber, TRUE);

  clutter_container_add (CLUTTER_CONTAINER (stage),
                         state.frame_display, grabber, NULL);

  state.stage = stage;
  state.is_grabbed = FALSE;
  state.grabber_pos = 0;
  state.max_grabber_pos = clutter_actor_get_height (stage) - GRABBER_HEIGHT;

  g_signal_connect (G_OBJECT (grabber), "button-press-event",
                    G_CALLBACK (on_grabber_button_press), &state);
  g_signal_connect (G_OBJECT (grabber), "button-release-event",
                    G_CALLBACK (on_grabber_button_release), &state);
  g_signal_connect (G_OBJECT (grabber), "motion-event",
                    G_CALLBACK (on_grabber_motion), &state);
  g_signal_connect (G_OBJECT (stage), "key-press-event",
                    G_CALLBACK (on_key_press), NULL);

  angle_buttons = make_angle_buttons (&state);
  clutter_actor_set_position (angle_buttons, 0,
                              clutter_actor_get_height (stage)
                              - clutter_actor_get_height (angle_buttons));
  clutter_container_add (CLUTTER_CONTAINER (stage), angle_buttons, NULL);

  font_map = cogl_pango_font_map_new ();
  state.pango_context
    = cogl_pango_font_map_create_context (COGL_PANGO_FONT_MAP (font_map));

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (state.pango_context);
  g_object_unref (font_map);

  return 0;
}
