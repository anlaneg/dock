/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _CMDLINE_H_
#define _CMDLINE_H_

#include <rte_common.h>

#include <termios.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>

/**
 * @file
 *
 * Command line API
 */

#ifdef __cplusplus
extern "C" {
#endif

//非常差的库，基本是个垃圾
struct cmdline {
	int s_in;//输入fd
	int s_out;//输出fd
	cmdline_parse_ctx_t *ctx;//支持的命令数组
	struct rdline rdl;
	char prompt[RDLINE_PROMPT_SIZE];//命令行提示符
	struct termios oldterm;//启动前版本的term设备
};

struct cmdline *cmdline_new(cmdline_parse_ctx_t *ctx, const char *prompt, int s_in, int s_out);
void cmdline_set_prompt(struct cmdline *cl, const char *prompt);
void cmdline_free(struct cmdline *cl);
void cmdline_printf(const struct cmdline *cl, const char *fmt, ...)
	__rte_format_printf(2, 3);
int cmdline_in(struct cmdline *cl, const char *buf, int size);
int cmdline_write_char(struct rdline *rdl, char c);

/**
 * This function is nonblocking equivalent of ``cmdline_interact()``. It polls
 * *cl* for one character and interpret it. If return value is *RDLINE_EXITED*
 * it mean that ``cmdline_quit()`` was invoked.
 *
 * @param cl
 *   The command line object.
 *
 * @return
 *   On success return object status - one of *enum rdline_status*.
 *   On error return negative value.
 */
int cmdline_poll(struct cmdline *cl);

void cmdline_interact(struct cmdline *cl);
void cmdline_quit(struct cmdline *cl);

#ifdef __cplusplus
}
#endif

#endif /* _CMDLINE_SOCKET_H_ */
