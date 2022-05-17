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

@simple_sizeof_assignment@
type t;
expression E, c;
position p;
@@
(
c = sizeof(t);@p
|
c = sizeof E;@p
)

@alloc_call_type@
type t;
type T =~ "^(unsigned char|char|unsigned short|short|unsigned int|int|unsigned long|long|size_t|ssize_t)$";
T c;
expression res;
fresh identifier i = "__uncontained_tmp";
identifier alloc_function.func;
position p1 != simple_sizeof_assignment.p;
position p2;
@@
c = <+... sizeof(t) ...+>;@p1
...
res = func(<+... c ...+>, ...);@p2
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }

@alloc_call_var@
expression E;
type T =~ "^(unsigned char|char|unsigned short|short|unsigned int|int|unsigned long|long|size_t|ssize_t)$";
T c;
expression res;
fresh identifier i = "__uncontained_tmp";
identifier alloc_function.func;
position p1 != simple_sizeof_assignment.p;
position p2;
@@
c = <+... sizeof E ...+>;@p1
...
res = func(<+... c ...+>, ...);@p2
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }

@add_glob_declaration depends on alloc_call_var || alloc_call_type@
@@
#include <...>
+ 
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@add_glob_declaration2 depends on (alloc_call_var || alloc_call_type) && !add_glob_declaration@
@@
#include "..."
+ 
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@script:python depends on report@
p2 << alloc_call_var.p2;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p2[0], msg)

@script:python depends on report@
p2 << alloc_call_type.p2;
@@

msg = "Detected custom type allocation"
coccilib.report.print_report(p2[0], msg)