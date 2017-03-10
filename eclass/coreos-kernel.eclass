# Copyright 2013-2014 CoreOS, Inc.
# Copyright 2012 The Chromium OS Authors.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS-VARIABLE: COREOS_SOURCE_REVISION
# @DESCRIPTION:
# Revision of the source ebuild, e.g. -r1. default is ""
: ${COREOS_SOURCE_REVISION:=}

COREOS_SOURCE_VERSION="${PV}${COREOS_SOURCE_REVISION}"
COREOS_SOURCE_NAME="linux-${PV}-coreos${COREOS_SOURCE_REVISION}"

[[ ${EAPI} != "5" ]] && die "Only EAPI=5 is supported"

inherit linux-info toolchain-funcs

HOMEPAGE="http://www.kernel.org"
LICENSE="GPL-2 freedist"
SLOT="0/${PVR}"
SRC_URI=""
IUSE=""

DEPEND="=sys-kernel/coreos-sources-${COREOS_SOURCE_VERSION}"

# Do not analyze or strip installed files
RESTRICT="binchecks strip"

# The build tools are OK and shouldn't trip up multilib-strict.
QA_MULTILIB_PATHS="usr/lib/modules/.*/build/scripts/.*"

# Use source installed by coreos-sources
KERNEL_DIR="${SYSROOT}/usr/src/${COREOS_SOURCE_NAME}"

# Search for an apropriate config in ${FILESDIR}. The config should reflect
# the kernel version but partial matching is allowed if the config is
# applicalbe to multiple ebuilds, such as different -r revisions or stable
# kernel releases. For an amd64 ebuild with version 3.12.4-r2 the order is:
# (uses the portage $ARCH instead of the kernel's for simplicity sake)
#  - amd64_defconfig-3.12.4-r2
#  - amd64_defconfig-3.12.4
#  - amd64_defconfig-3.12
#  - amd64_defconfig
# The first matching config is used, die otherwise.
find_config() {
	local base_path="${FILESDIR}/${1}"
	local try_suffix try_path
	for try_suffix in "-${PVR}" "-${PV}" "-${PV%.*}" ""; do
		try_path="${base_path}${try_suffix}"
		if [[ -f "${try_path}" ]]; then
			echo "${try_path}"
			return
		fi
	done

	die "No ${1} found for ${PVR} in ${FILESDIR}"
}

find_archconfig () {
	path=$(find_config "${ARCH}"_defconfig)
	if [ -z ${path} ]; then
		die "No arch config found for ${PVR} in ${FILESDIR}"
	fi
	echo "${path}"
}

find_commonconfig () {
	path=$(find_config commonconfig)
	if [ -z ${path} ]; then
		die "No common config found for ${PVR} in ${FILESDIR}"
	fi
	echo "${path}"
}

config_update() {
	key="${1%%=*}"
	sed -i -e "/^${key}=/d" build/.config || die
	echo "$1" >> build/.config || die
}

# Get the path to the architecture's kernel image.
kernel_path() {
	local kernel_arch=$(tc-arch-kernel)
	case "${kernel_arch}" in
		arm64)	echo build/arch/arm64/boot/Image;;
		x86)	echo build/arch/x86/boot/bzImage;;
		*)		die "Unsupported kernel arch '${kernel_arch}'";;
	esac
}

# Get the make target to build the kernel image.
kernel_target() {
	local path=$(kernel_path)
	echo "${path##*/}"
}

kmake() {
	local kernel_arch=$(tc-arch-kernel) kernel_cflags=
	if gcc-specs-pie; then
		kernel_cflags="-nopie -fstack-check=no"
	fi
	emake "--directory=${S}/source" \
		ARCH="${kernel_arch}" \
		CROSS_COMPILE="${CHOST}-" \
		KBUILD_OUTPUT="../build" \
		KCFLAGS="${kernel_cflags}" \
		LDFLAGS="" \
		"V=1" \
		"$@"
}

# Prints the value of a given kernel config option.
# Quotes around string values are removed.
getconfig() {
	local value=$(getfilevar_noexec "CONFIG_$1" build/.config)
	[[ -n "${value}" ]] || die "$1 is not in the kernel config"
	[[ "${value}" == '"'*'"' ]] && value="${value:1:-1}"
	echo "${value}"
}

# Generate the module signing key for this build.
setup_keys() {
	local sig_hash sig_key
	sig_hash=$(getconfig MODULE_SIG_HASH)
	sig_key="build/$(getconfig MODULE_SIG_KEY)"

	if [[ "${sig_key}" == "build/certs/signing_key.pem" ]]; then
		die "MODULE_SIG_KEY is using the default value"
	fi

	mkdir -p certs "${sig_key%/*}" || die

	# based on the default config the kernel auto-generates
	cat >certs/modules.cnf <<-EOF
		[ req ]
		default_bits = 4096
		distinguished_name = req_distinguished_name
		prompt = no
		string_mask = utf8only
		x509_extensions = myexts

		[ req_distinguished_name ]
		O = CoreOS, Inc
		CN = Module signing key for ${KV_FULL}

		[ myexts ]
		basicConstraints=critical,CA:FALSE
		keyUsage=digitalSignature
		subjectKeyIdentifier=hash
		authorityKeyIdentifier=keyid
	EOF
	openssl req -new -nodes -utf8 -days 36500 -batch -x509 \
		"-${sig_hash}" -outform PEM \
		-config certs/modules.cnf \
		-out certs/modules.pub.pem \
		-keyout certs/modules.key.pem \
		|| die "Generating module signing key failed"
	cat certs/modules.pub.pem certs/modules.key.pem > "${sig_key}"
}

# Discard the module signing key but keep public certificate.
shred_keys() {
	local sig_key
	sig_key="build/$(getconfig MODULE_SIG_KEY)"
	shred -u certs/modules.key.pem "${sig_key}" || die
	cp certs/modules.pub.pem "${sig_key}" || die
}

# Populate /lib/modules/$(uname -r)/{build,source}
install_build_source() {
	local kernel_arch=$(tc-arch-kernel)

	# remove the broken symlinks referencing $ROOT
	rm "${D}/usr/lib/modules/${KV_FULL}"/{build,source} || die

	# Install a stripped source for out-of-tree module builds (Debian-derived)
	{
		echo source/Makefile
		find source/arch/${kernel_arch} -follow -maxdepth 1 -name 'Makefile*' -print
		find source/arch/${kernel_arch} -follow \( -name 'module.lds' -o -name 'Kbuild.platforms' -o -name 'Platform' \) -print
		find $(find source/arch/${kernel_arch} -follow \( -name include -o -name scripts \) -follow -type d -print) -print
		find source/include source/scripts -follow -print
		find build/ -print
	} | cpio -pd \
		--preserve-modification-time \
		--owner=root:root \
		--dereference \
		"${D}/usr/lib/modules/${KV_FULL}" || die
}

coreos-kernel_pkg_pretend() {
	[[ "${MERGE_TYPE}" == binary ]] && return

	if [[ -f "${KERNEL_DIR}/.config" || -d "${KERNEL_DIR}/include/config" ]]
	then
		die "Source is not clean! Run make mrproper in ${KERNEL_DIR}"
	fi
}

coreos-kernel_pkg_setup() {
	[[ "${MERGE_TYPE}" == binary ]] && return

	# tc-arch-kernel requires a call to get_version from linux-info.eclass
	get_version || die "Failed to detect kernel version in ${KERNEL_DIR}"
}

coreos-kernel_src_unpack() {
	# we more or less reproduce the layout in /lib/modules/$(uname -r)/
	mkdir -p "${S}/build" || die
	mkdir -p "${S}/source" || die
	ln -s "${KERNEL_DIR}"/* "${S}/source/" || die
}

coreos-kernel_src_configure() {
	# Use default for any options not explitly set in defconfig
	kmake olddefconfig

	# Verify that olddefconfig has not converted any y or m options to n
	# (implying a new, disabled dependency). Allow options to be converted
	# from m to y.
	#
	# generate regexes from enabled boolean/tristate options |
	#	filter them out of the defconfig |
	#	filter for boolean/tristate options, and format |
	#	sort (why not)
	local missing=$( \
		gawk -F = '/=[ym]$/ {print "^" $1 "="}' "${S}/build/.config" | \
		grep -vf - "${S}/build/.config.old" | \
		gawk -F = '/=[ym]$/ {print "    " $1}' | \
		sort)
	if [[ -n "${missing}" ]]; then
		die "Requested options not enabled in build:\n${missing}"
	fi

	# For convenience, generate a minimal defconfig of the build
	kmake savedefconfig
}

EXPORT_FUNCTIONS pkg_pretend pkg_setup src_unpack src_configure
