ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libgpsim_otgw.la
libgpsim_otgw_la_SOURCES = manager.cc opentherm.cc opentherm.h probe.cc probe.h
libgpsim_otgw_la_LDFLAGS = -module -shared -export-dynamic

AM_CXXFLAGS = $(GPSIM_CFLAGS) $(GLIB_CFLAGS)
