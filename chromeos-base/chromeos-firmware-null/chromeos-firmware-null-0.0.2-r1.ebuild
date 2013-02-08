# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_PROJECT="chromiumos/platform/firmware"

# THIS IS A TEMPLATE EBUILD FILE.
# UNCOMMENT THE 'inherit' LINE TO ACTIVATE AND START YOUR MODIFICATION.

# inherit cros-workon cros-firmware

DESCRIPTION="Chrome OS Firmware (null template)"
# Empty (null) ebuild which satisifies virtual/chromeos-firmware.
# This is a direct dependency of chromeos-base/chromeos, but is expected to
# be overridden in an overlay for each specialized board.  A typical non-null
# implementation will install any board-specific configuration files and
# drivers which are not suitable for inclusion in a generic board overlay.

HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

CROS_WORKON_LOCALNAME="firmware"

# ---------------------------------------------------------------------------
# CUSTOMIZATION SECTION

# Name of user account on the Binary Component Server.
CROS_FIRMWARE_BCS_USER_NAME=""

# System firmware image.
# Examples:
#  CROS_FIRMWARE_MAIN_IMAGE="bcs://filename.tbz2" - Fetch from Binary Component Server.
#  CROS_FIRMWARE_MAIN_IMAGE="file://filename.fd"  - Fetch from "files" directory.
#  CROS_FIRMWARE_MAIN_IMAGE="${ROOT}/lib/firmware/filename.fd" - Absolute file path.
CROS_FIRMWARE_MAIN_IMAGE=""

# EC (embedded controller) firmware.
# Examples:
#  CROS_FIRMWARE_EC_IMAGE="bcs://filename.tbz2" - Fetch from Binary Component Server.
#  CROS_FIRMWARE_EC_IMAGE="file://filename.bin" - Fetch from "files" directory.
#  CROS_FIRMWARE_EC_IMAGE="${ROOT}/lib/firmware/filename.bin" - Absolute file path.
CROS_FIRMWARE_EC_IMAGE=""

# EC (embedded controller) firmware image version identifier.
CROS_FIRMWARE_EC_VERSION=""

# If you need any additional resources in firmware update (ex,
# a customization script like "install_firmware_custom.sh"),
# put the filename or directory name here. Accepts multiple colon delimited
# values.
# Example: CROS_FIRMWARE_EXTRA_LIST="$FILESDIR/a_directory:$FILESDIR/a_file"
CROS_FIRMWARE_EXTRA_LIST=""
