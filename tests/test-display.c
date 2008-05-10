#include <clutter/clutter.h>
#include <clutter-md2/clutter-md2.h>

int
main (int argc, char **argv)
{
  ClutterActor *stage, *md2;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  md2 = clutter_md2_new ();
  clutter_actor_set_size (md2, 100, 100);

  clutter_container_add (CLUTTER_CONTAINER (stage), md2, NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
