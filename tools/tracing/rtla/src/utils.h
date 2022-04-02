// SPDX-License-Identifier: GPL-2.0
#include <stdint.h>
#include <time.h>

/*
 * '18446744073709551615\0'
 */
#define BUFF_U64_STR_SIZE	24

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

#define __uncontained_container_of(ptr, type, member)({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)) ; })

extern int config_debug;
void debug_msg(const char *fmt, ...);
void err_msg(const char *fmt, ...);

long parse_seconds_duration(char *val);
void get_duration(time_t start_time, char *output, int output_size);

int parse_cpu_list(char *cpu_list, char **monitored_cpus);
long long get_llong_from_str(char *start);

static inline void
update_min(unsigned long long *a, unsigned long long *b)
{
	if (*a > *b)
		*a = *b;
}

static inline void
update_max(unsigned long long *a, unsigned long long *b)
{
	if (*a < *b)
		*a = *b;
}

static inline void
update_sum(unsigned long long *a, unsigned long long *b)
{
	*a += *b;
}

struct sched_attr {
	uint32_t size;
	uint32_t sched_policy;
	uint64_t sched_flags;
	int32_t sched_nice;
	uint32_t sched_priority;
	uint64_t sched_runtime;
	uint64_t sched_deadline;
	uint64_t sched_period;
};

int parse_prio(char *arg, struct sched_attr *sched_param);
int set_comm_sched_attr(const char *comm, struct sched_attr *attr);
