// SPDX-License-Identifier: GPL-2.0-only
/// Detect allocation that create arrays of structs using functions like alloc_large_system_hash
/// and mark the types with stores to __uncontained_array easily parsable
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

@array_type@
type t;
expression c, xxx;
fresh identifier i = "__uncontained_tmp";
position p1;
@@
c = alloc_large_system_hash(xxx, <+... sizeof(t) ...+>, ...)@p1;
++ {
++ t i;
++ __uncontained_array = (unsigned long)&i;
++ }

@array_var@
expression E, c, xxx;
fresh identifier i = "__uncontained_tmp";
position p1;
@@
c = alloc_large_system_hash(xxx, <+... sizeof E ...+>, ...)@p1;
++ {
++ typeof (E) i;
++ __uncontained_array = (unsigned long)&i;
++ }

@add_glob_declaration depends on array_type || array_var@
@@
#include <...>
+ 
+ #ifndef _UNCONTAINED_ARRAY_H
+ #define _UNCONTAINED_ARRAY_H
+ static volatile unsigned long __uncontained_array;
+ #endif /*_UNCONTAINED_ARRAY_H*/

@add_glob_declaration2 depends on (array_type || array_var) && !add_glob_declaration@
@@
#include "..."
+ 
+ #ifndef _UNCONTAINED_ARRAY_H
+ #define _UNCONTAINED_ARRAY_H
+ static volatile unsigned long __uncontained_array;
+ #endif /*_UNCONTAINED_ARRAY_H*/


@script:python depends on report@
p1 << array_type.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)

@script:python depends on report@
p1 << array_var.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)
