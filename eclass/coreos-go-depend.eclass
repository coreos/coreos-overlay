# Copyright 2016 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: coreos-go-depend.eclass
# @BLURB: Minimal Go eclass for simply depending on Go.

# @ECLASS-VARIABLE: COREOS_GO_VERSION
# @DESCRIPTION:
# This variable specifies the version of Go to use. If ommitted the
# default value below will be used.
#
# Example:
# @CODE
# COREOS_GO_VERSION=go1.11
# @CODE
export COREOS_GO_VERSION="${COREOS_GO_VERSION:-go1.11}"

case "${EAPI:-0}" in
	5|6) ;;
	*) die "Unsupported EAPI=${EAPI} for ${ECLASS}"
esac

inherit coreos-go-utils

# Set a use flag to indicate what version of Go is being used ensure
# the package gets rebuilt when the version changes.
IUSE="+go_version_${COREOS_GO_VERSION//./_}"
REQUIRED_USE="go_version_${COREOS_GO_VERSION//./_}"
DEPEND="dev-lang/go:${COREOS_GO_VERSION#go}="
