/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CONTAINER_OF_H
#define _LINUX_CONTAINER_OF_H

#include <linux/build_bug.h>
#include <linux/err.h>

#define typeof_member(T, m)	typeof(((T*)0)->m)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
// define the globals only once
#ifndef _UNCONTAINED_CONTAINER_OF_H
#define _UNCONTAINED_CONTAINER_OF_H

static volatile unsigned long __container_of_type_in;
static volatile unsigned long __container_of_type_out;
static volatile unsigned long __container_of_ptr_in;
static volatile unsigned long __container_of_ptr_out;

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
    (type*)__container_of_ptr_out;  })
#else
#define container_of(ptr, type, member) ({ \
    __uncontained_container_of(ptr, type, member); })
#endif

#define __uncontained_container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })

/**
 * container_of_safe - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 * If IS_ERR_OR_NULL(ptr), ptr is returned unchanged.
 */
// define the globals only once
#ifndef _UNCONTAINED_CONTAINER_OF_H
#define _UNCONTAINED_CONTAINER_OF_H

static volatile unsigned long __container_of_type_in;
static volatile unsigned long __container_of_type_out;
static volatile unsigned long __container_of_ptr_in;
static volatile unsigned long __container_of_ptr_out;

#endif /* _UNCONTAINED_CONTAINER_OF_H */

// define a wrapper for container_of_safe only if kasan is enabled
#ifdef KASAN_ENABLED
#define container_of_safe(ptr, type, member) ({ \
    typeof(ptr) __tmp_type_in; \
    type* __tmp_ptr_out = __uncontained_container_of_safe(ptr, type, member); \
    __container_of_ptr_in   = (unsigned long)ptr; \
    __container_of_type_in  = (unsigned long)&__tmp_type_in; \
    __container_of_type_out = (unsigned long)&__tmp_ptr_out; \
    __container_of_ptr_out  = (unsigned long) __tmp_ptr_out; \
    (type*)__container_of_ptr_out;  })
#else
#define container_of_safe(ptr, type, member) ({ \
    __uncontained_container_of_safe(ptr, type, member); })
#endif

#define __uncontained_container_of_safe(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of_safe()");	\
	IS_ERR_OR_NULL(__mptr) ? ERR_CAST(__mptr) :			\
		((type *)(__mptr - offsetof(type, member))); })

#endif	/* _LINUX_CONTAINER_OF_H */
