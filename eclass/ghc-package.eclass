# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/ghc-package.eclass,v 1.36 2013/01/06 13:06:35 slyfox Exp $

# @ECLASS: ghc-package.eclass
# @MAINTAINER:
# "Gentoo's Haskell Language team" <haskell@gentoo.org>
# @AUTHOR:
# Original Author: Andres Loeh <kosmikus@gentoo.org>
# @BLURB: This eclass helps with the Glasgow Haskell Compiler's package configuration utility.
# @DESCRIPTION:
# Helper eclass to handle ghc installation/upgrade/deinstallation process.

inherit versionator

# @FUNCTION: ghc-getghc
# @DESCRIPTION:
# returns the name of the ghc executable
ghc-getghc() {
	type -P ghc
}

# @FUNCTION: ghc-getghcpkg
# @DESCRIPTION:
# Internal function determines returns the name of the ghc-pkg executable
ghc-getghcpkg() {
	type -P ghc-pkg
}

# @FUNCTION: ghc-getghcpkgbin
# @DESCRIPTION:
# returns the name of the ghc-pkg binary (ghc-pkg
# itself usually is a shell script, and we have to
# bypass the script under certain circumstances);
# for Cabal, we add an empty global package config file,
# because for some reason the global package file
# must be specified
ghc-getghcpkgbin() {
	# the ghc-pkg executable changed name in ghc 6.10, as it no longer needs
	# the wrapper script with the static flags
	echo '[]' > "${T}/empty.conf"
	if version_is_at_least "7.7.20121101" "$(ghc-version)"; then
		# was moved to bin/ subtree by:
		# http://www.haskell.org/pipermail/cvs-ghc/2012-September/076546.html
		echo "$(ghc-libdir)/bin/ghc-pkg" "--global-package-db=${T}/empty.conf"
	elif version_is_at_least "7.5.20120516" "$(ghc-version)"; then
		echo "$(ghc-libdir)/ghc-pkg" "--global-package-db=${T}/empty.conf"
	else
		echo "$(ghc-libdir)/ghc-pkg" "--global-conf=${T}/empty.conf"
	fi
}

# @FUNCTION: ghc-version
# @DESCRIPTION:
# returns the version of ghc
_GHC_VERSION_CACHE=""
ghc-version() {
	if [[ -z "${_GHC_VERSION_CACHE}" ]]; then
		_GHC_VERSION_CACHE="$($(ghc-getghc) --numeric-version)"
	fi
	echo "${_GHC_VERSION_CACHE}"
}

# @FUNCTION: ghc-bestcabalversion
# @DESCRIPTION:
# return the best version of the Cabal library that is available
ghc-bestcabalversion() {
	# We ask portage, not ghc, so that we only pick up
	# portage-installed cabal versions.
	local cabalversion="$(ghc-extractportageversion dev-haskell/cabal)"
	echo "Cabal-${cabalversion}"
}

# @FUNCTION: ghc-sanecabal
# @DESCRIPTION:
# check if a standalone Cabal version is available for the
# currently used ghc; takes minimal version of Cabal as
# an optional argument
ghc-sanecabal() {
	local f
	local version
	if [[ -z "$1" ]]; then version="1.0.1"; else version="$1"; fi
	for f in $(ghc-confdir)/cabal-*; do
		[[ -f "${f}" ]] && version_is_at_least "${version}" "${f#*cabal-}" && return
	done
	return 1
}

# @FUNCTION: ghc-supports-shared-libraries
# @DESCRIPTION:
# checks if ghc is built with support for building
# shared libraries (aka '-dynamic' option)
ghc-supports-shared-libraries() {
	$(ghc-getghc) --info | grep "RTS ways" | grep -q "dyn"
}

# @FUNCTION: ghc-supports-threaded-runtime
# @DESCRIPTION:
# checks if ghc is built with support for threaded
# runtime (aka '-threaded' option)
ghc-supports-threaded-runtime() {
	$(ghc-getghc) --info | grep "RTS ways" | grep -q "thr"
}

# @FUNCTION: ghc-extractportageversion
# @DESCRIPTION:
# extract the version of a portage-installed package
ghc-extractportageversion() {
	local pkg
	local version
	pkg="$(best_version $1)"
	version="${pkg#$1-}"
	version="${version%-r*}"
	version="${version%_pre*}"
	echo "${version}"
}

# @FUNCTION: ghc-libdir
# @DESCRIPTION:
# returns the library directory
_GHC_LIBDIR_CACHE=""
ghc-libdir() {
	if [[ -z "${_GHC_LIBDIR_CACHE}" ]]; then
		_GHC_LIBDIR_CACHE="$($(ghc-getghc) --print-libdir)"
	fi
	echo "${_GHC_LIBDIR_CACHE}"
}

# @FUNCTION: ghc-confdir
# @DESCRIPTION:
# returns the (Gentoo) library configuration directory
ghc-confdir() {
	echo "$(ghc-libdir)/gentoo"
}

# @FUNCTION: ghc-localpkgconf
# @DESCRIPTION:
# returns the name of the local (package-specific)
# package configuration file
ghc-localpkgconf() {
	echo "${PF}.conf"
}

# @FUNCTION: ghc-makeghcilib
# @DESCRIPTION:
# make a ghci foo.o file from a libfoo.a file
ghc-makeghcilib() {
	local outfile
	outfile="$(dirname $1)/$(basename $1 | sed 's:^lib\?\(.*\)\.a$:\1.o:')"
	ld --relocatable --discard-all --output="${outfile}" --whole-archive "$1"
}

# @FUNCTION: ghc-package-exists
# @DESCRIPTION:
# tests if a ghc package exists
ghc-package-exists() {
	$(ghc-getghcpkg) describe "$1" > /dev/null 2>&1
}

# @FUNCTION: ghc-setup-pkg
# @DESCRIPTION:
# creates a local (package-specific) package
# configuration file; the arguments should be
# uninstalled package description files, each
# containing a single package description; if
# no arguments are given, the resulting file is
# empty
ghc-setup-pkg() {
	local localpkgconf="${S}/$(ghc-localpkgconf)"
	echo '[]' > "${localpkgconf}"

	for pkg in $*; do
		$(ghc-getghcpkgbin) -f "${localpkgconf}" update - --force \
			< "${pkg}" || die "failed to register ${pkg}"
	done
}

# @FUNCTION: ghc-fixlibpath
# @DESCRIPTION:
# fixes the library and import directories path
# of the package configuration file
ghc-fixlibpath() {
	sed -i "s|$1|$(ghc-libdir)|g" "${S}/$(ghc-localpkgconf)"
	if [[ -n "$2" ]]; then
		sed -i "s|$2|$(ghc-libdir)/imports|g" "${S}/$(ghc-localpkgconf)"
	fi
}

# @FUNCTION: ghc-install-pkg
# @DESCRIPTION:
# moves the local (package-specific) package configuration
# file to its final destination
ghc-install-pkg() {
	mkdir -p "${D}/$(ghc-confdir)"
	cat "${S}/$(ghc-localpkgconf)" | sed "s|${D}||g" \
		> "${D}/$(ghc-confdir)/$(ghc-localpkgconf)"
}

# @FUNCTION: ghc-register-pkg
# @DESCRIPTION:
# registers all packages in the local (package-specific)
# package configuration file
ghc-register-pkg() {
	local localpkgconf="$(ghc-confdir)/$1"

	if [[ -f "${localpkgconf}" ]]; then
		for pkg in $(ghc-listpkg "${localpkgconf}"); do
			ebegin "Registering ${pkg} "
			$(ghc-getghcpkgbin) -f "${localpkgconf}" describe "${pkg}" \
				| $(ghc-getghcpkg) update - --force > /dev/null
			eend $?
		done
	fi
}

# @FUNCTION: ghc-reregister
# @DESCRIPTION:
# re-adds all available .conf files to the global
# package conf file, to be used on a ghc reinstallation
ghc-reregister() {
	has "${EAPI:-0}" 0 1 2 && ! use prefix && EPREFIX=
	einfo "Re-adding packages (may cause several harmless warnings) ..."
	PATH="${EPREFIX}/usr/bin:${PATH}" CONFDIR="$(ghc-confdir)"
	if [ -d "${CONFDIR}" ]; then
		pushd "${CONFDIR}" > /dev/null
		for conf in *.conf; do
			PATH="${EPREFIX}/usr/bin:${PATH}" ghc-register-pkg "${conf}"
		done
		popd > /dev/null
	fi
}

# @FUNCTION: ghc-unregister-pkg
# @DESCRIPTION:
# unregisters a package configuration file
# protected are all packages that are still contained in
# another package configuration file
ghc-unregister-pkg() {
	local localpkgconf="$(ghc-confdir)/$1"
	local i
	local pkg

	if [[ -f "${localpkgconf}" ]]; then
		for pkg in $(ghc-reverse "$(ghc-listpkg ${localpkgconf})"); do
		  if ! ghc-package-exists "${pkg}"; then
			einfo "Package ${pkg} is not installed for ghc-$(ghc-version)."
		  else
			ebegin "Unregistering ${pkg} "
			$(ghc-getghcpkg) unregister "${pkg}" --force > /dev/null
			eend $?
		  fi
		done
	fi
}

# @FUNCTION: ghc-reverse
# @DESCRIPTION:
# help-function: reverse a list
ghc-reverse() {
	local result
	local i
	for i in $1; do
		result="${i} ${result}"
	done
	echo "${result}"
}

# @FUNCTION: ghc-elem
# @DESCRIPTION:
# help-function: element-check
ghc-elem() {
	local i
	for i in $2; do
		[[ "$1" == "${i}" ]] && return 0
	done
	return 1
}

# @FUNCTION: ghc-listpkg
# @DESCRIPTION:
# show the packages in a package configuration file
ghc-listpkg() {
	local ghcpkgcall
	local i
	local extra_flags
	if version_is_at_least '6.12.3' "$(ghc-version)"; then
		extra_flags="${extra_flags} -v0"
	fi
	for i in $*; do
		echo $($(ghc-getghcpkg) list ${extra_flags} -f "${i}") \
			| sed \
				-e "s|^.*${i}:\([^:]*\).*$|\1|" \
				-e "s|/.*$||" \
				-e "s|,| |g" -e "s|[(){}]||g"
	done
}

# @FUNCTION: ghc-package_pkg_postinst
# @DESCRIPTION:
# exported function: registers the package-specific package
# configuration file
ghc-package_pkg_postinst() {
	ghc-register-pkg "$(ghc-localpkgconf)"
}

# @FUNCTION: ghc-package_pkg_prerm
# @DESCRIPTION:
# exported function: unregisters the package-specific package
# configuration file; a package contained therein is unregistered
# only if it the same package is not also contained in another
# package configuration file ...
ghc-package_pkg_prerm() {
	ghc-unregister-pkg "$(ghc-localpkgconf)"
}

EXPORT_FUNCTIONS pkg_postinst pkg_prerm
