#include <clutter/clutter.h>
#include <clutter-md2/clutter-md2.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  ClutterActor *stage, *md2;
  GError *error = NULL;
  ClutterAlpha *alpha;
  ClutterTimeline *tl;
  ClutterBehaviour *b;

  clutter_init (&argc, &argv);

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <md2file>\n", argv[0]);
      exit (1);
    }

  stage = clutter_stage_get_default ();

  md2 = clutter_md2_new ();
  clutter_actor_set_size (md2, 100, 100);
  clutter_actor_set_position (md2, clutter_actor_get_width (stage) / 2,
			      clutter_actor_get_height (stage) / 2);

  if (!clutter_md2_load (CLUTTER_MD2 (md2), argv[1], &error))
    {
      fprintf (stderr, "%s\n", error->message);
      g_error_free (error);
    }

  tl = clutter_timeline_new (360, 60);
  clutter_timeline_start (tl);
  clutter_timeline_set_loop (tl, TRUE);

  alpha = clutter_alpha_new_full (tl, CLUTTER_ALPHA_RAMP_INC, NULL, NULL);
  
  b = clutter_behaviour_rotate_new (alpha, CLUTTER_Y_AXIS,
				    CLUTTER_ROTATE_CW, 0, 360);
  clutter_behaviour_apply (b, md2);

  clutter_container_add (CLUTTER_CONTAINER (stage), md2, NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
