#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

echo "#ifndef __RTE_CONFIG_H"
echo "#define __RTE_CONFIG_H"
# 采用一连中的sed替换，将CONFIG_XX=y,n,FF格式
# 替换为 c 语言可识别的宏定义
grep CONFIG_ $1 |
grep -v '^[ \t]*#' |
sed 's,CONFIG_\(.*\)=y.*$,#undef \1\
#define \1 1,' |
sed 's,CONFIG_\(.*\)=n.*$,#undef \1,' |
sed 's,CONFIG_\(.*\)=\(.*\)$,#undef \1\
#define \1 \2,' |
sed 's,\# CONFIG_\(.*\) is not set$,#undef \1,'
echo "#endif /* __RTE_CONFIG_H */"
