/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <assert.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_trace_point.h>

#include "eal_internal_cfg.h"
#include "eal_private.h"
#include "eal_thread.h"
#include "eal_trace.h"

RTE_DEFINE_PER_LCORE(unsigned int, _lcore_id) = LCORE_ID_ANY;
RTE_DEFINE_PER_LCORE(int, _thread_id) = -1;
static RTE_DEFINE_PER_LCORE(unsigned int, _socket_id) =
	(unsigned int)SOCKET_ID_ANY;
static RTE_DEFINE_PER_LCORE(rte_cpuset_t, _cpuset);

unsigned rte_socket_id(void)
{
	return RTE_PER_LCORE(_socket_id);
}

//检查cpusetp中cpu对应的numa,如果cpusetp中包含的cpu有多个
//且分属于不同的numa,则返回的numa为ANY,否则返回cpu对应的numa
static int
eal_cpuset_socket_id(rte_cpuset_t *cpusetp)
{
	unsigned cpu = 0;
	int socket_id = SOCKET_ID_ANY;
	int sid;

	if (cpusetp == NULL)
		return SOCKET_ID_ANY;

	do {
		//遍历检查cpusetp中包含的是那个cpu
		if (!CPU_ISSET(cpu, cpusetp))
			continue;

		//如果未设置值，则使用此cpu对应的numa
		if (socket_id == SOCKET_ID_ANY)
			socket_id = eal_cpu_socket_id(cpu);

		//取当前cpu对应的numa
		sid = eal_cpu_socket_id(cpu);

		//如果cpusetp中包含有多个cpu，且numa不相等，则将socket_id置为any
		//且不再尝试
		if (socket_id != sid) {
			socket_id = SOCKET_ID_ANY;
			break;
		}

	} while (++cpu < CPU_SETSIZE);

	return socket_id;
}

static void
thread_update_affinity(rte_cpuset_t *cpusetp)
{
	unsigned int lcore_id = rte_lcore_id();

	/* store socket_id in TLS for quick access */
	//设置当前线程默认使用的socket_id
	RTE_PER_LCORE(_socket_id) =
		eal_cpuset_socket_id(cpusetp);

	/* store cpuset in TLS for quick access */
	//存储本线程占用的cpuset
	memmove(&RTE_PER_LCORE(_cpuset), cpusetp,
		sizeof(rte_cpuset_t));

	if (lcore_id != (unsigned)LCORE_ID_ANY) {
		/* EAL thread will update lcore_config */
		lcore_config[lcore_id].socket_id = RTE_PER_LCORE(_socket_id);
		memmove(&lcore_config[lcore_id].cpuset, cpusetp,
			sizeof(rte_cpuset_t));
	}
}

int
rte_thread_set_affinity(rte_cpuset_t *cpusetp)
{
	if (pthread_setaffinity_np(pthread_self(), sizeof(rte_cpuset_t),
			cpusetp) != 0) {
		RTE_LOG(ERR, EAL, "pthread_setaffinity_np failed\n");
		return -1;
	}

	thread_update_affinity(cpusetp);
	return 0;
}

void
rte_thread_get_affinity(rte_cpuset_t *cpusetp)
{
	assert(cpusetp);
	memmove(cpusetp, &RTE_PER_LCORE(_cpuset),
		sizeof(rte_cpuset_t));
}

int
eal_thread_dump_affinity(rte_cpuset_t *cpuset, char *str, unsigned int size)
{
	unsigned cpu;
	int ret;
	unsigned int out = 0;

	//收集cpuset中的cpu列表
	for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (!CPU_ISSET(cpu, cpuset))
			continue;

		ret = snprintf(str + out,
			       size - out, "%u,", cpu);
		if (ret < 0 || (unsigned)ret >= size - out) {
			/* string will be truncated */
			ret = -1;
			goto exit;
		}

		out += ret;
	}

	ret = 0;
exit:
	/* remove the last separator */
	if (out > 0)
		str[out - 1] = '\0';

	return ret;
}

int
eal_thread_dump_current_affinity(char *str, unsigned int size)
{
	rte_cpuset_t cpuset;

	rte_thread_get_affinity(&cpuset);
	return eal_thread_dump_affinity(&cpuset, str, size);
}

void
__rte_thread_init(unsigned int lcore_id, rte_cpuset_t *cpuset)
{
	/* set the lcore ID in per-lcore memory area */
	RTE_PER_LCORE(_lcore_id) = lcore_id;

	/* acquire system unique id */
	rte_gettid();

	thread_update_affinity(cpuset);

	__rte_trace_mem_per_thread_alloc();
}

void
__rte_thread_uninit(void)
{
	trace_mem_per_thread_free();

	RTE_PER_LCORE(_lcore_id) = LCORE_ID_ANY;
}

struct rte_thread_ctrl_params {
	void *(*start_routine)(void *);
	void *arg;
	pthread_barrier_t configured;
};

static void *ctrl_thread_init(void *arg)
{
	int ret;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	rte_cpuset_t *cpuset = &internal_conf->ctrl_cpuset;
	struct rte_thread_ctrl_params *params = arg;
	void *(*start_routine)(void *) = params->start_routine;
	void *routine_arg = params->arg;

	__rte_thread_init(rte_lcore_id(), cpuset);

	//等待barrier
	ret = pthread_barrier_wait(&params->configured);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}

	//执行回调
	return start_routine(routine_arg);
}

//创建线程，并配置cpu亲昵性
int
rte_ctrl_thread_create(pthread_t *thread, const char *name,
		const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg)
{
	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	rte_cpuset_t *cpuset = &internal_conf->ctrl_cpuset;
	struct rte_thread_ctrl_params *params;
	int ret;

	params = malloc(sizeof(*params));
	if (!params)
		return -ENOMEM;

	params->start_routine = start_routine;
	params->arg = arg;

	//初始化thread barrier,等待数为1+1
	pthread_barrier_init(&params->configured, NULL, 2);

	ret = pthread_create(thread, attr, ctrl_thread_init, (void *)params);
	if (ret != 0) {
		free(params);
		return -ret;
	}

	//修改线程名称
	if (name != NULL) {
		ret = rte_thread_setname(*thread, name);
		if (ret < 0)
			RTE_LOG(DEBUG, EAL,
				"Cannot set name for ctrl thread\n");
	}

	//设置线程亲昵性
	ret = pthread_setaffinity_np(*thread, sizeof(*cpuset), cpuset);
	if (ret)
		goto fail;

	//知会线程开始跑
	ret = pthread_barrier_wait(&params->configured);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}

	return 0;

fail:
	if (PTHREAD_BARRIER_SERIAL_THREAD ==
	    pthread_barrier_wait(&params->configured)) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}
	pthread_cancel(*thread);
	pthread_join(*thread, NULL);
	return -ret;
}

int
rte_thread_register(void)
{
	unsigned int lcore_id;
	rte_cpuset_t cpuset;

	/* EAL init flushes all lcores, we can't register before. */
	if (eal_get_internal_configuration()->init_complete != 1) {
		RTE_LOG(DEBUG, EAL, "Called %s before EAL init.\n", __func__);
		rte_errno = EINVAL;
		return -1;
	}
	if (!rte_mp_disable()) {
		RTE_LOG(ERR, EAL, "Multiprocess in use, registering non-EAL threads is not supported.\n");
		rte_errno = EINVAL;
		return -1;
	}
	if (pthread_getaffinity_np(pthread_self(), sizeof(cpuset),
			&cpuset) != 0)
		CPU_ZERO(&cpuset);
	lcore_id = eal_lcore_non_eal_allocate();
	if (lcore_id >= RTE_MAX_LCORE)
		lcore_id = LCORE_ID_ANY;
	__rte_thread_init(lcore_id, &cpuset);
	if (lcore_id == LCORE_ID_ANY) {
		rte_errno = ENOMEM;
		return -1;
	}
	RTE_LOG(DEBUG, EAL, "Registered non-EAL thread as lcore %u.\n",
		lcore_id);
	return 0;
}

void
rte_thread_unregister(void)
{
	unsigned int lcore_id = rte_lcore_id();

	if (lcore_id != LCORE_ID_ANY)
		eal_lcore_non_eal_release(lcore_id);
	__rte_thread_uninit();
	if (lcore_id != LCORE_ID_ANY)
		RTE_LOG(DEBUG, EAL, "Unregistered non-EAL thread (was lcore %u).\n",
			lcore_id);
}
