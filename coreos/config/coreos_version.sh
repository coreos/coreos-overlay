#!/bin/sh

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# CoreOS version information
#
# This file is usually sourced by other build scripts, but can be run
# directly to see what it would do.

#############################################################################
# SET VERSION NUMBERS
#############################################################################
# Release Build number.
# Increment by 1 for every release build.
# COREOS_BUILD=0000

# Release Branch number.
# Increment by 1 for every release build on a branch.
# Reset to 0 when increasing release build number.
# COREOS_BRANCH=0

# Patch number.
# Increment by 1 in case a non-scheduled branch release build is necessary.
# Reset to 0 when increasing branch number.
# COREOS_PATCH=0

# Source COREOS_* from manifest
VERSION_FILE="${GCLIENT_ROOT:-/mnt/host/source}/.repo/manifests/version.txt"
if [ ! -f "$VERSION_FILE" ]; then
    echo "$VERSION_FILE does not exist!" >&2
    exit 1
fi

source "$VERSION_FILE"
if [ -z "$COREOS_BUILD" ] || \
   [ -z "$COREOS_BRANCH" ] || \
   [ -z "$COREOS_PATCH" ]
then
    echo "$VERSION_FILE is invalid!" >&2
    exit 1
fi

export COREOS_BUILD COREOS_BRANCH COREOS_PATCH
export CHROMEOS_BUILD=${COREOS_BUILD}
export CHROMEOS_BRANCH=${COREOS_BRANCH}
export CHROMEOS_PATCH=${COREOS_PATCH}

# Major version for Chrome.
# TODO(marineam): expunge this one...
export CHROME_BRANCH=26

# Official builds must set COREOS_OFFICIAL=1 or CHROMEOS_OFFICIAL=1.
: ${COREOS_OFFICIAL:=${CHROMEOS_OFFICIAL}}
if [ ${COREOS_OFFICIAL:-0} -ne 1 ]; then
  # For developer builds, overwrite CHROMEOS_VERSION_PATCH with a date string
  # for use by auto-updater.
  export COREOS_PATCH=$(date +%Y_%m_%d_%H%M)
  export CHROMEOS_PATCH=${COREOS_PATCH}
fi

# Version string. Not indentied to appease bash.
export COREOS_VERSION_STRING=\
"${COREOS_BUILD}.${COREOS_BRANCH}"\
".${COREOS_PATCH}"
export CHROMEOS_VERSION_STRING="${COREOS_VERSION_STRING}"

# Set CHROME values (Used for releases) to pass to chromeos-chrome-bin ebuild
# URL to chrome archive
# TODO(marineam): expunge these too...
export CHROME_BASE=
# export CHROME_VERSION from incoming value or NULL and let ebuild default
export CHROME_VERSION="$CHROME_VERSION"

# Print (and remember) version info.
echo "CoreOS version: ${COREOS_VERSION_STRING}"
