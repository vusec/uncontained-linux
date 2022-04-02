#ifndef MISC_H
#define MISC_H

#include "assume.h"
#include "int_typedefs.h"
#include "locks.h"

#include <linux/types.h>

/* Probably won't need to deal with bottom halves. */
static inline void local_bh_disable(void) {}
static inline void local_bh_enable(void) {}

#define MODULE_ALIAS(X)
#define module_param(...)
#define EXPORT_SYMBOL_GPL(x)

// define the globals only once
#ifndef _UNCONTAINED_CONTAINER_OF_H
#define _UNCONTAINED_CONTAINER_OF_H

static volatile unsigned long __container_of_type_in;
static volatile unsigned long __container_of_type_out;
static volatile unsigned long __container_of_ptr_in;
static volatile unsigned long __container_of_ptr_out;
static volatile unsigned long __container_of_ptr_diff;

#endif /* _UNCONTAINED_CONTAINER_OF_H */

// define a wrapper for container_of only if kasan is enabled
#ifdef KASAN_ENABLED
#define container_of(ptr, type, member) ({ \
    typeof(ptr) __tmp_type_in; \
    type* __tmp_ptr_out = __uncontained_container_of(ptr, type, member); \
    __container_of_ptr_in   = (unsigned long)ptr; \
    __container_of_type_in  = (unsigned long)&__tmp_type_in; \
    __container_of_type_out = (unsigned long)&__tmp_ptr_out; \
    __container_of_ptr_out  = (unsigned long) __tmp_ptr_out; \
    __container_of_ptr_diff = (unsigned long) offsetof(type, member); \
    (type*)__container_of_ptr_out;  })
#else
#define container_of(ptr, type, member) ({ \
    __uncontained_container_of(ptr, type, member); })
#endif

#define __uncontained_container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member));	\
})

#ifndef USE_SIMPLE_SYNC_SRCU
/* Abuse udelay to make sure that busy loops terminate. */
#define udelay(x) assume(0)

#else

/* The simple custom synchronize_srcu is ok with try_check_zero failing. */
#define udelay(x) do { } while (0)
#endif

#define trace_rcu_torture_read(rcutorturename, rhp, secs, c_old, c) \
	do { } while (0)

#define notrace

/* Avoid including rcupdate.h */
struct rcu_synchronize {
	struct rcu_head head;
	struct completion completion;
};

void wakeme_after_rcu(struct rcu_head *head);

#define rcu_lock_acquire(a) do { } while (0)
#define rcu_lock_release(a) do { } while (0)
#define rcu_lockdep_assert(c, s) do { } while (0)
#define RCU_LOCKDEP_WARN(c, s) do { } while (0)

/* Let CBMC non-deterministically choose switch between normal and expedited. */
bool rcu_gp_is_normal(void);
bool rcu_gp_is_expedited(void);

/* Do the same for old versions of rcu. */
#define rcu_expedited (rcu_gp_is_expedited())

#endif
