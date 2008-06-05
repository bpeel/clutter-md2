#include <clutter/clutter.h>
#include <clutter-md2/clutter-md2.h>
#include <clutter-md2/clutter-behaviour-md2-animate.h>
#include <stdlib.h>

#define BUTTON_WIDTH   200
#define BUTTON_HEIGHT  30
#define BUTTON_GAP     5
#define GRABBER_WIDTH  15
#define GRABBER_HEIGHT 20

typedef struct _CallbackData CallbackData;
typedef struct _DisplayState DisplayState;
typedef struct _GrabberData GrabberData;

struct _CallbackData
{
  int frame_num;
  DisplayState *state;
};

struct _DisplayState
{
  ClutterActor *md2;
  ClutterBehaviour *anim;
};

struct _GrabberData
{
  ClutterActor *stage;
  gboolean is_grabbed;
  ClutterActor *scroll_group;
  int grab_x, grab_y;
};

static gboolean
on_frame_button_press (ClutterActor *rect, ClutterButtonEvent *event,
		       CallbackData *data)
{
  int frame_start, frame_end;
  ClutterBehaviourMD2Animate *anim_md2
    = CLUTTER_BEHAVIOUR_MD2_ANIMATE (data->state->anim);
  ClutterAlpha *alpha = clutter_behaviour_get_alpha (data->state->anim);
  ClutterTimeline *tl = clutter_alpha_get_timeline (alpha);

  clutter_behaviour_md2_animate_get_bounds (anim_md2, &frame_start, &frame_end);

  if (event->button == 3)
    frame_end = data->frame_num;
  else
    {
      if (frame_start == frame_end)
	frame_start = frame_end = data->frame_num;
      else
	frame_start = data->frame_num;
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

  clutter_md2_set_current_frame (CLUTTER_MD2 (data->state->md2),
				 frame_start);

  return TRUE;
}

static gboolean
on_grabber_button_press (ClutterActor *actor, ClutterButtonEvent *event,
			 GrabberData *data)
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
			   GrabberData *data)
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
		   GrabberData *data)
{
  if (data->is_grabbed)
    {
      int max_grabber_pos
	= clutter_actor_get_height (data->stage) - GRABBER_HEIGHT;
      int grabber_pos = event->y - data->grab_y;
      int min_group_pos = clutter_actor_get_height (data->stage)
	- clutter_actor_get_height (data->scroll_group);

      if (grabber_pos < 0)
	grabber_pos = 0;
      else if (grabber_pos > max_grabber_pos)
	grabber_pos = max_grabber_pos;

      if (min_group_pos > 0)
	min_group_pos = 0;

      clutter_actor_set_y (actor, grabber_pos);

      clutter_actor_set_y (data->scroll_group,
			   grabber_pos * min_group_pos / max_grabber_pos);

      return TRUE;
    }

  return FALSE;
}

int
main (int argc, char **argv)
{
  ClutterActor *stage, *md2, *scroll_group, *grabber;
  ClutterMD2Data *data;
  GError *error = NULL;
  ClutterAlpha *alpha;
  ClutterTimeline *tl;
  ClutterBehaviour *b;
  GrabberData grabber_data;
  DisplayState state;
  int i;
  static const ClutterColor stage_color = { 0, 0, 0, 0 };
  static const ClutterColor bg_color = { 5, 139, 192, 255 };

  clutter_init (&argc, &argv);

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <md2file> [skin]...\n", argv[0]);
      exit (1);
    }

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  md2 = clutter_md2_new ();
  clutter_actor_set_size (md2, clutter_actor_get_width (stage)
			  - GRABBER_WIDTH - BUTTON_WIDTH - BUTTON_GAP * 2,
			  clutter_actor_get_height (stage));

  data = clutter_md2_data_new ();

  if (!clutter_md2_data_load (data, argv[1], &error))
    {
      fprintf (stderr, "%s\n", error->message);
      g_error_free (error);
    }

  for (i = 2; i < argc; i++)
    if (!clutter_md2_data_add_skin (data, argv[i], &error))
      {
	fprintf (stderr, "%s\n", error->message);
	g_error_free (error);
      }

  clutter_md2_set_data (CLUTTER_MD2 (md2), data);

  tl = clutter_timeline_new (360, 60);
  clutter_timeline_start (tl);
  clutter_timeline_set_loop (tl, TRUE);

  alpha = clutter_alpha_new_full (tl, CLUTTER_ALPHA_RAMP_INC, NULL, NULL);
  
  b = clutter_behaviour_rotate_new (alpha, CLUTTER_Y_AXIS,
				    CLUTTER_ROTATE_CW, 0, 360);
  clutter_behaviour_rotate_set_center (CLUTTER_BEHAVIOUR_ROTATE (b),
				       clutter_actor_get_width (md2) / 2,
				       clutter_actor_get_height (md2) / 2,
				       0);
  clutter_behaviour_apply (b, md2);

  clutter_container_add (CLUTTER_CONTAINER (stage), md2, NULL);

  tl = clutter_timeline_new (1, 60);
  clutter_timeline_set_loop (tl, TRUE);
  alpha = clutter_alpha_new_full (tl, CLUTTER_ALPHA_RAMP_INC, NULL, NULL);
  state.anim = clutter_behaviour_md2_animate_new (alpha, 0, 0);
  clutter_behaviour_apply (state.anim, md2);
  state.md2 = md2;

  scroll_group = clutter_group_new ();

  /* Make a button for each frame */
  for (i = 0; i < clutter_md2_get_n_frames (CLUTTER_MD2 (md2)); i++)
    {
      ClutterActor *group, *rect, *label;
      const gchar *frame_name;
      CallbackData *data = g_new (CallbackData, 1);

      data->state = &state;
      data->frame_num = i;

      group = clutter_group_new ();

      rect = clutter_rectangle_new_with_color (&bg_color);
      clutter_actor_set_size (rect, BUTTON_WIDTH, BUTTON_HEIGHT);
      clutter_actor_set_reactive (rect, TRUE);

      frame_name = clutter_md2_get_frame_name (CLUTTER_MD2 (md2), i);
      label = clutter_label_new_with_text ("Sans 10", frame_name);
      clutter_actor_set_position (label, BUTTON_WIDTH / 2
				  - clutter_actor_get_width (label) / 2,
				  BUTTON_HEIGHT / 2
				  - clutter_actor_get_height (label) / 2);

      clutter_actor_set_position (group, 0, i * (BUTTON_HEIGHT + BUTTON_GAP));

      clutter_container_add (CLUTTER_CONTAINER (group), rect, label, NULL);
      clutter_container_add (CLUTTER_CONTAINER (scroll_group), group, NULL);

      g_signal_connect_data (G_OBJECT (rect), "button-press-event",
			     G_CALLBACK (on_frame_button_press), data,
			     (GClosureNotify) g_free, 0);
    }

  clutter_actor_set_position (scroll_group,
			      clutter_actor_get_width (stage) - GRABBER_WIDTH
			      - BUTTON_GAP - BUTTON_WIDTH, 0);

  grabber = clutter_rectangle_new_with_color (&bg_color);
  clutter_actor_set_size (grabber, GRABBER_WIDTH, GRABBER_HEIGHT);
  clutter_actor_set_position (grabber, clutter_actor_get_width (stage)
			      - GRABBER_WIDTH, 0);
  clutter_actor_set_reactive (grabber, TRUE);

  clutter_container_add (CLUTTER_CONTAINER (stage),
			 scroll_group, grabber, NULL);

  grabber_data.stage = stage;
  grabber_data.is_grabbed = FALSE;
  grabber_data.scroll_group = scroll_group;

  g_signal_connect (G_OBJECT (grabber), "button-press-event", 
		    G_CALLBACK (on_grabber_button_press), &grabber_data);
  g_signal_connect (G_OBJECT (grabber), "button-release-event",
		    G_CALLBACK (on_grabber_button_release), &grabber_data);
  g_signal_connect (G_OBJECT (grabber), "motion-event",
		    G_CALLBACK (on_grabber_motion), &grabber_data);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
