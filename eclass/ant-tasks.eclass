# Eclass for building dev-java/ant-* packages
#
# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License, v2 or later
# Author Vlastimil Babka <caster@gentoo.org>
# $Header: /var/cvsroot/gentoo-x86/eclass/ant-tasks.eclass,v 1.13 2012/06/01 12:19:42 sera Exp $

# we set ant-core dep ourselves, restricted
JAVA_ANT_DISABLE_ANT_CORE_DEP=true
# rewriting build.xml for are the testcases has no reason atm
JAVA_PKG_BSFIX_ALL=no
inherit versionator java-pkg-2 java-ant-2

EXPORT_FUNCTIONS src_unpack src_compile src_install

# -----------------------------------------------------------------------------
# @eclass-begin
# @eclass-shortdesc Eclass for building dev-java/ant-* packages
# @eclass-maintainer java@gentoo.org
#
# This eclass provides functionality and default ebuild variables for building
# dev-java/ant-* packages easily.
#
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-preinherit ANT_TASK_JDKVER
# @variable-default 1.4
#
# Affects the >=virtual/jdk version set in DEPEND string. Defaults to 1.4, can
# be overriden from ebuild BEFORE inheriting this eclass.
# -----------------------------------------------------------------------------
ANT_TASK_JDKVER=${ANT_TASK_JDKVER-1.4}

# -----------------------------------------------------------------------------
# @variable-preinherit ANT_TASK_JREVER
# @variable-default 1.4
#
# Affects the >=virtual/jre version set in DEPEND string. Defaults to 1.4, can
# be overriden from ebuild BEFORE inheriting this eclass.
# -----------------------------------------------------------------------------
ANT_TASK_JREVER=${ANT_TASK_JREVER-1.4}

# -----------------------------------------------------------------------------
# @variable-internal ANT_TASK_NAME
# @variable-default the rest of $PN after "ant-"
#
# The name of this ant task as recognized by ant's build.xml, derived from $PN.
# -----------------------------------------------------------------------------
ANT_TASK_NAME="${PN#ant-}"

# -----------------------------------------------------------------------------
# @variable-preinherit ANT_TASK_DEPNAME
# @variable-default $ANT_TASK_NAME
#
# Specifies JAVA_PKG_NAME (PN{-SLOT} used with java-pkg_jar-from) of the package
# that this one depends on. Defaults to the name of ant task, ebuild can
# override it before inheriting this eclass.
# -----------------------------------------------------------------------------
ANT_TASK_DEPNAME=${ANT_TASK_DEPNAME-${ANT_TASK_NAME}}

# -----------------------------------------------------------------------------
# @variable-preinherit ANT_TASK_DISABLE_VM_DEPS
# @variable-default unset
#
# If set, no JDK/JRE deps are added.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-internal ANT_TASK_PV
# @variable-default Just the number in $PV without any beta/RC suffixes
#
# Version of ant-core this task is intended to register and thus load with.
# -----------------------------------------------------------------------------
ANT_TASK_PV="${PV}"

# special care for beta/RC releases
if [[ ${PV} == *beta2* ]]; then
	MY_PV=${PV/_beta2/beta}
	UPSTREAM_PREFIX="http://people.apache.org/dist/ant/v1.7.1beta2/src"
	GENTOO_PREFIX="http://dev.gentoo.org/~caster/distfiles"
	ANT_TASK_PV=$(get_version_component_range 1-3)
elif [[ ${PV} == *_rc* ]]; then
	MY_PV=${PV/_rc/RC}
	UPSTREAM_PREFIX="http://dev.gentoo.org/~caster/distfiles"
	GENTOO_PREFIX="http://dev.gentoo.org/~caster/distfiles"
	ANT_TASK_PV=$(get_version_component_range 1-3)
else
	# default for final releases
	MY_PV=${PV}
	UPSTREAM_PREFIX="mirror://apache/ant/source"
	case ${PV} in
	1.8.4)
		GENTOO_PREFIX="http://dev.gentoo.org/~sera/distfiles"
		;;
	*)
		GENTOO_PREFIX="http://dev.gentoo.org/~caster/distfiles"
		;;
	esac
fi

# source/workdir name
MY_P="apache-ant-${MY_PV}"

# -----------------------------------------------------------------------------
# Default values for standard ebuild variables, can be overriden from ebuild.
# -----------------------------------------------------------------------------
DESCRIPTION="Apache Ant's optional tasks depending on ${ANT_TASK_DEPNAME}"
HOMEPAGE="http://ant.apache.org/"
SRC_URI="${UPSTREAM_PREFIX}/${MY_P}-src.tar.bz2
	${GENTOO_PREFIX}/ant-${PV}-gentoo.tar.bz2"
LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="~dev-java/ant-core-${PV}"
DEPEND="${RDEPEND}"

if [[ -z "${ANT_TASK_DISABLE_VM_DEPS}" ]]; then
	RDEPEND+=" >=virtual/jre-${ANT_TASK_JREVER}"
	DEPEND+=" >=virtual/jdk-${ANT_TASK_JDKVER}"
fi

# we need direct blockers with old ant-tasks for file collisions - bug #252324
if version_is_at_least 1.7.1 ; then
	DEPEND+=" !dev-java/ant-tasks"
fi

# Would run the full ant test suite for every ant task
RESTRICT="test"

S="${WORKDIR}/${MY_P}"

# ------------------------------------------------------------------------------
# @eclass-src_unpack
#
# Is split into two parts, defaults to both of them ('all').
# base: performs the unpack, build.xml replacement and symlinks ant.jar from
#	ant-core
# jar-dep: symlinks the jar file(s) from dependency package
# ------------------------------------------------------------------------------
ant-tasks_src_unpack() {
	[[ -z "${1}" ]] && ant-tasks_src_unpack all

	while [[ -n "${1}" ]]; do
		case ${1} in
			base)
				unpack ${A}
				cd "${S}"

				# replace build.xml with our modified for split building
				mv -f "${WORKDIR}"/build.xml .

				cd lib
				# remove bundled xerces
				rm -f *.jar

				# ant.jar to build against
				java-pkg_jar-from --build-only ant-core ant.jar;;
			jar-dep)
				# get jar from the dependency package
				if [[ -n "${ANT_TASK_DEPNAME}" ]]; then
					java-pkg_jar-from ${ANT_TASK_DEPNAME}
				fi;;
			all)
				ant-tasks_src_unpack base jar-dep;;
		esac
		shift
	done

}

# ------------------------------------------------------------------------------
# @eclass-src_compile
#
# Compiles the jar with installed ant-core.
# ------------------------------------------------------------------------------
ant-tasks_src_compile() {
	ANT_TASKS="none" eant -Dbuild.dep=${ANT_TASK_NAME} jar-dep
}

# ------------------------------------------------------------------------------
# @eclass-src_install
#
# Installs the jar and registers its presence for the ant launcher script.
# Version param ensures it won't get loaded (thus break) when ant-core is
# updated to newer version.
# ------------------------------------------------------------------------------
ant-tasks_src_install() {
	java-pkg_dojar build/lib/${PN}.jar
	java-pkg_register-ant-task --version "${ANT_TASK_PV}"

	# create the compatibility symlink
	if version_is_at_least 1.7.1_beta2; then
		dodir /usr/share/ant/lib
		dosym /usr/share/${PN}/lib/${PN}.jar /usr/share/ant/lib/${PN}.jar
	fi
}
