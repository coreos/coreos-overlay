# Copyright 2019 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit coreos-go-lang

KEYWORDS="-* amd64 arm64"

PATCHES=(
    "${FILESDIR}/${PN}-1.12-revert-url-parsing-change.patch"
)
