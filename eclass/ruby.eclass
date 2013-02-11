# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/ruby.eclass,v 1.81 2011/08/22 04:46:32 vapier Exp $

# DEPRECATION NOTICE
# This eclass is deprecated because it does not properly handle
# multiple ruby targets. Please use ruby-ng.eclass instead.
#
# @ECLASS: ruby.eclass
# @MAINTAINER:
# Ruby herd <ruby@gentoo.org>
# @AUTHOR:
# Author: Mamoru KOMACHI <usata@gentoo.org>

# The ruby eclass is designed to allow easier installation of ruby
# softwares, and their incorporation into the Gentoo Linux system.

# src_unpack, src_compile and src_install call a set of functions to emerge
# ruby with SLOT support; econf, emake and einstall is a wrapper for ruby
# to automate configuration, make and install process (they override default
# econf, emake and einstall defined by ebuild.sh respectively).

# Functions:
# src_unpack	Unpacks source archive(s) and apply patches if any.
# src_compile	Invokes econf and emake.
# src_install	Runs einstall and erubydoc.
# econf		Detects setup.rb, install.rb, extconf.rb and configure,
#		and then runs the configure script.
# emake		Runs make if any Makefile exists.
# einstall	Calls install script or Makefile. If both not present,
#		installs programs under sitedir.
# erubydoc	Finds any documents and puts them in the right place.
#		erubydoc needs more sophistication to handle all types of
#		appropriate documents.

# Variables:
# USE_RUBY	Space delimited list of supported ruby.
#		Set it to "any" if it installs only version independent files.
#		If your ebuild supports both ruby 1.6 and 1.8 but has version
#		depenedent files such as libraries, set it to something like
#		"ruby16 ruby18". Possible values are "any ruby16 ruby18 ruby19"
# RUBY_ECONF	You can pass extra arguments to econf by defining this
#		variable. Note that you cannot specify them by command line
#		if you are using <sys-apps/portage-2.0.49-r17.
# @VARIABLE: PATCHES
# @DESCRIPTION:
# If you have any patches to apply, set PATCHES to their locations and
# epatch will apply them. It also handles epatch-style bulk patches,
# if you know how to use them and set the correct variables. If you
# don't, read eutils.eclass.

inherit eutils toolchain-funcs

EXPORT_FUNCTIONS src_unpack src_compile src_install

HOMEPAGE="http://raa.ruby-lang.org/list.rhtml?name=${PN}"
SRC_URI="mirror://gentoo/${P}.tar.gz"

SLOT="0"
LICENSE="Ruby"

# If you specify RUBY_OPTIONAL you also need to take care of ruby useflag and dependency.
if [[ ${RUBY_OPTIONAL} != "yes" ]]; then
	DEPEND="${DEPEND} =dev-lang/ruby-1.8*"
	RDEPEND="${RDEPEND} =dev-lang/ruby-1.8*"
fi

[[ -z "${RUBY}" ]] && export RUBY=/usr/bin/ruby

ruby_patch_mkmf() {

	if [ ! -x /bin/install -a -x /usr/bin/install ]; then
		einfo "Patching mkmf"
		cat <<END >"${T}"/mkmf.rb
require 'mkmf'

STDERR.puts 'Modified mkmf is used'
CONFIG['INSTALL'] = '/usr/bin/install'
END
		# save it because rubygems needs it (for unsetting RUBYOPT)
		export GENTOO_RUBYOPT="-r${T}/mkmf.rb"
		export RUBYOPT="${RUBYOPT} ${GENTOO_RUBYOPT}"
	fi

}

ruby_src_unpack() {
	#ruby_patch_mkmf
	unpack ${A}
	cd "${S}"

	# Apply any patches that have been provided.
	if [[ ${#PATCHES[@]} -gt 1 ]]; then
		for x in "${PATCHES[@]}"; do
			epatch "${x}"
		done
	elif [[ -n "${PATCHES}" ]]; then
		for x in ${PATCHES}; do
			epatch "${x}"
		done
	fi
}

ruby_econf() {

	RUBY_ECONF="${RUBY_ECONF} ${EXTRA_ECONF}"
	if [ -f configure ] ; then
		./configure \
			--prefix=/usr \
			--host=${CHOST} \
			--mandir=/usr/share/man \
			--infodir=/usr/share/info \
			--datadir=/usr/share \
			--sysconfdir=/etc \
			--localstatedir=/var/lib \
			--with-ruby=${RUBY} \
			${RUBY_ECONF} \
			"$@" || die "econf failed"
	fi
	if [ -f install.rb ] ; then
		${RUBY} install.rb config --prefix=/usr "$@" \
			${RUBY_ECONF} || die "install.rb config failed"
		${RUBY} install.rb setup "$@" \
			${RUBY_ECONF} || die "install.rb setup failed"
	fi
	if [ -f setup.rb ] ; then
		${RUBY} setup.rb config --prefix=/usr "$@" \
			${RUBY_ECONF} || die "setup.rb config failed"
		${RUBY} setup.rb setup "$@" \
			${RUBY_ECONF} || die "setup.rb setup failed"
	fi
	if [ -f extconf.rb ] ; then
		${RUBY} extconf.rb "$@" \
			${RUBY_ECONF} || die "extconf.rb failed"
	fi
}

ruby_emake() {
	if [ -f makefiles -o -f GNUmakefile -o -f makefile -o -f Makefile ] ; then
		emake CC="$(tc-getCC)" CXX="$(tc-getCXX)" DLDFLAGS="${LDFLAGS}" "$@" || die "emake for ruby failed"
	fi
}

ruby_src_compile() {
	# You can pass configure options via RUBY_ECONF
	ruby_econf || die
	ruby_emake "$@" || die
}

doruby() {
	( # dont want to pollute calling env
		insinto $(${RUBY} -r rbconfig -e 'print Config::CONFIG["sitedir"]')
		insopts -m 0644
		doins "$@"
	) || die "failed to install $@"
}

ruby_einstall() {
	local siteruby

	RUBY_ECONF="${RUBY_ECONF} ${EXTRA_ECONF}"
	if [ -f install.rb ] ; then
		${RUBY} install.rb config --prefix="${D}"/usr "$@" \
			${RUBY_ECONF} || die "install.rb config failed"
		${RUBY} install.rb install "$@" \
			${RUBY_ECONF} || die "install.rb install failed"
	elif [ -f setup.rb ] ; then
		${RUBY} setup.rb config --prefix="${D}"/usr "$@" \
			${RUBY_ECONF} || die "setup.rb config failed"
		${RUBY} setup.rb install "$@" \
			${RUBY_ECONF} || die "setup.rb install failed"
	elif [ -f extconf.rb -o -f Makefile ] ; then
		make DESTDIR="${D}" "$@" install || die "make install failed"
	else
		siteruby=$(${RUBY} -r rbconfig -e 'print Config::CONFIG["sitedir"]')
		insinto ${siteruby}
		doins *.rb || die "doins failed"
	fi
}

erubydoc() {
	local rdbase=/usr/share/doc/${PF}/rd rdfiles=$(find . -name '*.rd*')

	einfo "running dodoc for ruby ;)"

	insinto ${rdbase}
	[ -n "${rdfiles}" ] && doins ${rdfiles}
	rmdir "${D}"${rdbase} 2>/dev/null || true
	if [ -d doc -o -d docs ] ; then
		dohtml -x html -r {doc,docs}/*
		dohtml -r {doc,docs}/html/*
	else
		dohtml -r *
	fi

	if has examples ${IUSE} && use examples; then
		for dir in sample samples example examples; do
			if [ -d ${dir} ] ; then
				dodir /usr/share/doc/${PF}
				cp -pPR ${dir} "${D}"/usr/share/doc/${PF} || die "cp failed"
			fi
		done
	fi

	for i in ChangeLog* [[:upper:]][[:upper:]]* ; do
		[ -e $i ] && dodoc $i
	done
}

ruby_src_install() {

	ruby_einstall "$@" || die

	erubydoc
}

# erubyconf, erubymake and erubyinstall are kept for compatibility
erubyconf() {
	ruby_econf "$@"
}

erubymake() {
	ruby_emake "$@"
}

erubyinstall() {
	ruby_einstall "$@"
}

# prepall adds SLOT support for ruby.eclass. SLOT support currently
# does not work for gems, so if a gem is installed we skip all the
# SLOT code to avoid possible errors, in particular the mv command
# that is part of the USE_RUBY="any" case.
prepall() {

	if [ -z "${GEM_SRC}" ]; then

		[[ ! -x /usr/bin/ruby16 ]] && export USE_RUBY=${USE_RUBY/ruby16/}
		[[ ! -x /usr/bin/ruby18 ]] && export USE_RUBY=${USE_RUBY/ruby18/}
		[[ ! -x /usr/bin/ruby19 ]] && export USE_RUBY=${USE_RUBY/ruby19/}
		[[ ! -x /usr/bin/rubyee ]] && export USE_RUBY=${USE_RUBY/rubyee/}

		local ruby_slots=$(echo "${USE_RUBY}" | wc -w)

		if [ "$ruby_slots" -ge 2 ] ; then
			einfo "Now we are building the package for ${USE_RUBY}"
			for rb in ${USE_RUBY} ; do
				einfo "Using $rb"
				export RUBY=/usr/bin/$rb
				ruby() { /usr/bin/$rb "$@" ; }
				mkdir -p "${S}"
				cd "${WORKDIR}"
				einfo "Unpacking for $rb"
				src_unpack || die "src_unpack failed"
				cd "${S}"
				find . -name '*.[ao]' -exec rm {} \;
				einfo "Building for $rb"
				src_compile || die "src_compile failed"
				cd "${S}"
				einfo "Installing for $rb"
				src_install || die "src_install failed"
			done
		elif [ "${USE_RUBY}" == "any" ] ; then
			eerror "USE_RUBY=\"any\" is no longer supported. Please use explicit versions instead."
			die "USE_RUBY=\"any\" is no longer supported."
		fi
	fi

	# Continue with the regular prepall, see bug 140697
	(unset prepall; prepall)
}

