INCLUDES =                                              \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/clutter-md2                     \
	-I$(top_builddir)/clutter-md2			\
	-DCLUTTER_MD2_PREFIX=\""$(prefix)"\"		\
	-DCLUTTER_MD2_LIBDIR=\""$(libdir)"\"		\
	-DCLUTTER_MD2_DATADIR=\""$(datadir)"\"		\
	-DCLUTTER_MD2_COMPILATION=1     		\
	-DG_DISABLE_DEPRECATED 				\
	-DG_LOG_DOMAIN=\"Clutter-MD2\" 			\
	$(GCC_FLAGS) 					\
	$(CLUTTER_MD2_CFLAGS)

LDADD =                                                 \
	$(CLUTTER_MD2_LT_LDFLAGS)                       \
	-export-dynamic                                 \
	-export-symbols-regex "^clutter_md2"            \
	-rpath $(libdir)

source_h =					          \
	$(srcdir)/clutter-md2.h                           \
	$(top_builddir)/clutter-md2/clutter-md2-version.h \
	$(srcdir)/clutter-behaviour-md2-animate.h         \
	$(srcdir)/clutter-md2-data.h

source_h_priv =                         \
	clutter-md2-norms.h

source_c =                              \
	clutter-md2.c                   \
	clutter-behaviour-md2-animate.c \
	clutter-md2-norms.c             \
	clutter-md2-data.c

libclutter_md2_@CLUTTER_MD2_API_VERSION@_la_LIBADD = \
  $(CLUTTER_MD2_LIBS)

libclutter_md2_@CLUTTER_MD2_API_VERSION@_la_SOURCES = \
	$(source_c) \
	$(source_h) \
	$(source_h_priv)

libclutter_md2_@CLUTTER_MD2_API_VERSION@_la_LDFLAGS = \
	$(LDADD)

lib_LTLIBRARIES = \
	libclutter-md2-@CLUTTER_MD2_API_VERSION@.la

clutter_md2dir = \
	$(includedir)/clutter-md2-$(CLUTTER_MD2_API_VERSION)/clutter-md2

clutter_md2_HEADERS = \
	$(source_h)

DISTCLEANFILES = \
	clutter-md2-version.h

EXTRA_DIST = \
	clutter-md2-version.h.in
