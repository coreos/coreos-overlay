# Copyright 2016 CoreOS, Inc
# Distributed under the terms of the GNU General Public License v2

EAPI=6

KEYWORDS="amd64"
DESCRIPTION="Binary assets for use in kola tests"
HOMEPAGE="https://github.com/coreos/mantle"
LICENSE="Apache"

SRC_URI="https://get.docker.com/builds/Linux/x86_64/docker-1.9.1.tgz"
SLOT="0"

QA_PREBUILT="usr/lib/kola/amd64/docker-1.9.1"

S="${WORKDIR}"

src_install() {
    exeinto /usr/lib/kola/amd64
    newexe usr/local/bin/docker docker-1.9.1
}
