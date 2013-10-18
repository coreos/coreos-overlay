# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Docker complements LXC with a high-level API which operates at the process level."
HOMEPAGE="http://www.docker.io/"
SRC_URI=""

EGIT_REPO_URI="git://github.com/dotcloud/docker.git"
if [[ ${PV} == *9999 ]]; then
	KEYWORDS=""
else
	EGIT_COMMIT="v${PV}"
	KEYWORDS="~amd64"
fi

inherit bash-completion-r1 git-2 linux-info systemd user

LICENSE="Apache-2.0"
SLOT="0"
IUSE="doc vim-syntax"

DEPEND="
	>=dev-lang/go-1.1.2
	dev-vcs/git
	dev-vcs/mercurial
	doc? (
		dev-python/sphinx
		dev-python/sphinxcontrib-httpdomain
	)
"
RDEPEND="
	!app-emulation/lxc-docker-bin
	>=app-arch/tar-1.26
	>=sys-apps/iproute2-3.5
	>=net-firewall/iptables-1.4
	>=app-emulation/lxc-0.8
	>=dev-vcs/git-1.7
	>=app-arch/xz-utils-4.9
	|| (
		sys-fs/aufs3
		sys-kernel/aufs-sources
	)
"

RESTRICT="strip"

# TODO AUFS will be replaced with device-mapper (sys-fs/lvm2[static-libs]) in 0.7
ERROR_AUFS_FS="AUFS_FS is required to be set if and only if aufs-sources are used"

ERROR_MEMCG_SWAP="MEMCG_SWAP is required if you wish to limit swap usage of containers"

pkg_setup() {
	CONFIG_CHECK+=" ~AUFS_FS ~BRIDGE ~MEMCG_SWAP ~NETFILTER_XT_MATCH_ADDRTYPE ~NF_NAT ~NF_NAT_NEEDED"
	check_extra_config
}

src_unpack() {
	git-2_src_unpack
}

src_compile() {
	export CGO_ENABLED=0 # we need static linking!

	export GOPATH="${WORKDIR}/gopath"
	mkdir -p "$GOPATH" || die

	# copy GOROOT so we can build it without cgo and not modify anything in the REAL_GOROOT
	REAL_GOROOT="$(go env GOROOT)"
	export GOROOT="${WORKDIR}/goroot"
	rm -rf "$GOROOT" || die
	cp -R "$REAL_GOROOT" "$GOROOT" || die

	# recompile GOROOT to be cgo-less and thus static-able (especially net package)
	go install -a -v std || die

	# make sure docker itself is in our shiny new GOPATH
	mkdir -p "${GOPATH}/src/github.com/dotcloud" || die
	ln -sf "$(pwd -P)" "${GOPATH}/src/github.com/dotcloud/docker" || die

	# we need our vendored deps, too
	export GOPATH="$GOPATH:$(pwd -P)/vendor"

	# time to build!
	./hack/make.sh binary || die

	# now copy the binary to a consistent location that doesn't involve the current version number
	mkdir -p bin || die
	VERSION=$(cat ./VERSION)
	cp -v bundles/$VERSION/binary/docker-$VERSION bin/docker || die

	if use doc; then
		emake -C docs docs man || die
	fi
}

src_install() {
	dobin bin/docker
	dodoc AUTHORS CONTRIBUTING.md CHANGELOG.md MAINTAINERS NOTICE README.md

	newinitd "${FILESDIR}/docker-r3.initd" docker
	newconfd "${FILESDIR}/docker-r3.confd" docker

	systemd_dounit "${FILESDIR}/docker.service"

	insinto /usr/share/${P}/contrib
	doins contrib/README
	cp -R "${S}/contrib"/* "${D}/usr/share/${P}/contrib/"

	if use doc; then
		dohtml -r docs/_build/html/*
		doman docs/_build/man/*
	fi

	dobashcomp contrib/completion/bash/*

	insinto /usr/share/zsh/site-functions
	doins contrib/completion/zsh/*

	if use vim-syntax; then
		insinto /usr/share/vim/vimfiles
		doins -r contrib/vim-syntax/ftdetect
		doins -r contrib/vim-syntax/syntax
	fi
}

pkg_postinst() {
	elog ""
	elog "To use docker, the docker daemon must be running as root. To automatically"
	elog "start the docker daemon at boot, add docker to the default runlevel:"
	elog "  rc-update add docker default"
	elog "Similarly for systemd:"
	elog "  systemctl enable docker.service"
	elog ""

	# create docker group if the code checking for it in /etc/group exists
	enewgroup docker

	elog "To use docker as a non-root user, add yourself to the docker group."
	elog ""

	ewarn ""
	ewarn "If you want your containers to have access to the public internet or even"
	ewarn "the existing private network, IP Forwarding must be enabled:"
	ewarn "  sysctl -w net.ipv4.ip_forward=1"
	ewarn "or more permanently:"
	ewarn "  echo net.ipv4.ip_forward = 1 > /etc/sysctl.d/${PN}.conf"
	ewarn "Please be mindful of the security implications of enabling IP Forwarding."
	ewarn ""
}
