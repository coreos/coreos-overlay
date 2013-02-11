# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/selinux-policy-2.eclass,v 1.16 2013/01/26 15:01:52 swift Exp $

# Eclass for installing SELinux policy, and optionally
# reloading the reference-policy based modules.

# @ECLASS: selinux-policy-2.eclass
# @MAINTAINER:
# selinux@gentoo.org
# @BLURB: This eclass supports the deployment of the various SELinux modules in sec-policy
# @DESCRIPTION:
# The selinux-policy-2.eclass supports deployment of the various SELinux modules
# defined in the sec-policy category. It is responsible for extracting the
# specific bits necessary for single-module deployment (instead of full-blown
# policy rebuilds) and applying the necessary patches.
#
# Also, it supports for bundling patches to make the whole thing just a bit more
# manageable.

# @ECLASS-VARIABLE: MODS
# @DESCRIPTION:
# This variable contains the (upstream) module name for the SELinux module.
# This name is only the module name, not the category!
: ${MODS:="_illegal"}

# @ECLASS-VARIABLE: BASEPOL
# @DESCRIPTION:
# This variable contains the version string of the selinux-base-policy package
# that this module build depends on. It is used to patch with the appropriate
# patch bundle(s) that are part of selinux-base-policy.
: ${BASEPOL:=""}

# @ECLASS-VARIABLE: POLICY_PATCH
# @DESCRIPTION:
# This variable contains the additional patch(es) that need to be applied on top
# of the patchset already contained within the BASEPOL variable. The variable
# can be both a simple string (space-separated) or a bash array.
: ${POLICY_PATCH:=""}

# @ECLASS-VARIABLE: POLICY_FILES
# @DESCRIPTION:
# When defined, this contains the files (located in the ebuilds' files/
# directory) which should be copied as policy module files into the store.
# Generally, users would want to include at least a .te and .fc file, but .if
# files are supported as well. The variable can be both a simple string
# (space-separated) or a bash array.
: ${POLICY_FILES:=""}

# @ECLASS-VARIABLE: POLICY_TYPES
# @DESCRIPTION:
# This variable informs the eclass for which SELinux policies the module should
# be built. Currently, Gentoo supports targeted, strict, mcs and mls.
# This variable is the same POLICY_TYPES variable that we tell SELinux
# users to set in /etc/make.conf. Therefor, it is not the module that should
# override it, but the user.
: ${POLICY_TYPES:="targeted strict mcs mls"}

extra_eclass=""
case ${BASEPOL} in
	9999)	extra_eclass="git-2";
			EGIT_REPO_URI="git://git.overlays.gentoo.org/proj/hardened-refpolicy.git";
			EGIT_SOURCEDIR="${WORKDIR}/refpolicy";;
esac

inherit eutils ${extra_eclass}

IUSE=""

HOMEPAGE="http://www.gentoo.org/proj/en/hardened/selinux/"
if [[ -n ${BASEPOL} ]] && [[ "${BASEPOL}" != "9999" ]];
then
	SRC_URI="http://oss.tresys.com/files/refpolicy/refpolicy-${PV}.tar.bz2
		http://dev.gentoo.org/~swift/patches/selinux-base-policy/patchbundle-selinux-base-policy-${BASEPOL}.tar.bz2"
elif [[ "${BASEPOL}" != "9999" ]];
then
	SRC_URI="http://oss.tresys.com/files/refpolicy/refpolicy-${PV}.tar.bz2"
else
	SRC_URI=""
fi

LICENSE="GPL-2"
SLOT="0"
S="${WORKDIR}/"
PATCHBUNDLE="${DISTDIR}/patchbundle-selinux-base-policy-${BASEPOL}.tar.bz2"

# Modules should always depend on at least the first release of the
# selinux-base-policy for which they are generated.
if [[ -n ${BASEPOL} ]];
then
	RDEPEND=">=sys-apps/policycoreutils-2.0.82
		>=sec-policy/selinux-base-policy-${BASEPOL}"
else
	RDEPEND=">=sys-apps/policycoreutils-2.0.82
		>=sec-policy/selinux-base-policy-${PV}"
fi
DEPEND="${RDEPEND}
	sys-devel/m4
	>=sys-apps/checkpolicy-2.0.21"

SELINUX_EXPF="src_unpack src_compile src_install pkg_postinst pkg_postrm"
case "${EAPI:-0}" in
	2|3|4|5) SELINUX_EXPF+=" src_prepare" ;;
	*) ;;
esac

EXPORT_FUNCTIONS ${SELINUX_EXPF}

# @FUNCTION: selinux-policy-2_src_unpack
# @DESCRIPTION:
# Unpack the policy sources as offered by upstream (refpolicy). In case of EAPI
# older than 2, call src_prepare too.
selinux-policy-2_src_unpack() {
	if [[ "${BASEPOL}" != "9999" ]];
	then
		unpack ${A}
	else
		git-2_src_unpack
	fi

	# Call src_prepare explicitly for EAPI 0 or 1
	has "${EAPI:-0}" 0 1 && selinux-policy-2_src_prepare
}

# @FUNCTION: selinux-policy-2_src_prepare
# @DESCRIPTION:
# Patch the reference policy sources with our set of enhancements. Start with
# the base patchbundle referred to by the ebuilds through the BASEPOL variable,
# then apply the additional patches as offered by the ebuild.
#
# Next, extract only those files needed for this particular module (i.e. the .te
# and .fc files for the given module in the MODS variable).
#
# Finally, prepare the build environments for each of the supported SELinux
# types (such as targeted or strict), depending on the POLICY_TYPES variable
# content.
selinux-policy-2_src_prepare() {
	local modfiles
	local add_interfaces=0;

	# Create 3rd_party location for user-contributed policies
	cd "${S}/refpolicy/policy/modules" && mkdir 3rd_party;

	# Patch the sources with the base patchbundle
	if [[ -n ${BASEPOL} ]] && [[ "${BASEPOL}" != "9999" ]];
	then
		cd "${S}"
		EPATCH_MULTI_MSG="Applying SELinux policy updates ... " \
		EPATCH_SUFFIX="patch" \
		EPATCH_SOURCE="${WORKDIR}" \
		EPATCH_FORCE="yes" \
		epatch
	fi

	# Copy additional files to the 3rd_party/ location
	if [[ "$(declare -p POLICY_FILES 2>/dev/null 2>&1)" == "declare -a"* ]] ||
	   [[ -n ${POLICY_FILES} ]];
	then
	    add_interfaces=1;
		cd "${S}/refpolicy/policy/modules"
		for POLFILE in ${POLICY_FILES[@]};
		do
			cp "${FILESDIR}/${POLFILE}" 3rd_party/ || die "Could not copy ${POLFILE} to 3rd_party/ location";
		done
	fi

	# Apply the additional patches refered to by the module ebuild.
	# But first some magic to differentiate between bash arrays and strings
	if [[ "$(declare -p POLICY_PATCH 2>/dev/null 2>&1)" == "declare -a"* ]] ||
	   [[ -n ${POLICY_PATCH} ]];
	then
		cd "${S}/refpolicy/policy/modules"
		for POLPATCH in ${POLICY_PATCH[@]};
		do
			epatch "${POLPATCH}"
		done
	fi

	# Collect only those files needed for this particular module
	for i in ${MODS}; do
		modfiles="$(find ${S}/refpolicy/policy/modules -iname $i.te) $modfiles"
		modfiles="$(find ${S}/refpolicy/policy/modules -iname $i.fc) $modfiles"
		if [ ${add_interfaces} -eq 1 ];
		then
			modfiles="$(find ${S}/refpolicy/policy/modules -iname $i.if) $modfiles"
		fi
	done

	for i in ${POLICY_TYPES}; do
		mkdir "${S}"/${i} || die "Failed to create directory ${S}/${i}"
		cp "${S}"/refpolicy/doc/Makefile.example "${S}"/${i}/Makefile \
			|| die "Failed to copy Makefile.example to ${S}/${i}/Makefile"

		cp ${modfiles} "${S}"/${i} \
			|| die "Failed to copy the module files to ${S}/${i}"
	done
}

# @FUNCTION: selinux-policy-2_src_compile
# @DESCRIPTION:
# Build the SELinux policy module (.pp file) for just the selected module, and
# this for each SELinux policy mentioned in POLICY_TYPES
selinux-policy-2_src_compile() {
	for i in ${POLICY_TYPES}; do
		# Parallel builds are broken, so we need to force -j1 here
		emake -j1 NAME=$i -C "${S}"/${i} || die "${i} compile failed"
	done
}

# @FUNCTION: selinux-policy-2_src_install
# @DESCRIPTION:
# Install the built .pp files in the correct subdirectory within
# /usr/share/selinux.
selinux-policy-2_src_install() {
	local BASEDIR="/usr/share/selinux"

	for i in ${POLICY_TYPES}; do
		for j in ${MODS}; do
			einfo "Installing ${i} ${j} policy package"
			insinto ${BASEDIR}/${i}
			doins "${S}"/${i}/${j}.pp || die "Failed to add ${j}.pp to ${i}"

			if [[ "${POLICY_FILES[@]}" == *"${j}.if"* ]];
			then
				insinto ${BASEDIR}/${i}/include/3rd_party
				doins "${S}"/${i}/${j}.if || die "Failed to add ${j}.if to ${i}"
			fi
		done
	done
}

# @FUNCTION: selinux-policy-2_pkg_postinst
# @DESCRIPTION:
# Install the built .pp files in the SELinux policy stores, effectively
# activating the policy on the system.
selinux-policy-2_pkg_postinst() {
	# build up the command in the case of multiple modules
	local COMMAND
	for i in ${MODS}; do
		COMMAND="-i ${i}.pp ${COMMAND}"
	done

	for i in ${POLICY_TYPES}; do
		einfo "Inserting the following modules into the $i module store: ${MODS}"

		cd /usr/share/selinux/${i} || die "Could not enter /usr/share/selinux/${i}"
		semodule -s ${i} ${COMMAND}
		if [ $? -ne 0 ];
		then
			ewarn "SELinux module load failed. Trying full reload...";
			if [ "${i}" == "targeted" ];
			then
				semodule -s ${i} -b base.pp -i $(ls *.pp | grep -v base.pp);
			else
				semodule -s ${i} -b base.pp -i $(ls *.pp | grep -v base.pp | grep -v unconfined.pp);
			fi
			if [ $? -ne 0 ];
			then
				ewarn "Failed to reload SELinux policies."
				ewarn ""
				ewarn "If this is *not* the last SELinux module package being installed,"
				ewarn "then you can safely ignore this as the reloads will be retried"
				ewarn "with other, recent modules."
				ewarn ""
				ewarn "If it is the last SELinux module package being installed however,"
				ewarn "then it is advised to look at the error above and take appropriate"
				ewarn "action since the new SELinux policies are not loaded until the"
				ewarn "command finished succesfully."
				ewarn ""
				ewarn "To reload, run the following command from within /usr/share/selinux/${i}:"
				ewarn "  semodule -b base.pp -i \$(ls *.pp | grep -v base.pp)"
				ewarn "or"
				ewarn "  semodule -b base.pp -i \$(ls *.pp | grep -v base.pp | grep -v unconfined.pp)"
				ewarn "depending on if you need the unconfined domain loaded as well or not."
			else
				einfo "SELinux modules reloaded succesfully."
			fi
		else
			einfo "SELinux modules loaded succesfully."
		fi
	done
}

# @FUNCTION: selinux-policy-2_pkg_postrm
# @DESCRIPTION:
# Uninstall the module(s) from the SELinux policy stores, effectively
# deactivating the policy on the system.
selinux-policy-2_pkg_postrm() {
	# Only if we are not upgrading
	if [[ "${EAPI}" -lt 4 || -z "${REPLACED_BY_VERSION}" ]];
	then
		# build up the command in the case of multiple modules
		local COMMAND
		for i in ${MODS}; do
			COMMAND="-r ${i} ${COMMAND}"
		done
	
		for i in ${POLICY_TYPES}; do
			einfo "Removing the following modules from the $i module store: ${MODS}"
	
			semodule -s ${i} ${COMMAND}
			if [ $? -ne 0 ];
			then
				ewarn "SELinux module unload failed.";
			else
				einfo "SELinux modules unloaded succesfully."
			fi
		done
	fi
}

