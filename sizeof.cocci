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
identifier func =~ "^(kmalloc|kzalloc|kcalloc|kmalloc_node|kzalloc_node|vmalloc|vzalloc|kvmalloc|kvzalloc|kvmalloc_node|kvzalloc_node|kmem_alloc|kmem_zalloc|vmalloc_node|vzalloc_node)$";
@@

c = func(...);

@simple_sizeof@
type t;
expression E, c;
position p;
identifier alloc_function.func;
@@
(
c = func(sizeof(t), ...)@p;
|
c = func(sizeof E, ...)@p;
)

@complex_sizeof_type@
type t;
expression c;
fresh identifier i = "__uncontained_tmp";
position p1 != simple_sizeof.p;
identifier alloc_function.func;
@@
c = func(<+... sizeof(t) ...+>, ...)@p1;
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }

@complex_sizeof_var@
expression E, c;
fresh identifier i = "__uncontained_tmp";
position p1 != simple_sizeof.p;
identifier alloc_function.func;
@@
c = func(<+... sizeof E ...+>, ...)@p1;
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }

@add_glob_declaration depends on complex_sizeof_type || complex_sizeof_var@
@@

#include <...>
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
