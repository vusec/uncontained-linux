// SPDX-License-Identifier: GPL-2.0-only
/// Detect structs that are setup to be allocated near `struct request` in `blk_alloc`
/// via the `blk_mq_alloc_tag_set` api by being assigned like:
/// tag_set.cmd_size = sizeof(struct S);
/// Coarse grained script, will have false positives in:
/// - drivers/platform/x86/intel/...
/// - drivers/infiniband/hw/bnxt_re/qplib_rcfw.h
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

@sizeof_assignment_type@
type t;
expression e;
fresh identifier i = "__uncontained_tmp";
@@
(
e.cmd_size = <+... sizeof(t) ...+>;
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
|
e->cmd_size = <+... sizeof(t) ...+>;
++ {
++ t i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
)

@sizeof_assignment_var@
fresh identifier i = "__uncontained_tmp";
expression e;
expression E;
@@
(
e.cmd_size = <+... sizeof E ...+>;
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
|
e->cmd_size = <+... sizeof E ...+>;
++ {
++ typeof (E) i;
++ __uncontained_complex_alloc = (unsigned long)&i;
++ }
)

@add_glob_declaration depends on sizeof_assignment_var || sizeof_assignment_type@
@@
#include <...>
+ 
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/

@add_glob_declaration2 depends on (sizeof_assignment_var || sizeof_assignment_type) && !add_glob_declaration@
@@
#include "..."
+ 
+ #ifndef _UNCONTAINED_COMPLEX_ALLOC_H
+ #define _UNCONTAINED_COMPLEX_ALLOC_H
+ static volatile unsigned long __uncontained_complex_alloc;
+ #endif /*_UNCONTAINED_COMPLEX_ALLOC_H*/