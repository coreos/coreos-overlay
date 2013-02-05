# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Install Gobi firmware for Chromium OS
# See:
# https://sites.google.com/a/google.com/chromeos/for-team-members/systems/gobi3kfirmwaretarball

inherit cros-binary

# @ECLASS-VARIABLE: GOBI_FIRMWARE_VID
# @DESCRIPTION:
# OEM Vendor ID
: ${GOBI_FIRMWARE_VID:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_PID
# @DESCRIPTION:
# OEM Product ID
: ${GOBI_FIRMWARE_PID:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_HASH
# @DESCRIPTION:
# Tarball hash
: ${GOBI_FIRMWARE_HASH:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_CARRIERS
# @DESCRIPTION:
# Install firmware for this list of carrier names.
: ${GOBI_FIRMWARE_CARRIERS:=}

# Check for EAPI 2+
case "${EAPI:-0}" in
	4|3|2) ;;
	*) die "unsupported EAPI" ;;
esac

gobi3k-firmware_src_unpack() {
  mkdir -p ${S}
  local fn="${PN}-${GOBI_FIRMWARE_VID}:${GOBI_FIRMWARE_PID}"
        fn="${fn}-${GOBI_FIRMWARE_HASH}.tar.bz2"
  CROS_BINARY_URI="${URI_BASE}/${fn}"
  cros-binary_src_unpack
  local target="${CROS_BINARY_STORE_DIR}/${fn}"
  cp $target "${S}"
}

gobi3k-firmware_install_firmware_files() {
  local vid=${GOBI_FIRMWARE_VID}
  local pid=${GOBI_FIRMWARE_PID}
  local hash=${GOBI_FIRMWARE_HASH}
  local fwid=${vid}:${pid}
  local firmware_install_dir=${D}/opt/Qualcomm/firmware/${fwid}

  mkdir -p firmware
  # tar rudely interprets x:y as a host:path specifier (!?) and tries to ssh
  # to it on your behalf (!!), so...
  tar -C firmware -xvj < gobi3k-firmware-${fwid}-${hash}.tar.bz2
  local base_firmware=firmware/${fwid}

  install -d ${firmware_install_dir} ${udev_rules_install_dir}

  for carrier in ${GOBI_FIRMWARE_CARRIERS} UMTS ; do
    cp -af ${base_firmware}/${carrier} ${firmware_install_dir}
  done

  find ${firmware_install_dir}/${oem} -type f -exec chmod 444 {} \;
  find ${firmware_install_dir}/${oem} -type d -exec chmod 555 {} \;
}

gobi3k-firmware_src_install() {
  # Verify that eclass variables are set
  [ -z "${GOBI_FIRMWARE_VID}" ] && die "Must specify GOBI_FIRMWARE_VID"
  [ -z "${GOBI_FIRMWARE_PID}" ] && die "Must specify GOBI_FIRMWARE_PID"
  [ -z "${GOBI_FIRMWARE_HASH}" ] && die "Must specify GOBI_FIRMWARE_HASH"
  [ -z "${GOBI_FIRMWARE_CARRIERS}" ] &&
    die "Must specify GOBI_FIRMWARE_CARRIERS"

  gobi3k-firmware_install_firmware_files
}

EXPORT_FUNCTIONS src_unpack src_install
