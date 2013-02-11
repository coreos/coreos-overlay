# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/vim.eclass,v 1.205 2012/10/24 18:55:30 ulm Exp $

# Authors:
# 	Jim Ramsay <lack@gentoo.org>
# 	Ryan Phillips <rphillips@gentoo.org>
# 	Seemant Kulleen <seemant@gentoo.org>
# 	Aron Griffis <agriffis@gentoo.org>
# 	Ciaran McCreesh <ciaranm@gentoo.org>
#	Mike Kelly <pioto@gentoo.org>

# This eclass handles vim, gvim and vim-core.  Support for -cvs ebuilds is
# included in the eclass, since it's rather easy to do, but there are no
# official vim*-cvs ebuilds in the tree.

# gvim's GUI preference order is as follows:
# aqua                          CARBON (not tested)
# -aqua gtk gnome               GNOME2
# -aqua gtk -gnome              GTK2
# -aqua -gtk  motif             MOTIF
# -aqua -gtk -motif neXt        NEXTAW
# -aqua -gtk -motif -neXt       ATHENA

# Support -cvs ebuilds, even though they're not in the official tree.
MY_PN=${PN%-cvs}

if [[ ${MY_PN} != "vim-core" ]] ; then
	# vim supports python-2 only
	PYTHON_DEPEND="python? 2"
	PYTHON_USE_WITH_OPT="python"
	PYTHON_USE_WITH="threads"
fi
inherit eutils vim-doc flag-o-matic versionator fdo-mime bash-completion-r1 prefix python

HOMEPAGE="http://www.vim.org/"
SLOT="0"
LICENSE="vim"

# Check for EAPI functions we need:
case "${EAPI:-0}" in
	0|1)
		die "vim.eclass no longer supports EAPI 0 or 1"
		;;
	2|3)
		HAS_SRC_PREPARE=1
		HAS_USE_DEP=1
		;;
	*)
		die "Unknown EAPI ${EAPI}"
		;;
esac

if [[ ${PN##*-} == "cvs" ]] ; then
	inherit cvs
fi

IUSE="nls acl"

TO_EXPORT="pkg_setup src_compile src_install src_test pkg_postinst pkg_postrm"
if [[ $HAS_SRC_PREPARE ]]; then
	TO_EXPORT="${TO_EXPORT} src_prepare src_configure"
else
	TO_EXPORT="${TO_EXPORT} src_unpack"
fi
EXPORT_FUNCTIONS ${TO_EXPORT}

DEPEND="${DEPEND}
	>=app-admin/eselect-vi-1.1
	>=sys-apps/sed-4
	sys-devel/autoconf
	>=sys-libs/ncurses-5.2-r2
	nls? ( virtual/libintl )"
RDEPEND="${RDEPEND}
	>=app-admin/eselect-vi-1.1
	>=sys-libs/ncurses-5.2-r2
	nls? ( virtual/libintl )"

if [[ ${MY_PN} == "vim-core" ]] ; then
	IUSE="${IUSE} livecd"
	PDEPEND="!livecd? ( app-vim/gentoo-syntax )"
else
	IUSE="${IUSE} cscope debug gpm perl python ruby"

	DEPEND="${DEPEND}
		cscope?  ( dev-util/cscope )
		gpm?     ( >=sys-libs/gpm-1.19.3 )
		perl?    ( dev-lang/perl )
		acl?     ( kernel_linux? ( sys-apps/acl ) )
		ruby?    ( || ( dev-lang/ruby:1.9 dev-lang/ruby:1.8 ) )"
	RDEPEND="${RDEPEND}
		cscope?  ( dev-util/cscope )
		gpm?     ( >=sys-libs/gpm-1.19.3 )
		perl?    ( dev-lang/perl )
		acl?     ( kernel_linux? ( sys-apps/acl ) )
		ruby?    ( || ( dev-lang/ruby:1.9 dev-lang/ruby:1.8 ) )
		!<app-vim/align-30-r1
		!<app-vim/vimbuddy-0.9.1-r1
		!<app-vim/autoalign-11
		!<app-vim/supertab-0.41"

	# mzscheme support is currently broken. bug #91970
	# IUSE="${IUSE} mzscheme"
	# DEPEND="${DEPEND}
	# 	mzscheme? ( dev-scheme/mzscheme )"
	# RDEPEND="${RDEPEND}
	# 	mzscheme? ( dev-scheme/mzscheme )"

	if [[ ${MY_PN} == vim ]] ; then
		IUSE="${IUSE} X minimal vim-pager"
		DEPEND="${DEPEND}
			X? ( x11-libs/libXt x11-libs/libX11
				x11-libs/libSM x11-proto/xproto )
			!minimal? ( dev-util/ctags )"
		RDEPEND="${RDEPEND}
			X? ( x11-libs/libXt )
			!minimal? ( ~app-editors/vim-core-${PV}
				dev-util/ctags )
			!<app-editors/nvi-1.81.5-r4"
	elif [[ ${MY_PN} == gvim ]] ; then
		IUSE="${IUSE} aqua gnome gtk motif neXt netbeans"
		DEPEND="${DEPEND}
			dev-util/ctags
			!aqua? (
				gtk? (
					virtual/pkgconfig
				)
			)"
		RDEPEND="${RDEPEND}
			~app-editors/vim-core-${PV}
			dev-util/ctags
			x11-libs/libXext
			!aqua? (
				gtk? (
					>=x11-libs/gtk+-2.6:2
					x11-libs/libXft
					gnome? ( >=gnome-base/libgnomeui-2.6 )
				)
				!gtk? (
					motif? (
						>=x11-libs/motif-2.3:0
					)
					!motif? (
						neXt? (
							x11-libs/neXtaw
						)
						!neXt? ( x11-libs/libXaw )
					)
				)
			)"
	fi
fi

apply_vim_patches() {
	local p
	cd "${S}" || die "cd ${S} failed"

	# Scan the patches, applying them only to files that either
	# already exist or that will be created by the patch
	#
	# Changed awk to gawk in the below; BSD's awk chokes on it
	# --spb, 2004/12/18
	#
	# Allow either gzipped or uncompressed patches in the tarball.
	# --lack 2009-05-18
	#
	# Also removed date-seeking regexp to find first and second lines of the
	# patch since as of 7.2.167 the date format has changed.  It is less work
	# (while marginally less correct) to just look for lines that start with
	# '***' and do not end with '****' (and "---" / "----" for the second line).
	# --lack 2009-05-18
	einfo "Filtering vim patches ..."
	p=${WORKDIR}/${VIM_ORG_PATCHES%.tar*}.patch
	ls "${WORKDIR}"/vimpatches | sort | \
	while read f; do
		local fpath="${WORKDIR}"/vimpatches/${f}
		case $f in
			*.gz)
				gzip -dc "${fpath}"
				;;
			*)
				cat "${fpath}"
				;;
		esac
	done | gawk '
		/^Subject: [Pp]atch/ {
			if (patchnum) {printf "\n" >"/dev/stderr"}
			patchnum = $3
			printf "%s:", patchnum >"/dev/stderr"
		}
		$1=="***" && $(NF)!="****" {
			# First line of a patch; suppress printing
			firstlines = $0
			next
		}
		$1=="---" && $(NF)!="----" {
			# Second line of a patch; try to open the file to see
			# if it exists.
			thisfile = $2
			if (!seen[thisfile] && (getline tryme < thisfile) == -1) {
				# Check if it will be created
				firstlines = firstlines "\n" $0
				getline
				firstlines = firstlines "\n" $0
				getline
				if ($0 != "*** 0 ****") {
					# Non-existent and not created, stop printing
					printing = 0
					printf " (%s)", thisfile >"/dev/stderr"
					next
				}
			}
			# Close the file to avoid leakage, bug 205037
			close(thisfile)
			# Print the previous lines and start printing
			print firstlines
			printing = 1
			printf " %s", thisfile >"/dev/stderr"
			# Remember that we have seen this file
			seen[thisfile] = 1
		}
		printing { print }
		END { if (patchnum) {printf "\n" >"/dev/stderr"} }
		' > ${p} || die

	# For reasons yet unknown, epatch fails to apply this cleanly
	ebegin "Applying filtered vim patches"
	TMPDIR=${T} patch -f -s -p0 < ${p}
	eend 0
}

vim_pkg_setup() {
	# people with broken alphabets run into trouble. bug 82186.
	unset LANG LC_ALL
	export LC_COLLATE="C"

	# Gnome sandbox silliness. bug #114475.
	mkdir -p "${T}/home"
	export HOME="${T}/home"

	if [[ ${MY_PN} != "vim-core" ]] && use python; then
		# vim supports python-2 only
		python_set_active_version 2
		if [[ $HAS_USE_DEP ]]; then
			# python.eclass only defines python_pkg_setup for EAPIs that support
			# USE dependencies
			python_pkg_setup
		elif ! has_version "=dev-lang/python-2*[threads]"; then
			die "You must build dev-lang/python with USE=threads"
		fi
	fi
}

vim_src_prepare() {
	has "${EAPI:-0}" 0 1 2 && ! use prefix && EPREFIX=
	if [[ ${PN##*-} == cvs ]] ; then
		ECVS_SERVER="vim.cvs.sourceforge.net:/cvsroot/vim"
		ECVS_PASS=""
		ECVS_MODULE="vim7"
		ECVS_TOP_DIR="${PORTAGE_ACTUAL_DISTDIR-${DISTDIR}}/cvs-src/${ECVS_MODULE}"
		cvs_src_unpack
	else
		# Apply any patches available from vim.org for this version
		if [[ $VIM_ORG_PATCHES == *.patch.bz2 ]]; then
			einfo "Applying monolithic patch ${VIM_ORG_PATCHES}"
			epatch "${WORKDIR}/${VIM_ORG_PATCHES%.bz2}"
		else
			apply_vim_patches
		fi

		# Unpack the runtime snapshot if available (only for vim-core)
		if [[ -n "$VIM_RUNTIME_SNAP" ]] ; then
			cd "${S}" || die
			ebegin "Unpacking vim runtime snapshot"
			rm -rf runtime
			# Changed this from bzip2 |tar to tar -j since the former broke for
			# some reason on freebsd.
			#  --spb, 2004/12/18
			tar xjf "${DISTDIR}"/${VIM_RUNTIME_SNAP}
			eend $?
		fi
	fi

	# Another set of patches borrowed from src rpm to fix syntax errors etc.
	cd "${S}" || die "cd ${S} failed"
	if [[ -d "${WORKDIR}"/gentoo/patches-all/ ]]; then
		EPATCH_SUFFIX="gz" EPATCH_FORCE="yes" \
			epatch "${WORKDIR}"/gentoo/patches-all/
	elif [[ ${MY_PN} == "vim-core" ]] && [[ -d "${WORKDIR}"/gentoo/patches-core/ ]]; then
		# Patches for vim-core only (runtime/*)
		EPATCH_SUFFIX="patch" EPATCH_FORCE="yes" \
			epatch "${WORKDIR}"/gentoo/patches-core/
	fi

	# Unpack an updated netrw snapshot if necessary. This is nasty. Don't
	# ask, you don't want to know.
	if [[ -n "${VIM_NETRW_SNAP}" ]] ; then
		ebegin "Unpacking updated netrw snapshot"
		tar xjf "${DISTDIR}"/${VIM_NETRW_SNAP} -C runtime/
		eend $?
	fi

	# Fixup a script to use awk instead of nawk
	sed -i '1s|.*|#!'"${EPREFIX}"'/usr/bin/awk -f|' "${S}"/runtime/tools/mve.awk \
		|| die "mve.awk sed failed"

	# Patch to build with ruby-1.8.0_pre5 and following
	sed -i 's/defout/stdout/g' "${S}"/src/if_ruby.c

	# Read vimrc and gvimrc from /etc/vim
	echo '#define SYS_VIMRC_FILE "'${EPREFIX}'/etc/vim/vimrc"' >> "${S}"/src/feature.h
	echo '#define SYS_GVIMRC_FILE "'${EPREFIX}'/etc/vim/gvimrc"' >> "${S}"/src/feature.h

	# Use exuberant ctags which installs as /usr/bin/exuberant-ctags.
	# Hopefully this pattern won't break for a while at least.
	# This fixes bug 29398 (27 Sep 2003 agriffis)
	sed -i 's/\<ctags\("\| [-*.]\)/exuberant-&/g' \
		"${S}"/runtime/doc/syntax.txt \
		"${S}"/runtime/doc/tagsrch.txt \
		"${S}"/runtime/doc/usr_29.txt \
		"${S}"/runtime/menu.vim \
		"${S}"/src/configure.in || die 'sed failed'

	# Don't be fooled by /usr/include/libc.h.  When found, vim thinks
	# this is NeXT, but it's actually just a file in dev-libs/9libs
	# This fixes bug 43885 (20 Mar 2004 agriffis)
	sed -i 's/ libc\.h / /' "${S}"/src/configure.in || die 'sed failed'

	# gcc on sparc32 has this, uhm, interesting problem with detecting EOF
	# correctly. To avoid some really entertaining error messages about stuff
	# which isn't even in the source file being invalid, we'll do some trickery
	# to make the error never occur. bug 66162 (02 October 2004 ciaranm)
	find "${S}" -name '*.c' | while read c ; do echo >> "$c" ; done

	# conditionally make the manpager.sh script
	if [[ ${MY_PN} == vim ]] && use vim-pager ; then
		cat <<END > "${S}"/runtime/macros/manpager.sh
#!/bin/sh
sed -e 's/\x1B\[[[:digit:]]\+m//g' | col -b | \\
		vim \\
			-c 'let no_plugin_maps = 1' \\
			-c 'set nolist nomod ft=man' \\
			-c 'let g:showmarks_enable=0' \\
			-c 'runtime! macros/less.vim' -
END
	fi

	# Try to avoid sandbox problems. Bug #114475.
	if [[ -d "${S}"/src/po ]] ; then
		sed -i -e \
			'/-S check.vim/s,..VIM.,ln -s $(VIM) testvim \; ./testvim -X,' \
			"${S}"/src/po/Makefile
	fi

	if version_is_at_least 7.3.122; then
		cp "${S}"/src/config.mk.dist "${S}"/src/auto/config.mk
	fi

	# Bug #378107 - Build properly with >=perl-core/ExtUtils-ParseXS-3.20.0
	if version_is_at_least 7.3; then
		sed -i "s:\\\$(PERLLIB)/ExtUtils/xsubpp:${EPREFIX}/usr/bin/xsubpp:"	\
			"${S}"/src/Makefile || die 'sed for ExtUtils-ParseXS failed'
	fi
}

vim_src_unpack() {
	unpack ${A}
	vim_src_prepare
}

vim_src_configure() {
	local myconf

	# Fix bug 37354: Disallow -funroll-all-loops on amd64
	# Bug 57859 suggests that we want to do this for all archs
	filter-flags -funroll-all-loops

	# Fix bug 76331: -O3 causes problems, use -O2 instead. We'll do this for
	# everyone since previous flag filtering bugs have turned out to affect
	# multiple archs...
	replace-flags -O3 -O2

	# Fix bug 18245: Prevent "make" from the following chain:
	# (1) Notice configure.in is newer than auto/configure
	# (2) Rebuild auto/configure
	# (3) Notice auto/configure is newer than auto/config.mk
	# (4) Run ./configure (with wrong args) to remake auto/config.mk
	ebegin "Creating configure script"
	sed -i 's/ auto.config.mk:/:/' src/Makefile || die "Makefile sed failed"
	rm -f src/auto/configure
	# autoconf-2.13 needed for this package -- bug 35319
	# except it seems we actually need 2.5 now -- bug 53777
	WANT_AUTOCONF=2.5 \
		emake -j1 -C src autoconf || die "make autoconf failed"
	eend $?

	# This should fix a sandbox violation (see bug 24447). The hvc
	# things are for ppc64, see bug 86433.
	for file in /dev/pty/s* /dev/console /dev/hvc/* /dev/hvc* ; do
		[[ -e ${file} ]] && addwrite $file
	done

	if [[ ${MY_PN} == "vim-core" ]] ||
			( [[ ${MY_PN} == vim ]] && use minimal ); then
		myconf="--with-features=tiny \
			--enable-gui=no \
			--without-x \
			--disable-darwin \
			--disable-perlinterp \
			--disable-pythoninterp \
			--disable-rubyinterp \
			--disable-gpm"

	else
		use debug && append-flags "-DDEBUG"

		myconf="--with-features=huge \
			--enable-multibyte"
		myconf="${myconf} `use_enable cscope`"
		myconf="${myconf} `use_enable gpm`"
		myconf="${myconf} `use_enable perl perlinterp`"
		myconf="${myconf} `use_enable python pythoninterp`"
		myconf="${myconf} `use_enable ruby rubyinterp`"
		# tclinterp is broken; when you --enable-tclinterp flag, then
		# the following command never returns:
		#   VIMINIT='let OS=system("uname -s")' vim
		# mzscheme support is currently broken. bug #91970
		#myconf="${myconf} `use_enable mzscheme mzschemeinterp`"
		if [[ ${MY_PN} == gvim ]] ; then
			myconf="${myconf} `use_enable netbeans`"
		fi

		# --with-features=huge forces on cscope even if we --disable it. We need
		# to sed this out to avoid screwiness. (1 Sep 2004 ciaranm)
		if ! use cscope ; then
			sed -i -e '/# define FEAT_CSCOPE/d' src/feature.h || \
				die "couldn't disable cscope"
		fi

		if [[ ${MY_PN} == vim ]] ; then
			# don't test USE=X here ... see bug #19115
			# but need to provide a way to link against X ... see bug #20093
			myconf="${myconf} --enable-gui=no --disable-darwin `use_with X x`"

		elif [[ ${MY_PN} == gvim ]] ; then
			myconf="${myconf} --with-vim-name=gvim --with-x"

			echo ; echo
			if use aqua ; then
				einfo "Building gvim with the Carbon GUI"
				myconf="${myconf} --enable-darwin --enable-gui=carbon"
			elif use gtk ; then
				myconf="${myconf} --enable-gtk2-check"
				if use gnome ; then
					einfo "Building gvim with the Gnome 2 GUI"
					myconf="${myconf} --enable-gui=gnome2"
				else
					einfo "Building gvim with the gtk+-2 GUI"
					myconf="${myconf} --enable-gui=gtk2"
				fi
			elif use motif ; then
				einfo "Building gvim with the MOTIF GUI"
				myconf="${myconf} --enable-gui=motif"
			elif use neXt ; then
				einfo "Building gvim with the neXtaw GUI"
				myconf="${myconf} --enable-gui=nextaw"
			else
				einfo "Building gvim with the Athena GUI"
				myconf="${myconf} --enable-gui=athena"
			fi
			echo ; echo

		else
			die "vim.eclass doesn't understand MY_PN=${MY_PN}"
		fi
	fi

	if [[ ${MY_PN} == vim ]] && use minimal ; then
		myconf="${myconf} --disable-nls --disable-multibyte --disable-acl"
	else
		myconf="${myconf} `use_enable nls` `use_enable acl`"
	fi

	# Note: If USE=gpm, then ncurses will still be required. See bug #93970
	# for the reasons behind the USE flag change.
	myconf="${myconf} --with-tlib=curses"

	myconf="${myconf} --disable-selinux"

	# Let Portage do the stripping. Some people like that.
	export ac_cv_prog_STRIP="$(type -P true ) faking strip"

	# Keep Gentoo Prefix env contained within the EPREFIX
	use prefix && myconf="${myconf} --without-local-dir"

	if [[ ${MY_PN} == "*vim" ]] ; then
		if [[ ${CHOST} == *-interix* ]]; then
			# avoid finding of this function, to avoid having to patch either
			# configure or the source, which would be much more hackish.
			# after all vim does it right, only interix is badly broken (again)
			export ac_cv_func_sigaction=no
		fi
	fi

	myconf="${myconf} --with-modified-by=Gentoo-${PVR}"
	econf ${myconf} || die "vim configure failed"
}

vim_src_compile() {
	has src_configure ${TO_EXPORT} || vim_src_configure

	# The following allows emake to be used
	emake -j1 -C src auto/osdef.h objects || die "make failed"

	if [[ ${MY_PN} == "vim-core" ]] ; then
		emake tools || die "emake tools failed"
		rm -f src/vim
	else
		if ! emake ; then
			eerror "If the above messages seem to be talking about perl"
			eerror "and undefined references, please try re-emerging both"
			eerror "perl and libperl with the same USE flags. For more"
			eerror "information, see:"
			eerror "    https://bugs.gentoo.org/show_bug.cgi?id=18129"
			die "emake failed"
		fi
	fi
}

vim_src_install() {
	has "${EAPI:-0}" 0 1 2 && use !prefix && EPREFIX=
	has "${EAPI:-0}" 0 1 2 && use !prefix && ED="${D}"
	local vimfiles=/usr/share/vim/vim${VIM_VERSION/.}

	if [[ ${MY_PN} == "vim-core" ]] ; then
		dodir /usr/{bin,share/{man/man1,vim}}
		cd src || die "cd src failed"
		make \
			installruntime \
			installmanlinks \
			installmacros \
			installtutor \
			installtutorbin \
			installtools \
			install-languages \
			install-icons \
			DESTDIR=${D} \
			BINDIR="${EPREFIX}"/usr/bin \
			MANDIR="${EPREFIX}"/usr/share/man \
			DATADIR="${EPREFIX}"/usr/share \
			|| die "install failed"

		keepdir ${vimfiles}/keymap

		# default vimrc is installed by vim-core since it applies to
		# both vim and gvim
		insinto /etc/vim/
		newins "${FILESDIR}"/vimrc${VIMRC_FILE_SUFFIX} vimrc
		eprefixify "${ED}"/etc/vim/vimrc

		if use livecd ; then
			# To save space, install only a subset of the files if we're on a
			# livecd. bug 65144.
			einfo "Removing some files for a smaller livecd install ..."

			eshopts_push -s extglob

			rm -fr "${ED}${vimfiles}"/{compiler,doc,ftplugin,indent}
			rm -fr "${ED}${vimfiles}"/{macros,print,tools,tutor}
			rm "${ED}"/usr/bin/vimtutor

			local keep_colors="default"
			ignore=$(rm -fr "${ED}${vimfiles}"/colors/!(${keep_colors}).vim )

			local keep_syntax="conf|crontab|fstab|inittab|resolv|sshdconfig"
			# tinkering with the next line might make bad things happen ...
			keep_syntax="${keep_syntax}|syntax|nosyntax|synload"
			ignore=$(rm -fr "${ED}${vimfiles}"/syntax/!(${keep_syntax}).vim )

			eshopts_pop
		fi

		# These files might have slight security issues, so we won't
		# install them. See bug #77841. We don't mind if these don't
		# exist.
		rm "${ED}${vimfiles}"/tools/{vimspell.sh,tcltags} 2>/dev/null

	elif [[ ${MY_PN} == gvim ]] ; then
		dobin src/gvim
		dosym gvim /usr/bin/gvimdiff
		dosym gvim /usr/bin/evim
		dosym gvim /usr/bin/eview
		dosym gvim /usr/bin/gview
		dosym gvim /usr/bin/rgvim
		dosym gvim /usr/bin/rgview
		dosym vim.1.gz /usr/share/man/man1/gvim.1.gz
		dosym vim.1.gz /usr/share/man/man1/gview.1.gz
		dosym vimdiff.1.gz /usr/share/man/man1/gvimdiff.1.gz

		insinto /etc/vim
		newins "${FILESDIR}"/gvimrc${GVIMRC_FILE_SUFFIX} gvimrc
		eprefixify "${ED}"/etc/vim/gvimrc

		insinto /usr/share/applications
		newins "${FILESDIR}"/gvim.desktop${GVIM_DESKTOP_SUFFIX} gvim.desktop
		insinto /usr/share/pixmaps
		doins "${FILESDIR}"/gvim.xpm

	else # app-editor/vim
		# Note: Do not install symlinks for 'vi', 'ex', or 'view', as these are
		#       managed by eselect-vi
		dobin src/vim
		dosym vim /usr/bin/vimdiff
		dosym vim /usr/bin/rvim
		dosym vim /usr/bin/rview
		if use vim-pager ; then
			dosym ${vimfiles}/macros/less.sh /usr/bin/vimpager
			dosym ${vimfiles}/macros/manpager.sh /usr/bin/vimmanpager
			insinto ${vimfiles}/macros
			doins runtime/macros/manpager.sh
			fperms a+x ${vimfiles}/macros/manpager.sh
		fi
	fi

	# bash completion script, bug #79018.
	if [[ ${MY_PN} == "vim-core" ]] ; then
		newbashcomp "${FILESDIR}"/xxd-completion xxd
	else
		newbashcomp "${FILESDIR}"/${MY_PN}-completion ${MY_PN}
	fi
	# We shouldn't be installing the ex or view man page symlinks, as they
	# are managed by eselect-vi
	rm -f "${ED}"/usr/share/man/man1/{ex,view}.1
}

# Make convenience symlinks, hopefully without stepping on toes.  Some
# of these links are "owned" by the vim ebuild when it is installed,
# but they might be good for gvim as well (see bug 45828)
update_vim_symlinks() {
	has "${EAPI:-0}" 0 1 2 && use !prefix && EROOT="${ROOT}"
	local f syms
	syms="vimdiff rvim rview"
	einfo "Calling eselect vi update..."
	# Call this with --if-unset to respect user's choice (bug 187449)
	eselect vi update --if-unset

	# Make or remove convenience symlink, vim -> gvim
	if [[ -f "${EROOT}"/usr/bin/gvim ]]; then
		ln -s gvim "${EROOT}"/usr/bin/vim 2>/dev/null
	elif [[ -L "${EROOT}"/usr/bin/vim && ! -f "${EROOT}"/usr/bin/vim ]]; then
		rm "${EROOT}"/usr/bin/vim
	fi

	# Make or remove convenience symlinks to vim
	if [[ -f "${EROOT}"/usr/bin/vim ]]; then
		for f in ${syms}; do
			ln -s vim "${EROOT}"/usr/bin/${f} 2>/dev/null
		done
	else
		for f in ${syms}; do
			if [[ -L "${EROOT}"/usr/bin/${f} && ! -f "${EROOT}"/usr/bin/${f} ]]; then
				rm -f "${EROOT}"/usr/bin/${f}
			fi
		done
	fi

	# This will still break if you merge then remove the vi package,
	# but there's only so much you can do, eh?  Unfortunately we don't
	# have triggers like are done in rpm-land.
}

vim_pkg_postinst() {
	# Update documentation tags (from vim-doc.eclass)
	update_vim_helptags

	# Update fdo mime stuff, bug #78394
	if [[ ${MY_PN} == gvim ]] ; then
		fdo-mime_mime_database_update
	fi

	if [[ ${MY_PN} == vim ]] ; then
		if use X; then
			echo
			elog "The 'X' USE flag enables vim <-> X communication, like"
			elog "updating the xterm titlebar. It does not install a GUI."
		fi
		echo
		elog "To install a GUI version of vim, use the app-editors/gvim"
		elog "package."
	fi
	echo
	elog "Vim 7 includes an integrated spell checker. You need to install"
	elog "word list files before you can use it. There are ebuilds for"
	elog "some of these named app-vim/vim-spell-*. If your language of"
	elog "choice is not included, please consult vim-spell.eclass for"
	elog "instructions on how to make a package."
	echo
	ewarn "Note that the English word lists are no longer installed by"
	ewarn "default."

	if [[ ${MY_PN} != "vim-core" ]] ; then
		echo
		elog "To see what's new in this release, use :help version${VIM_VERSION/.*/}.txt"
	fi

	# Warn about VIMRUNTIME
	if [ -n "$VIMRUNTIME" -a "${VIMRUNTIME##*/vim}" != "${VIM_VERSION/./}" ] ; then
		echo
		ewarn "WARNING: You have VIMRUNTIME set in your environment from an old"
		ewarn "installation.  You will need to either unset VIMRUNTIME in each"
		ewarn "terminal, or log out completely and back in.  This problem won't"
		ewarn "happen again since the ebuild no longer sets VIMRUNTIME."
	fi

	# Scream loudly if the user is using a -cvs ebuild
	if [[ -z "${PN/*-cvs/}" ]] ; then
		ewarn
		ewarn "You are using a -cvs ebuild. Be warned that this is not"
		ewarn "officially supported and may not work."
		ebeep 5
	fi

	echo

	# Make convenience symlinks
	if [[ ${MY_PN} != "vim-core" ]] ; then
		# But only for vim/gvim, bug #252724
		update_vim_symlinks
	fi
}

vim_pkg_postrm() {
	# Update documentation tags (from vim-doc.eclass)
	update_vim_helptags

	# Make convenience symlinks
	if [[ ${MY_PN} != "vim-core" ]] ; then
		# But only for vim/gvim, bug #252724
		update_vim_symlinks
	fi

	# Update fdo mime stuff, bug #78394
	if [[ ${MY_PN} == gvim ]] ; then
		fdo-mime_mime_database_update
	fi
}

vim_src_test() {

	if [[ ${MY_PN} == "vim-core" ]] ; then
		einfo "No testing needs to be done for vim-core"
		return
	fi

	einfo " "
	einfo "Starting vim tests. Several error messages will be shown "
	einfo "whilst the tests run. This is normal behaviour and does not "
	einfo "indicate a fault."
	einfo " "
	ewarn "If the tests fail, your terminal may be left in a strange "
	ewarn "state. Usually, running 'reset' will fix this."
	ewarn " "
	echo

	# Don't let vim talk to X
	unset DISPLAY

	if [[ ${MY_PN} == gvim ]] ; then
		# Make gvim not try to connect to X. See :help gui-x11-start
		# in vim for how this evil trickery works.
		ln -s "${S}"/src/gvim "${S}"/src/testvim
		testprog="../testvim"
	else
		testprog="../vim"
	fi

	# We've got to call make test from within testdir, since the Makefiles
	# don't pass through our VIMPROG argument
	cd "${S}"/src/testdir

	# Test 49 won't work inside a portage environment
	einfo "Test 49 isn't sandbox-friendly, so it will be skipped."
	sed -i -e 's~test49.out~~g' Makefile

	# We don't want to rebuild vim before running the tests
	sed -i -e 's,: \$(VIMPROG),: ,' Makefile

	# Don't try to do the additional GUI test
	make VIMPROG=${testprog} nongui \
		|| die "At least one test failed"
}

