#! /bin/bash

#
# Copyright (c) 2019 Matthieu Riolo
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


#
# Tests if the installed toolchain is up-to-date
#

show_usage() {
	echo "== Toolchain sanity test =="
	echo
	echo "Will check if the existing toolchain is existing and up-to-date"
	echo "Either the toolchain platform must by passed as first argument"
	echo "or Makefile.config will be read in and the platform provided from"
	echo "there used"
	exit 1
}

if [ -z "$1" ] ; then
	if [ -f Makefile.config ] ; then
		PLATFORM=$(cat Makefile.config | grep -e "^PLATFORM" | cut -f2 -d "=")
	else
		show_usage
	fi
else
	PLATFORM="$1"
fi



MSG=`./tools/toolchain.sh --test-version $PLATFORM`
echo "$MSG"

ERRORS=`echo "$MSG" | grep -e "^- " | wc -l`

if [[ $ERRORS -ne 0 ]] ; then
	echo "The toolchain for $PLATFORM is either not installed or not up-to-date"
	echo "Use 'make toolchain-build PROFILE=$PLATFORM' to build the toolchain"
	exit 1
fi