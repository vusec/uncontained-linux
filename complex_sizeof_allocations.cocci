// SPDX-License-Identifier: GPL-2.0-only
/// Detect allocation that create custom objects by combining different sizeof()
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
expression c;
identifier func =~ "^(kmalloc|kzalloc|kmalloc_node|kzalloc_node|vmalloc|vzalloc|kvmalloc|kvzalloc|kvmalloc_node|kvzalloc_node|kmem_alloc|kmem_zalloc|vmalloc_node|vzalloc_node)$";
@@
c = func(...);

@alloc_function2@
expression c;
identifier func2 =~ "^(kmem_cache_create)$";
@@
c = func2(...);

@simple_sizeof@
type t;
expression E, c, first_param;
position p;
identifier alloc_function.func;
identifier alloc_function2.func2;
@@
(
c = func(sizeof(t), ...)@p;
|
c = func(sizeof E, ...)@p;
|
c = func2(first_param, sizeof(t), ...)@p;
|
c = func2(first_param, sizeof E, ...)@p;
)

@complex_sizeof_type exists@
type t;
expression c, first_param;
fresh identifier i = "__uncontained_tmp";
position p1 != simple_sizeof.p;
identifier alloc_function.func;
identifier alloc_function2.func2;
@@
(
c = func(<+... sizeof(t) ...+>, ...)@p1;
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
|
c = func2(first_param, <+... sizeof(t) ...+>, ...)@p1;
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
)

@complex_sizeof_var exists@
expression E, c, first_param;
fresh identifier i = "__uncontained_tmp";
position p1 != simple_sizeof.p;
identifier alloc_function.func;
identifier alloc_function2.func2;
@@
(
c = func(<+... sizeof E ...+>, ...)@p1;
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
|
c = func2(first_param, <+... sizeof E ...+>, ...)@p1;
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
)


@add_glob_declaration depends on complex_sizeof_type || complex_sizeof_var@
@@
#include <...>
+
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@add_glob_declaration2 depends on (complex_sizeof_type || complex_sizeof_var) && !add_glob_declaration@
@@
#include "..."
+ 
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@script:python depends on report@
p1 << complex_sizeof_type.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)

@script:python depends on report@
p1 << complex_sizeof_var.p1;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p1[0], msg)
