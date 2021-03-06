#ifndef _RUT_PROP_INSPECTOR_H_
#define _RUT_PROP_INSPECTOR_H_

#include "rut.h"

extern RutType rut_prop_inspector_type;

typedef struct _RutPropInspector RutPropInspector;

#define RUT_PROP_INSPECTOR(x) ((RutPropInspector *) x)

/* This is called whenever the properties changes */
typedef void
(* RutPropInspectorCallback) (RutProperty *target_property,
                              RutProperty *source_property,
                              void *user_data);

/* This is called whenever the animated state changes */
typedef void
(* RutPropInspectorAnimatedCallback) (RutProperty *property,
                                      CoglBool value,
                                      void *user_data);

RutPropInspector *
rut_prop_inspector_new (RutContext *ctx,
                        RutProperty *property,
                        RutPropInspectorCallback property_changed_cb,
                        RutPropInspectorAnimatedCallback animated_changed_cb,
                        void *user_data);

void
rut_prop_inspector_reload_property (RutPropInspector *inspector);

void
rut_prop_inspector_set_animated (RutPropInspector *inspector,
                                 CoglBool animated);

#endif /* _RUT_PROP_INSPECTOR_H_ */
