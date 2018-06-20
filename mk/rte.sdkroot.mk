# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

MAKEFLAGS += --no-print-directory

# define Q to '@' or not. $(Q) is used to prefix all shell commands to
# be executed silently.
#如果命令行指定了V,则显示执行的命令
Q=@
ifeq '$V' '0'
override V=
endif
ifdef V
ifeq ("$(origin V)", "command line")
Q=
endif
endif
export Q

#如果RTE_SDK未定义，则是报错
ifeq ($(RTE_SDK),)
$(error RTE_SDK is not defined)
endif

RTE_SRCDIR = $(CURDIR)
export RTE_SRCDIR

BUILDING_RTE_SDK := 1
export BUILDING_RTE_SDK

#
# We can specify the configuration template when doing the "make
# config". For instance: make config T=x86_64-native-linuxapp-gcc
#
# 如果命令行执行T,则对应到配置模板上。
RTE_CONFIG_TEMPLATE :=
ifdef T
ifeq ("$(origin T)", "command line")
RTE_CONFIG_TEMPLATE := $(RTE_SRCDIR)/config/defconfig_$(T)
endif
endif
export RTE_CONFIG_TEMPLATE

#
# Default output is $(RTE_SRCDIR)/build
# output files wil go in a separate directory
#
# 如果命令行指定了o,则指定输出，否则按默认输出到$(RTE_SRCDIR)/build
ifdef O
ifeq ("$(origin O)", "command line")
RTE_OUTPUT := $(abspath $(O))
endif
endif
RTE_OUTPUT ?= $(RTE_SRCDIR)/build
export RTE_OUTPUT

# the directory where intermediate build files are stored, like *.o,
# *.d, *.cmd, ...
BUILDDIR = $(RTE_OUTPUT)/build
export BUILDDIR

export ROOTDIRS-y ROOTDIRS- ROOTDIRS-n

#定义首目标default,其依赖于all
.PHONY: default
default: all

#
# 下面的代码对目标进行分类
#

#配置版本类信息显示相关
.PHONY: config defconfig showconfigs showversion showversionum
config defconfig showconfigs showversion showversionum:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkconfig.mk $@

#代码阅读器相关
.PHONY: cscope gtags tags etags
cscope gtags tags etags:
	$(Q)$(RTE_SDK)/devtools/build-tags.sh $@ $T

#测试相关
.PHONY: test test-basic test-fast test-ring test-mempool test-perf coverage
test test-basic test-fast test-ring test-mempool test-perf coverage:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdktest.mk $@

test: test-build

#安装相关
.PHONY: install
install:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkinstall.mk pre_install
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkinstall.mk $@
install-%:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkinstall.mk $@

#文档相关
.PHONY: doc help
doc: doc-all
help: doc-help
doc-%:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkdoc.mk $*

# 覆盖率相关
.PHONY: gcov gcovclean
gcov gcovclean:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkgcov.mk $@

#示例代码相关
.PHONY: examples examples_clean
examples examples_clean:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkexamples.mk $@

# all other build targets
# 其它类目标,目标"all"将走此流程
%:
	#执行checkconfig，确保config.h生成，且已配置
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkconfig.mk checkconfig
	#执行相应目标$@
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkbuild.mk $@
