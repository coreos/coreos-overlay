# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
#
# This package is originated from
# http://sources.gentoo.org/sys-devel/clang/clang-9999.ebuild
#
# Note that we use downloading sources from SVN because llvm.org has
# not released this version yet.

EAPI=4

RESTRICT_PYTHON_ABIS="3.*"
SUPPORT_PYTHON_ABIS="1"

inherit subversion eutils multilib python

SVN_COMMIT=${PV#*_pre}

DESCRIPTION="C language family frontend for LLVM"
HOMEPAGE="http://clang.llvm.org/"
SRC_URI=""
ESVN_REPO_URI="http://llvm.org/svn/llvm-project/cfe/trunk@${SVN_COMMIT}"

LICENSE="UoI-NCSA"
SLOT="0"
KEYWORDS="amd64"
IUSE="debug multitarget +static-analyzer test"

DEPEND="static-analyzer? ( dev-lang/perl )"
RDEPEND="~sys-devel/llvm-${PV}[multitarget=]"

S="${WORKDIR}/llvm"

src_unpack() {
	# Fetching LLVM and subprojects
	ESVN_PROJECT=llvm subversion_fetch "http://llvm.org/svn/llvm-project/llvm/trunk@${SVN_COMMIT}"
	ESVN_PROJECT=compiler-rt S="${S}"/projects/compiler-rt subversion_fetch "http://llvm.org/svn/llvm-project/compiler-rt/trunk@${SVN_COMMIT}"
	ESVN_PROJECT=clang S="${S}"/tools/clang subversion_fetch
}

src_prepare() {
	if [ "/usr/x86_64-pc-linux-gnu/gcc-bin/4.6.x-google" != $(gcc-config -B) ]; then
		ewarn "Beware sheriff: gcc's binaries are not in '/usr/x86_64-pc-linux-gnu/gcc-bin/4.6.x-google'"
		ewarn "and are instead in $(gcc-config -B). This may lead to an unusable clang."
		ewarn "Please test clang with a simple hello_world.cc file and update this message"
	fi

	# Same as llvm doc patches
	epatch "${FILESDIR}"/${PN}-2.7-fixdoc.patch

	# multilib-strict
	sed -e "/PROJ_headers/s#lib/clang#$(get_libdir)/clang#" \
		-i tools/clang/lib/Headers/Makefile \
		|| die "clang Makefile failed"
	sed -e "/PROJ_resources/s#lib/clang#$(get_libdir)/clang#" \
		-i tools/clang/runtime/compiler-rt/Makefile \
		|| die "compiler-rt Makefile failed"
	# fix the static analyzer for in-tree install
	sed -e 's/import ScanView/from clang \0/'  \
		-i tools/clang/tools/scan-view/scan-view \
		|| die "scan-view sed failed"
	sed -e "/scanview.css\|sorttable.js/s#\$RealBin#${EPREFIX}/usr/share/${PN}#" \
		-i tools/clang/tools/scan-build/scan-build \
		|| die "scan-build sed failed"
	# Set correct path for gold plugin
	sed -e "/LLVMgold.so/s#lib/#$(get_libdir)/llvm/#" \
		-i  tools/clang/lib/Driver/Tools.cpp \
		|| die "gold plugin path sed failed"
	# Specify python version
	python_convert_shebangs 2 tools/clang/tools/scan-view/scan-view
	python_convert_shebangs -r 2 test/Scripts
	python_convert_shebangs 2 projects/compiler-rt/lib/asan/scripts/asan_symbolize.py

	# From llvm src_prepare
	einfo "Fixing install dirs"
	sed -e 's,^PROJ_docsdir.*,PROJ_docsdir := $(PROJ_prefix)/share/doc/'${PF}, \
		-e 's,^PROJ_etcdir.*,PROJ_etcdir := '"${EPREFIX}"'/etc/llvm,' \
		-e 's,^PROJ_libdir.*,PROJ_libdir := $(PROJ_prefix)/'$(get_libdir)/llvm, \
		-i Makefile.config.in || die "Makefile.config sed failed"

	einfo "Fixing rpath and CFLAGS"
	sed -e 's,\$(RPATH) -Wl\,\$(\(ToolDir\|LibDir\)),$(RPATH) -Wl\,'"${EPREFIX}"/usr/$(get_libdir)/llvm, \
		-e '/OmitFramePointer/s/-fomit-frame-pointer//' \
		-i Makefile.rules || die "rpath sed failed"

	# Use system llc (from llvm ebuild) for tests
	sed -e "/^llc_props =/s/os.path.join(llvm_tools_dir, 'llc')/'llc'/" \
		-i tools/clang/test/lit.cfg  || die "test path sed failed"

	# User patches
	epatch_user
}

src_configure() {
	local CONF_FLAGS="--enable-shared
		--with-optimize-option=
		$(use_enable !debug optimized)
		$(use_enable debug assertions)
		$(use_enable debug expensive-checks)"

	# Setup the search path to include the Prefix includes
	if use prefix ; then
		CONF_FLAGS="${CONF_FLAGS} \
			--with-c-include-dirs=${EPREFIX}/usr/include:/usr/include"
	fi

	if use multitarget; then
		CONF_FLAGS="${CONF_FLAGS} --enable-targets=all"
	else
		CONF_FLAGS="${CONF_FLAGS} --enable-targets=host,cpp"
	fi

	if use amd64; then
		CONF_FLAGS="${CONF_FLAGS} --enable-pic"
	fi

	# clang prefers clang over gcc, so we may need to force that
	tc-export CC CXX
	econf ${CONF_FLAGS}
}

src_compile() {
	emake VERBOSE=1 KEEP_SYMBOLS=1 REQUIRES_RTTI=1 clang-only
}

src_test() {
	cd "${S}"/tools/clang || die "cd clang failed"

	echo ">>> Test phase [test]: ${CATEGORY}/${PF}"

	testing() {
		if ! emake -j1 VERBOSE=1 test; then
			has test $FEATURES && die "Make test failed. See above for details."
			has test $FEATURES || eerror "Make test failed. See above for details."
		fi
	}
	python_execute_function testing
}

src_install() {
	cd "${S}"/tools/clang || die "cd clang failed"
	emake KEEP_SYMBOLS=1 DESTDIR="${D}" install

	if use static-analyzer ; then
		dobin tools/scan-build/ccc-analyzer
		dosym ccc-analyzer /usr/bin/c++-analyzer
		dobin tools/scan-build/scan-build

		insinto /usr/share/${PN}
		doins tools/scan-build/scanview.css
		doins tools/scan-build/sorttable.js

		cd tools/scan-view || die "cd scan-view failed"
		dobin scan-view
		install-scan-view() {
			insinto "$(python_get_sitedir)"/clang
			doins Reporter.py Resources ScanView.py startfile.py
			touch "${ED}"/"$(python_get_sitedir)"/clang/__init__.py
		}
		python_execute_function install-scan-view
	fi

	# AddressSanitizer symbolizer (currently separate)
	dobin "${S}"/projects/compiler-rt/lib/asan/scripts/asan_symbolize.py

	# Fix install_names on Darwin.  The build system is too complicated
	# to just fix this, so we correct it post-install
	if [[ ${CHOST} == *-darwin* ]] ; then
		for lib in libclang.dylib ; do
			ebegin "fixing install_name of $lib"
			install_name_tool -id "${EPREFIX}"/usr/lib/llvm/${lib} \
				"${ED}"/usr/lib/llvm/${lib}
			eend $?
		done
		for f in usr/bin/{c-index-test,clang} usr/lib/llvm/libclang.dylib ; do
			ebegin "fixing references in ${f##*/}"
			install_name_tool \
				-change "@rpath/libclang.dylib" \
					"${EPREFIX}"/usr/lib/llvm/libclang.dylib \
				-change "@executable_path/../lib/libLLVM-${PV}.dylib" \
					"${EPREFIX}"/usr/lib/llvm/libLLVM-${PV}.dylib \
				-change "${S}"/Release/lib/libclang.dylib \
					"${EPREFIX}"/usr/lib/llvm/libclang.dylib \
				"${ED}"/$f
			eend $?
		done
	fi
}

pkg_postinst() {
	python_mod_optimize clang
}

pkg_postrm() {
	python_mod_cleanup clang
}
