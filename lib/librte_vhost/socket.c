/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <rte_log.h>

#include "fd_man.h"
#include "vhost.h"
#include "vhost_user.h"


TAILQ_HEAD(vhost_user_connection_list, vhost_user_connection);

/*
 * Every time rte_vhost_driver_register() is invoked, an associated
 * vhost_user_socket struct will be created.
 */
struct vhost_user_socket {
	struct vhost_user_connection_list conn_list;
	pthread_mutex_t conn_mutex;
	char *path;//server unix socket地址
	int socket_fd;//unix socket对应的fd
	struct sockaddr_un un;//采用path构造的unix地址
	bool is_server;//是否为server
	bool reconnect;//是否开启重连接
	bool dequeue_zero_copy;//是否开启出队zero copy

	/*
	 * The "supported_features" indicates the feature bits the
	 * vhost driver supports. The "features" indicates the feature
	 * bits after the rte_vhost_driver_features_disable/enable().
	 * It is also the final feature bits used for vhost-user
	 * features negotiation.
	 */
	uint64_t supported_features;
	uint64_t features;

	struct vhost_device_ops const *notify_ops;//操作集
};

struct vhost_user_connection {
	struct vhost_user_socket *vsocket;
	int connfd;
	int vid;

	TAILQ_ENTRY(vhost_user_connection) next;
};

#define MAX_VHOST_SOCKET 1024
struct vhost_user {
	struct vhost_user_socket *vsockets[MAX_VHOST_SOCKET];
	struct fdset fdset;
	int vsocket_cnt;
	pthread_mutex_t mutex;
};

#define MAX_VIRTIO_BACKLOG 128

static void vhost_user_server_new_connection(int fd, void *data, int *remove);
static void vhost_user_read_cb(int fd, void *dat, int *remove);
static int create_unix_socket(struct vhost_user_socket *vsocket);
static int vhost_user_start_client(struct vhost_user_socket *vsocket);

static struct vhost_user vhost_user = {
	.fdset = {
		.fd = { [0 ... MAX_FDS - 1] = {-1, NULL, NULL, NULL, 0} },
		.fd_mutex = PTHREAD_MUTEX_INITIALIZER,
		.num = 0
	},
	.vsocket_cnt = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

/* return bytes# of read on success or negative val on failure. */
int
read_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num)
{
	struct iovec iov;
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len  = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	ret = recvmsg(sockfd, &msgh, 0);
	if (ret <= 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "recvmsg failed\n");
		return ret;
	}

	if (msgh.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
		RTE_LOG(ERR, VHOST_CONFIG, "truncted msg\n");
		return -1;
	}

	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
		cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		if ((cmsg->cmsg_level == SOL_SOCKET) &&
			(cmsg->cmsg_type == SCM_RIGHTS)) {
			memcpy(fds, CMSG_DATA(cmsg), fdsize);
			break;
		}
	}

	return ret;
}

int
send_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num)
{

	struct iovec iov;
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;

	if (fds && fd_num > 0) {
		msgh.msg_control = control;
		msgh.msg_controllen = sizeof(control);
		cmsg = CMSG_FIRSTHDR(&msgh);
		cmsg->cmsg_len = CMSG_LEN(fdsize);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		memcpy(CMSG_DATA(cmsg), fds, fdsize);
	} else {
		msgh.msg_control = NULL;
		msgh.msg_controllen = 0;
	}

	do {
		ret = sendmsg(sockfd, &msgh, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,  "sendmsg error\n");
		return ret;
	}

	return ret;
}

//加入连接，注册fd读事件
static void
vhost_user_add_connection(int fd, struct vhost_user_socket *vsocket)
{
	int vid;
	size_t size;
	struct vhost_user_connection *conn;
	int ret;

	conn = malloc(sizeof(*conn));
	if (conn == NULL) {
		close(fd);
		return;
	}

	//申请一个vhost
	vid = vhost_new_device();
	if (vid == -1) {
		close(fd);
		free(conn);
		return;
	}

	size = strnlen(vsocket->path, PATH_MAX);
	vhost_set_ifname(vid, vsocket->path, size);

	//如果vsocket开启了入队zero copy,则设置在dev上
	if (vsocket->dequeue_zero_copy)
		vhost_enable_dequeue_zero_copy(vid);

	RTE_LOG(INFO, VHOST_CONFIG, "new device, handle is %d\n", vid);

	//初始化连接，注册fd进行读取
	conn->connfd = fd;
	conn->vsocket = vsocket;
	conn->vid = vid;
	ret = fdset_add(&vhost_user.fdset, fd, vhost_user_read_cb,
			NULL, conn);
	if (ret < 0) {
		conn->connfd = -1;
		free(conn);
		close(fd);
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to add fd %d into vhost server fdset\n",
			fd);
		return;
	}

	pthread_mutex_lock(&vsocket->conn_mutex);
	TAILQ_INSERT_TAIL(&vsocket->conn_list, conn, next);
	pthread_mutex_unlock(&vsocket->conn_mutex);
}

/* call back when there is new vhost-user connection from client  */
//vhost_user 服务端读取函数，监听新连接
static void
vhost_user_server_new_connection(int fd, void *dat, int *remove __rte_unused)
{
	struct vhost_user_socket *vsocket = dat;

	fd = accept(fd, NULL, NULL);
	if (fd < 0)
		return;

	RTE_LOG(INFO, VHOST_CONFIG, "new vhost user connection is %d\n", fd);
	//收到新的连接，加入读事件
	vhost_user_add_connection(fd, vsocket);
}

//vhost_user客户端读取处理
static void
vhost_user_read_cb(int connfd, void *dat, int *remove)
{
	struct vhost_user_connection *conn = dat;
	struct vhost_user_socket *vsocket = conn->vsocket;
	int ret;

	ret = vhost_user_msg_handler(conn->vid, connfd);
	if (ret < 0) {
		close(connfd);
		*remove = 1;
		vhost_destroy_device(conn->vid);

		pthread_mutex_lock(&vsocket->conn_mutex);
		TAILQ_REMOVE(&vsocket->conn_list, conn, next);
		pthread_mutex_unlock(&vsocket->conn_mutex);

		free(conn);

		if (vsocket->reconnect) {
			create_unix_socket(vsocket);
			vhost_user_start_client(vsocket);
		}
	}
}

//创建vhost user对应的unix socket
static int
create_unix_socket(struct vhost_user_socket *vsocket)
{
	int fd;
	struct sockaddr_un *un = &vsocket->un;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	RTE_LOG(INFO, VHOST_CONFIG, "vhost-user %s: socket created, fd: %d\n",
		vsocket->is_server ? "server" : "client", fd);

	//如果不是server,则将fd置为非阻塞
	if (!vsocket->is_server && fcntl(fd, F_SETFL, O_NONBLOCK)) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"vhost-user: can't set nonblocking mode for socket, fd: "
			"%d (%s)\n", fd, strerror(errno));
		close(fd);
		return -1;
	}

	memset(un, 0, sizeof(*un));
	un->sun_family = AF_UNIX;
	strncpy(un->sun_path, vsocket->path, sizeof(un->sun_path));//使用path进行监听
	un->sun_path[sizeof(un->sun_path) - 1] = '\0';

	vsocket->socket_fd = fd;
	return 0;
}

//启动vhost user server
static int
vhost_user_start_server(struct vhost_user_socket *vsocket)
{
	int ret;
	int fd = vsocket->socket_fd;
	const char *path = vsocket->path;

	//bind到地址un
	ret = bind(fd, (struct sockaddr *)&vsocket->un, sizeof(vsocket->un));
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to bind to %s: %s; remove it and try again\n",
			path, strerror(errno));
		goto err;
	}
	RTE_LOG(INFO, VHOST_CONFIG, "bind to %s\n", path);

	//开始监听地址
	ret = listen(fd, MAX_VIRTIO_BACKLOG);
	if (ret < 0)
		goto err;

	//监听读事件，接入新的client
	ret = fdset_add(&vhost_user.fdset, fd, vhost_user_server_new_connection,
		  NULL, vsocket);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to add listen fd %d to vhost server fdset\n",
			fd);
		goto err;
	}

	return 0;

err:
	close(fd);
	return -1;
}

struct vhost_user_reconnect {
	struct sockaddr_un un;
	int fd;
	struct vhost_user_socket *vsocket;

	TAILQ_ENTRY(vhost_user_reconnect) next;
};

TAILQ_HEAD(vhost_user_reconnect_tailq_list, vhost_user_reconnect);
struct vhost_user_reconnect_list {
	struct vhost_user_reconnect_tailq_list head;
	pthread_mutex_t mutex;
};

static struct vhost_user_reconnect_list reconn_list;//重连链表
static pthread_t reconn_tid;//重连线程id

//连接到un地址
static int
vhost_user_connect_nonblock(int fd, struct sockaddr *un, size_t sz)
{
	int ret, flags;

	//连接到un
	ret = connect(fd, un, sz);
	if (ret < 0 && errno != EISCONN)
		return -1;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"can't get flags for connfd %d\n", fd);
		//fd有问题
		return -2;
	}

	//将socket 置为非阻塞
	if ((flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags & ~O_NONBLOCK)) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"can't disable nonblocking on fd %d\n", fd);
		//fd有问题
		return -2;
	}
	return 0;
}

//周期性处理reconn_list链接，执行连接操作
static void *
vhost_user_client_reconnect(void *arg __rte_unused)
{
	int ret;
	struct vhost_user_reconnect *reconn, *next;

	while (1) {
		pthread_mutex_lock(&reconn_list.mutex);

		/*
		 * An equal implementation of TAILQ_FOREACH_SAFE,
		 * which does not exist on all platforms.
		 */
		for (reconn = TAILQ_FIRST(&reconn_list.head);
		     reconn != NULL; reconn = next) {
			next = TAILQ_NEXT(reconn, next);

			ret = vhost_user_connect_nonblock(reconn->fd,
						(struct sockaddr *)&reconn->un,
						sizeof(reconn->un));
			if (ret == -2) {
				//重连失败，主要是fd的问题，不再尝试
				close(reconn->fd);
				RTE_LOG(ERR, VHOST_CONFIG,
					"reconnection for fd %d failed\n",
					reconn->fd);
				goto remove_fd;
			}
			if (ret == -1)
				//重链失败，后续尝试
				continue;

			//重连成功，加入connect
			RTE_LOG(INFO, VHOST_CONFIG,
				"%s: connected\n", reconn->vsocket->path);
			vhost_user_add_connection(reconn->fd, reconn->vsocket);

			//丢弃重连数据
remove_fd:
			TAILQ_REMOVE(&reconn_list.head, reconn, next);
			free(reconn);
		}

		pthread_mutex_unlock(&reconn_list.mutex);
		sleep(1);//应搞个信号量来处理此情况，而不是简单的sleep(1)
	}

	return NULL;
}

//初始化重连线程
static int
vhost_user_reconnect_init(void)
{
	int ret;

	pthread_mutex_init(&reconn_list.mutex, NULL);
	TAILQ_INIT(&reconn_list.head);

	ret = pthread_create(&reconn_tid, NULL,
			     vhost_user_client_reconnect, NULL);
	if (ret < 0)
		RTE_LOG(ERR, VHOST_CONFIG, "failed to create reconnect thread");

	return ret;
}

//连接到vsocket.un，如果连接成功，则监测，并注册read处理函数
static int
vhost_user_start_client(struct vhost_user_socket *vsocket)
{
	int ret;
	int fd = vsocket->socket_fd;
	const char *path = vsocket->path;
	struct vhost_user_reconnect *reconn;

	ret = vhost_user_connect_nonblock(fd, (struct sockaddr *)&vsocket->un,
					  sizeof(vsocket->un));
	if (ret == 0) {
		//连接成功
		vhost_user_add_connection(fd, vsocket);
		return 0;
	}

	//连接失败
	RTE_LOG(WARNING, VHOST_CONFIG,
		"failed to connect to %s: %s\n",
		path, strerror(errno));

	if (ret == -2 || !vsocket->reconnect) {
		//无重连，直接失败
		close(fd);
		return -1;
	}

	//有重连，申请reconn结构，并挂在reconn-list上
	RTE_LOG(INFO, VHOST_CONFIG, "%s: reconnecting...\n", path);
	reconn = malloc(sizeof(*reconn));
	if (reconn == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to allocate memory for reconnect\n");
		close(fd);
		return -1;
	}
	reconn->un = vsocket->un;
	reconn->fd = fd;
	reconn->vsocket = vsocket;
	pthread_mutex_lock(&reconn_list.mutex);
	TAILQ_INSERT_TAIL(&reconn_list.head, reconn, next);
	pthread_mutex_unlock(&reconn_list.mutex);

	return 0;
}

//通过path找到vsocket
static struct vhost_user_socket *
find_vhost_user_socket(const char *path)
{
	int i;

	for (i = 0; i < vhost_user.vsocket_cnt; i++) {
		struct vhost_user_socket *vsocket = vhost_user.vsockets[i];

		if (!strcmp(vsocket->path, path))
			return vsocket;
	}

	return NULL;
}

int
rte_vhost_driver_disable_features(const char *path, uint64_t features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		vsocket->features &= ~features;
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_enable_features(const char *path, uint64_t features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket) {
		if ((vsocket->supported_features & features) != features) {
			/*
			 * trying to enable features the driver doesn't
			 * support.
			 */
			pthread_mutex_unlock(&vhost_user.mutex);
			return -1;
		}
		vsocket->features |= features;
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_set_features(const char *path, uint64_t features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket) {
		vsocket->supported_features = features;
		vsocket->features = features;
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_get_features(const char *path, uint64_t *features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		*features = vsocket->features;
	pthread_mutex_unlock(&vhost_user.mutex);

	if (!vsocket) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"socket file %s is not registered yet.\n", path);
		return -1;
	} else {
		return 0;
	}
}

/*
 * Register a new vhost-user socket; here we could act as server
 * (the default case), or client (when RTE_VHOST_USER_CLIENT) flag
 * is set.
 */
//vhost-user socket注册
int
rte_vhost_driver_register(const char *path, uint64_t flags)
{
	int ret = -1;
	struct vhost_user_socket *vsocket;

	if (!path)
		return -1;

	pthread_mutex_lock(&vhost_user.mutex);

	//vsocket数量已超限，报错
	if (vhost_user.vsocket_cnt == MAX_VHOST_SOCKET) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"error: the number of vhost sockets reaches maximum\n");
		goto out;
	}

	//申请vsocket对象
	vsocket = malloc(sizeof(struct vhost_user_socket));
	if (!vsocket)
		goto out;
	memset(vsocket, 0, sizeof(struct vhost_user_socket));
	vsocket->path = strdup(path);
	TAILQ_INIT(&vsocket->conn_list);
	pthread_mutex_init(&vsocket->conn_mutex, NULL);
	//是否开启出队zero copy
	vsocket->dequeue_zero_copy = flags & RTE_VHOST_USER_DEQUEUE_ZERO_COPY;

	/*
	 * Set the supported features correctly for the builtin vhost-user
	 * net driver.
	 *
	 * Applications know nothing about features the builtin virtio net
	 * driver (virtio_net.c) supports, thus it's not possible for them
	 * to invoke rte_vhost_driver_set_features(). To workaround it, here
	 * we set it unconditionally. If the application want to implement
	 * another vhost-user driver (say SCSI), it should call the
	 * rte_vhost_driver_set_features(), which will overwrite following
	 * two values.
	 */
	vsocket->supported_features = VIRTIO_NET_SUPPORTED_FEATURES;
	vsocket->features           = VIRTIO_NET_SUPPORTED_FEATURES;

	//指明为vhost user client
	if ((flags & RTE_VHOST_USER_CLIENT) != 0) {
		vsocket->reconnect = !(flags & RTE_VHOST_USER_NO_RECONNECT);
		if (vsocket->reconnect && reconn_tid == 0) {
			if (vhost_user_reconnect_init() < 0) {
				free(vsocket->path);
				free(vsocket);
				goto out;
			}
		}
	} else {
		vsocket->is_server = true;
	}
	ret = create_unix_socket(vsocket);
	if (ret < 0) {
		free(vsocket->path);
		free(vsocket);
		goto out;
	}

	//注册刚创建的vsocket
	vhost_user.vsockets[vhost_user.vsocket_cnt++] = vsocket;

out:
	pthread_mutex_unlock(&vhost_user.mutex);

	return ret;
}

static bool
vhost_user_remove_reconnect(struct vhost_user_socket *vsocket)
{
	int found = false;
	struct vhost_user_reconnect *reconn, *next;

	pthread_mutex_lock(&reconn_list.mutex);

	for (reconn = TAILQ_FIRST(&reconn_list.head);
	     reconn != NULL; reconn = next) {
		next = TAILQ_NEXT(reconn, next);

		if (reconn->vsocket == vsocket) {
			TAILQ_REMOVE(&reconn_list.head, reconn, next);
			close(reconn->fd);
			free(reconn);
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&reconn_list.mutex);
	return found;
}

/**
 * Unregister the specified vhost socket
 */
int
rte_vhost_driver_unregister(const char *path)
{
	int i;
	int count;
	struct vhost_user_connection *conn, *next;

	pthread_mutex_lock(&vhost_user.mutex);

	//先找到path对应的vsocket
	for (i = 0; i < vhost_user.vsocket_cnt; i++) {
		struct vhost_user_socket *vsocket = vhost_user.vsockets[i];

		if (!strcmp(vsocket->path, path)) {
			//要解注册的就是这个vsocket
			if (vsocket->is_server) {
				fdset_del(&vhost_user.fdset, vsocket->socket_fd);
				close(vsocket->socket_fd);
				unlink(path);
			} else if (vsocket->reconnect) {
				vhost_user_remove_reconnect(vsocket);
			}

			pthread_mutex_lock(&vsocket->conn_mutex);
			for (conn = TAILQ_FIRST(&vsocket->conn_list);
			     conn != NULL;
			     conn = next) {
				next = TAILQ_NEXT(conn, next);

				fdset_del(&vhost_user.fdset, conn->connfd);
				RTE_LOG(INFO, VHOST_CONFIG,
					"free connfd = %d for device '%s'\n",
					conn->connfd, path);
				close(conn->connfd);
				vhost_destroy_device(conn->vid);
				TAILQ_REMOVE(&vsocket->conn_list, conn, next);
				free(conn);
			}
			pthread_mutex_unlock(&vsocket->conn_mutex);

			free(vsocket->path);
			free(vsocket);

			count = --vhost_user.vsocket_cnt;
			vhost_user.vsockets[i] = vhost_user.vsockets[count];
			vhost_user.vsockets[count] = NULL;
			pthread_mutex_unlock(&vhost_user.mutex);

			return 0;
		}
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return -1;
}

/*
 * Register ops so that we can add/remove device to data core.
 */
int
rte_vhost_driver_callback_register(const char *path,
	struct vhost_device_ops const * const ops)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		vsocket->notify_ops = ops;
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

//通过path查找vhost设备的操作集
struct vhost_device_ops const *
vhost_driver_callback_get(const char *path)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? vsocket->notify_ops : NULL;
}

//处理此path对应的socket(或监听，或连接到服务器）
int
rte_vhost_driver_start(const char *path)
{
	struct vhost_user_socket *vsocket;
	static pthread_t fdset_tid;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	pthread_mutex_unlock(&vhost_user.mutex);

	if (!vsocket)
		return -1;

	//如果fdset dispatch线程还没有创建，则创建
	if (fdset_tid == 0) {
		//启动线程，处理vhost_user的读写事件
		int ret = pthread_create(&fdset_tid, NULL, fdset_event_dispatch,
				     &vhost_user.fdset);
		if (ret < 0)
			RTE_LOG(ERR, VHOST_CONFIG,
				"failed to create fdset handling thread");
	}

	if (vsocket->is_server)
		//监听地址，接受客户端连接
		return vhost_user_start_server(vsocket);
	else
		//连接服务器，读取服务器输出
		return vhost_user_start_client(vsocket);
}
