// SPDX-License-Identifier: GPL-2.0-only
/// Detect allocation of array of structs
/// and mark the types with stores to __uncontained_kcalloc easily parsable
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

@calloc_function@
expression c;
identifier func =~ "^(kcalloc|kcalloc_node|kmalloc_array_node)$";
@@

c = func(...);

@single_nr@
type t;
expression c;
position p;
identifier calloc_function.func;
@@
c = func(1, ...)@p;

@complex_nr_type@
type t;
expression c, nr;
fresh identifier i = "__uncontained_tmp";
position p1 != single_nr.p;
identifier calloc_function.func;
@@
c = func(nr, <+... sizeof(t) ...+>, ...)@p1;
++ {
++ t i;
++ __uncontained_kcalloc = (unsigned long)&i;
++ }

@complex_nr_var@
expression E, c, nr;
fresh identifier i = "__uncontained_tmp";
position p1 != single_nr.p;
identifier calloc_function.func;
@@
c = func(nr, <+... sizeof E ...+>, ...)@p1;
++ {
++ typeof (E) i;
++ __uncontained_kcalloc = (unsigned long)&i;
++ }

@add_glob_declaration depends on complex_nr_type || complex_nr_var@
@@
#include <...>
+
+ #ifndef _UNCONTAINED_KCALLOC_H
+ #define _UNCONTAINED_KCALLOC_H
+ static volatile unsigned long __uncontained_kcalloc;
+ #endif /*_UNCONTAINED_KCALLOC_H*/

@add_glob_declaration2 depends on (complex_nr_type || complex_nr_var) && !add_glob_declaration@
@@
#include "..."
+
+ #ifndef _UNCONTAINED_KCALLOC_H
+ #define _UNCONTAINED_KCALLOC_H
+ static volatile unsigned long __uncontained_kcalloc;
+ #endif /*_UNCONTAINED_KCALLOC_H*/

@script:python depends on report@
p1 << complex_nr_type.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)

@script:python depends on report@
p1 << complex_nr_var.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)
