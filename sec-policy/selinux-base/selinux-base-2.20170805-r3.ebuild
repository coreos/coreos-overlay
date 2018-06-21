# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="6"

inherit coreos-sec-policy systemd

if [[ ${PV} == 9999* ]]; then
	EGIT_REPO_URI="${SELINUX_GIT_REPO:-https://anongit.gentoo.org/git/proj/hardened-refpolicy.git}"
	EGIT_BRANCH="${SELINUX_GIT_BRANCH:-master}"
	EGIT_CHECKOUT_DIR="${WORKDIR}/refpolicy"

	inherit git-r3
else
	SRC_URI="https://raw.githubusercontent.com/wiki/TresysTechnology/refpolicy/files/refpolicy-${PV}.tar.bz2
			https://dev.gentoo.org/~swift/patches/selinux-base-policy/patchbundle-selinux-base-policy-${PVR}.tar.bz2"

	KEYWORDS="amd64 -arm arm64 ~mips x86"
fi

IUSE="doc +open_perms +peer_perms systemd +ubac +unconfined"

DESCRIPTION="Gentoo base policy for SELinux"
HOMEPAGE="https://www.gentoo.org/proj/en/hardened/selinux/"
LICENSE="GPL-2"
SLOT="0"

RDEPEND=">=sys-apps/policycoreutils-2.3
	virtual/udev"
DEPEND="${RDEPEND}
	sys-devel/m4
	>=sys-apps/checkpolicy-2.3"

S=${WORKDIR}/

src_prepare() {
	if [[ ${PV} != 9999* ]]; then
		einfo "Applying SELinux policy updates ... "
		eapply -p0 "${WORKDIR}/0001-full-patch-against-stable-release.patch"
	fi

	# Additional selinux policy for rkt.
	local kte="${S}/refpolicy/policy/modules/kernel/kernel.te"
	#  Read and write files and directories regardless of their category set.
	echo "mcs_file_read_all(kernel_t)" >> "${kte}"
	echo "mcs_file_write_all(kernel_t)" >> "${kte}"
	#  Trusted for setting any category set for the processes it executes.
	echo "mcs_process_set_categories(kernel_t)" >> "${kte}"
	#  Sigkill and sigstop all domains regardless of their category set.
	echo "mcs_killall(kernel_t)" >> "${kte}"
	#  Allowed to ptrace all domains regardless of their category set.
	echo "mcs_ptrace_all(kernel_t)" >> "${kte}"
	#  Quiet wake_alarm AVCs.
	echo "allow kernel_t self:capability2 wake_alarm;" >> "${kte}"

	eapply_user

	cd "${S}/refpolicy" || die
	emake bare
}

src_configure() {
	[ -z "${POLICY_TYPES}" ] && local POLICY_TYPES="targeted strict mls mcs"

	# Update the SELinux refpolicy capabilities based on the users' USE flags.

	if ! use peer_perms; then
		sed -i -e '/network_peer_controls/d' \
			"${S}/refpolicy/policy/policy_capabilities" || die
	fi

	if ! use open_perms; then
		sed -i -e '/open_perms/d' \
			"${S}/refpolicy/policy/policy_capabilities" || die
	fi

	if ! use ubac; then
		sed -i -e '/^UBAC/s/y/n/' "${S}/refpolicy/build.conf" \
			|| die "Failed to disable User Based Access Control"
	fi

	if use systemd; then
		sed -i -e '/^SYSTEMD/s/n/y/' "${S}/refpolicy/build.conf" \
			|| die "Failed to enable SystemD"
	fi

	echo "DISTRO = gentoo" >> "${S}/refpolicy/build.conf" || die

	# Prepare initial configuration
	cd "${S}/refpolicy" || die
	emake conf || die "Make conf failed"

	# Setup the policies based on the types delivered by the end user.
	# These types can be "targeted", "strict", "mcs" and "mls".
	for i in ${POLICY_TYPES}; do
		cp -a "${S}/refpolicy" "${S}/${i}" || die
		cd "${S}/${i}" || die

		#cp "${FILESDIR}/modules-2.20120215.conf" "${S}/${i}/policy/modules.conf"
		sed -i -e "/= module/d" "${S}/${i}/policy/modules.conf" || die

		sed -i -e '/^QUIET/s/n/y/' -e "/^NAME/s/refpolicy/$i/" \
			"${S}/${i}/build.conf" || die "build.conf setup failed."

		if [[ "${i}" == "mls" ]] || [[ "${i}" == "mcs" ]];
		then
			# MCS/MLS require additional settings
			sed -i -e "/^TYPE/s/standard/${i}/" "${S}/${i}/build.conf" \
				|| die "failed to set type to mls"
		fi

		if [ "${i}" == "targeted" ]; then
			sed -i -e '/root/d' -e 's/user_u/unconfined_u/' \
			"${S}/${i}/config/appconfig-standard/seusers" \
			|| die "targeted seusers setup failed."
		fi

		if [ "${i}" != "targeted" ] && [ "${i}" != "strict" ] && use unconfined; then
			sed -i -e '/root/d' -e 's/user_u/unconfined_u/' \
			"${S}/${i}/config/appconfig-${i}/seusers" \
			|| die "policy seusers setup failed."
		fi
	done
}

src_compile() {
	[ -z "${POLICY_TYPES}" ] && local POLICY_TYPES="targeted strict mls mcs"

	for i in ${POLICY_TYPES}; do
		cd "${S}/${i}" || die
		emake base
		if use doc; then
			emake html
		fi
	done
}

src_install() {
	[ -z "${POLICY_TYPES}" ] && local POLICY_TYPES="targeted strict mls mcs"

	for i in ${POLICY_TYPES}; do
		cd "${S}/${i}" || die

		emake DESTDIR="${D}" install \
			|| die "${i} install failed."

		emake DESTDIR="${D}" install-headers \
			|| die "${i} headers install failed."

		echo "run_init_t" > "${D}/etc/selinux/${i}/contexts/run_init_type" || die

		echo "textrel_shlib_t" >> "${D}/etc/selinux/${i}/contexts/customizable_types" || die
	done

	insinto /usr/lib/selinux
	doins "${FILESDIR}/config"

	local tmpfiles_conf="${T}"/selinux-base.conf
	cp "${FILESDIR}"/tmpfiles.d/selinux-base.conf "${tmpfiles_conf}"

	for i in ${POLICY_TYPES}; do
		# relocate policy to /usr
		mv "${D}/etc/selinux/${i}" "${D}/usr/lib/selinux"
		echo "L	/etc/selinux/${i}	-	-	-	-	../../usr/lib/selinux/${i}" \
			>> "${tmpfiles_conf}"

		insinto /usr/lib/selinux/${i}/contexts
		doins "${FILESDIR}/lxc_contexts"
	done

	systemd_dotmpfilesd "${tmpfiles_conf}"
	systemd-tmpfiles --root="${D}" --create selinux-base.conf
}
