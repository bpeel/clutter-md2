noinst_PROGRAMS = test-display

INCLUDES = -I$(top_srcdir)
LDADD = $(top_builddir)/clutter-md2/libclutter-md2-@CLUTTER_MD2_MAJORMINOR@.la
AM_CFLAGS = $(CLUTTER_MD2_CFLAGS)
AM_LDFLAGS = $(CLUTTER_MD2_LIBS)

test_display_SOURCES     = test-display.c
