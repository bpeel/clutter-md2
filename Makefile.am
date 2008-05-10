SUBDIRS = clutter-md2

pcfiles = \
	clutter-md2-$(CLUTTER_MD2_API_VERSION).pc

# clutter-md2-<major>.<minor>.pc - for generic dependencies
clutter-md2-$(CLUTTER_MD2_API_VERSION).pc: clutter-md2.pc
	@cp $< $@

pkgconfig_DATA = $(pcfiles)
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = clutter-md2.pc.in

CLEANFILES = $(pcfiles)
