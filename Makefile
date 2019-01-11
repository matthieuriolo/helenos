#
# Copyright (c) 2006 Martin Decky
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# - The name of the author may not be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Just for this Makefile. Sub-makes will run in parallel if requested.
.NOTPARALLEL:

CSCOPE = cscope
FORMAT = clang-format
CHECK = tools/check.sh
CONFIG = tools/config.py
AUTOTOOL = tools/autotool.py
SANDBOX = autotool

CONFIG_RULES = HelenOS.config

COMMON_MAKEFILE = Makefile.common
COMMON_HEADER = common.h

CONFIG_MAKEFILE = Makefile.config
CONFIG_HEADER = config.h
ERRNO_HEADER = abi/include/abi/errno.h
ERRNO_INPUT = abi/include/abi/errno.in

.PHONY: all
all: kernel uspace export-cross test-xcw
	$(MAKE) -r -C boot PRECHECK=$(PRECHECK)

.PHONY: help
help:
	@echo "Usage of HelenOS build system"
	@echo "------------------------------"
	@echo "The following targets are available:"

	@echo "Configuring"
	@echo "  config"
	@echo "    Displays a text interface for configuring the build-system."
	@echo "    It is recommended to call at first config-default for "
	@echo "    initializing the config files for a specific architecture"
	@echo "  config-default PROFILE=<architecture>"
	@echo "    Configures the build-system with default values for a specific architecture"
	@echo "  config-random"
	@echo "    Configures the build-system randomly. Only used for testing purpose"

	@echo "Building"
	@echo "  all [PROFILE=<architecture>]"
	@echo "    Builds HelenOS and is the default target"
	@echo "  kernel [PROFILE=<architecture>]"
	@echo "    Builds only the kernel"
	@echo "  uspace [PROFILE=<architecture>]"
	@echo "    Builds only the user space"
	@echo "  doxy"
	@echo "    Creates the doxygen documentation"

	@echo "Managing code"
	@echo "  clean"
	@echo "    Deletes all compiled or built files"
	@echo "  distclean"
	@echo "    Same as 'clean' but deletes cached files of the build-system too"
	@echo "  check"
	@echo "    Performs a rudimentary build test for every architecture"
	@echo "  ccheck"
	@echo "    Performs a syntax test on C files. This target should"
	@echo "    always be executed before publishing code to the repository"
	@echo "  ccheck-fix"
	@echo "    Same as the target ccheck but performs automatically some"
	@echo "    correction on the code for known syntax failures"
	@echo "  space"
	@echo "    Removes white spaces at the end of a line"
	@echo "Distribution"
	@echo "  release"
	@echo "    Builds all architectures for realeses"
	@echo "  releasefile [PROFILE=<architecture>]"
	@echo "    Copies the build into the release folder"
	@echo "  export-cross"
	@echo "    Exports the posix library and headers to uspace/export"

.PHONY: common
common: $(COMMON_MAKEFILE) $(COMMON_HEADER) $(CONFIG_MAKEFILE) $(CONFIG_HEADER) $(ERRNO_HEADER)

.PHONY: kernel
kernel: common
	$(MAKE) -r -C kernel PRECHECK=$(PRECHECK)

.PHONY: uspace
uspace: common
	$(MAKE) -r -C uspace PRECHECK=$(PRECHECK)

.PHONY: test-xcw
test-xcw: uspace export-cross
	export PATH=$$PATH:$(abspath tools/xcw/bin) && $(MAKE) -r -C tools/xcw/demo

.PHONY: export-posix
export-posix: common
ifndef EXPORT_DIR
	@echo ERROR: Variable EXPORT_DIR is not defined. && false
else
	$(MAKE) -r -C uspace export EXPORT_DIR=$(abspath $(EXPORT_DIR))
endif

.PHONY: export-cross
export-cross: common
	$(MAKE) -r -C uspace export EXPORT_DIR=$(abspath uspace/export)

.PHONY: precheck
precheck: clean
	$(MAKE) -r all PRECHECK=y

.PHONY: cscope
cscope:
	find abi kernel boot uspace -type f -regex '^.*\.[chsS]$$' | xargs $(CSCOPE) -b -k -u -f$(CSCOPE).out

.PHONY: cscope-parts
cscope-parts:
	find abi -type f -regex '^.*\.[chsS]$$' | xargs $(CSCOPE) -b -k -u -f$(CSCOPE)_abi.out
	find kernel -type f -regex '^.*\.[chsS]$$' | xargs $(CSCOPE) -b -k -u -f$(CSCOPE)_kernel.out
	find boot -type f -regex '^.*\.[chsS]$$' | xargs $(CSCOPE) -b -k -u -f$(CSCOPE)_boot.out
	find uspace -type f -regex '^.*\.[chsS]$$' | xargs $(CSCOPE) -b -k -u -f$(CSCOPE)_uspace.out

.PHONY: format
format:
	find abi kernel boot uspace -type f -regex '^.*\.[ch]$$' | xargs $(FORMAT) -i -sort-includes -style=file

.PHONY: ccheck
ccheck: ccheck-build
	tools/ccheck.sh

.PHONY: ccheck-fix
ccheck-fix: ccheck-build
	tools/ccheck.sh --fix

.PHONY: ccheck-build
ccheck-build:
	cd tools && ./ccheck-build.sh

.PHONY: space
space:
	tools/srepl.sh '[ \t]\+$$' ''

doxy:
	$(MAKE) -r -C doxygen

# Pre-integration build check
.PHONY: check
check: ccheck $(CHECK)
ifdef JOBS
	$(CHECK) -j $(JOBS)
else
	$(CHECK) -j $(shell nproc)
endif

# `sed` pulls a list of "compatibility-only" error codes from `errno.in`,
# the following grep finds instances of those error codes in HelenOS code.
.PHONY: check-errno
check-errno:
	@ ! cat abi/include/abi/errno.in | \
	sed -n -e '1,/COMPAT_START/d' -e 's/__errno_entry(\([A-Z0-9]\+\).*/\\b\1\\b/p' | \
	git grep -n -f - -- ':(exclude)abi' ':(exclude)uspace/lib/posix'

# Autotool (detects compiler features)
.PHONY: autotool
autotool $(COMMON_MAKEFILE) $(COMMON_HEADER): $(CONFIG_MAKEFILE) $(AUTOTOOL)
	$(AUTOTOOL)
	diff -q $(COMMON_HEADER).new $(COMMON_HEADER) 2> /dev/null; if [ $$? -ne 0 ]; then mv -f $(COMMON_HEADER).new $(COMMON_HEADER); fi

# Build-time configuration
.PHONY: config-default
config-default $(CONFIG_MAKEFILE) $(CONFIG_HEADER): $(CONFIG_RULES)
ifeq ($(HANDS_OFF),y)
	$(CONFIG) $< hands-off $(PROFILE)
else
	$(CONFIG) $< default $(PROFILE)
endif

.PHONY: config
config: $(CONFIG_RULES)
	$(CONFIG) $<

.PHONY: config-random
config-random: $(CONFIG_RULES)
	$(CONFIG) $< random

# Release files
.PHONY: releasefile
releasefile: all
	$(MAKE) -r -C release releasefile

.PHONY: release
release:
	$(MAKE) -r -C release release

# Cleaning
.PHONY: distclean
distclean: clean
	rm -f $(CSCOPE).out $(COMMON_MAKEFILE) $(COMMON_HEADER) $(CONFIG_MAKEFILE) $(CONFIG_HEADER) tools/*.pyc tools/checkers/*.pyc release/HelenOS-*

.PHONY: clean
clean:
	rm -fr $(SANDBOX)
	$(MAKE) -r -C kernel clean
	$(MAKE) -r -C uspace clean
	$(MAKE) -r -C boot clean
	$(MAKE) -r -C doxygen clean
	$(MAKE) -r -C tools/xcw/demo clean

$(ERRNO_HEADER): $(ERRNO_INPUT)
	echo '/* Generated file. Edit errno.in instead. */' > $@.new
	sed 's/__errno_entry(\([^,]*\),\([^,]*\),.*/#define \1 __errno_t(\2)/' < $< >> $@.new
	mv $@.new $@

-include Makefile.local
