
lib_LTLIBRARIES = librut.la

components_source_h = \
    components/rut-animation-clip.h \
    components/rut-camera.h \
    components/rut-light.h \
    components/rut-mesh.h \
    components/rut-material.h \
    components/rut-diamond.h

source_h = \
    $(components_source_h) \
    mash-data.h \
    mash-data-loader.h \
    mash-ply-loader.h \
    rut-color.h \
    rut-entity.h \
    rut-components.h \
    rut-type.h \
    rut-object.h \
    rut-property.h \
    rut-types.h \
    rut-interfaces.h \
    rut-planes.h \
    rut-timeline.h \
    rut-display-list.h \
    rut.h \
    rut-arcball.h \
    rut-bitmask.h \
    rut-shell.h \
    rut-keysyms.h \
    rut-memory-stack.h \
    rut-global.h \
    rut-util.h \
    rut-text-buffer.h \
    rut-text.h \
    rut-tool.h \
    rut-geometry.h \
    rut-context.h \
    rut-number-slider.h \
    rut-vec3-slider.h \
    rut-paintable.h \
    rut-drop-down.h \
    rut-inspector.h \
    rut-prop-inspector.h \
    rut-closure.h \
    rut-asset.h \
    rut-bevel.h \
    rut-split-view.h \
    rut-stack.h \
    rut-entry.h \
    rut-gaussian-blurrer.h \
    rut-downsampler.h \
    rut-toggle.h \
    rut-dof-effect.h

librut_la_SOURCES = \
    components/rut-animation-clip.c \
    components/rut-camera.c \
    components/rut-light.c \
    components/rut-mesh.c \
    components/rut-material.c \
    components/rut-diamond.c \
    mash-data.c \
    mash-data-loader.c \
    mash-ply-loader.c \
    rply.c \
    rply.h \
    color-table.h \
    rut-color.c \
    rut-entity.c \
    rut-type.c \
    rut-object.c \
    rut-property.c \
    rut-interfaces.c \
    rut.c \
    rut-arcball.c \
    rut-transform-private.h \
    rut-camera-private.h \
    rut-volume.c \
    rut-volume-private.h \
    rut-planes.c \
    rut-shell.c \
    rut-bitmask.h \
    rut-bitmask.c \
    rut-flags.h \
    rut-memory-stack.c \
    rut-list.c \
    rut-list.h \
    rut-util.c \
    rut-timeline.c \
    rut-display-list.c \
    rut-text-buffer.c \
    rut-text.c \
    rut-tool.c \
    rut-geometry.c \
    rut-number-slider.c \
    rut-vec3-slider.c \
    rut-paintable.c \
    rut-drop-down.c \
    rut-inspector.c \
    rut-prop-inspector.c \
    rut-closure.c \
    rut-asset.c \
    rut-bevel.c \
    rut-split-view.c \
    rut-stack.c \
    rut-entry.c \
    rut-gaussian-blurrer.c \
    rut-downsampler.c \
    rut-toggle.c \
    rut-dof-effect.c \
    $(source_h)

if !HAVE_ANDROID
librut_la_SOURCES += \
    rut-sdl-keysyms.c
endif

librut_la_LIBADD = \
    @RIG_DEP_LIBS@ \
    @RIG_EXTRA_LDFLAGS@
librut_la_LDFLAGS = \
    -no-undefined \
    -version-info @RIG_LT_CURRENT@:@RIG_LT_REVISION@:@RIG_LT_AGE@ \
    -export-dynamic \
    -export-symbols-regex "(^rut.*)|(^_rut_bitmask.*)"
librut_la_CFLAGS = \
    @RIG_DEP_CFLAGS@ \
    @RIG_EXTRA_CFLAGS@
librut_la_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_srcdir)/rut \
    -DRIG_DATA_DIR=\""$(datadir)/"\"


rut_headersdir =  $(includedir)/rut
rut_headers_DATA = $(source_h)

rut_components_headersdir =  $(includedir)/rut/components
rut_components_headers_DATA = $(components_source_h)

EXTRA_DIST = rut.pc.in
DISTCLEANFILES = $(pc_files)