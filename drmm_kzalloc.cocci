// SPDX-License-Identifier: GPL-2.0-only
/// Detect allocation with drmm_kzalloc since they internally always add some padding
/// and mark the types with stores to __uncontained_complex_alloc easily parsable
/// by llvm
///
// Confidence: Moderate
// Copyright: (C) 2012 Julia Lawall, INRIA/LIP6.
// Copyright: (C) 2012 Gilles Muller, INRIA/LIP6.
// URL: http://coccinelle.lip6.fr/
// Comments:
// Options: --no-includes --include-headers

virtual context
virtual org
virtual report
virtual patch

@alloc_function@
expression c, E, first_param;
type t;
identifier func =~ "^(drmm_kzalloc|__drmm_encoder_alloc|__drmm_universal_plane_alloc|__drmm_crtc_alloc_with_planes)$";
position p1;
fresh identifier i = "__uncontained_tmp";
@@
(
c = func(first_param, sizeof(t), ...);@p1
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
|
c = func(first_param, sizeof E, ...);@p1
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
)

@add_glob_declaration depends on alloc_function@
@@
#include <...>
+
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@add_glob_declaration2 depends on alloc_function && !add_glob_declaration@
@@
#include "..."
+
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@script:python depends on report@
p1 << alloc_function.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)

@script:python depends on report@
p1 << alloc_function.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)
