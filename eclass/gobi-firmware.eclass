# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Install Gobi firmware for Chromium OS
#

# @ECLASS-VARIABLE: GOBI_FIRMWARE_OEM
# @DESCRIPTION:
# OEM name for firmware to install
: ${GOBI_FIRMWARE_OEM:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_VID
# @DESCRIPTION:
# OEM Vendor ID
: ${GOBI_FIRMWARE_VID:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_PID
# @DESCRIPTION:
# OEM Product ID
: ${GOBI_FIRMWARE_PID:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_CARRIERS
# @DESCRIPTION:
# Install firmware for this list of carrier numbers
: ${GOBI_FIRMWARE_CARRIERS:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_ZIP_FILE
# @DESCRIPTION:
# Filename of zip file containing firmware
: ${GOBI_FIRMWARE_ZIP_FILE:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_FLAVOR
# @DESCRIPTION:
# The flavor (gps, xtra) to install
: ${GOBI_FIRMWARE_FLAVOR:="gps"}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_QDL
# @DESCRIPTION:
# Install the qdl program from the firmware zip file
: ${GOBI_FIRMWARE_QDL:="no"}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_DEFAULT_CARRIER
# @DESCRIPTION:
# Default carrier firmware to load if not set on modem
: ${GOBI_FIRMWARE_DEFAULT_CARRIER:=}

# @ECLASS-VARIABLE: GOBI_FIRMWARE_APPS_DIR
# @DESCRIPTION:
# directory name for the .apps files
: ${GOBI_FIRMWARE_APPS_DIR:=""}

GOBI_FIRMWARE_CARRIER_VOD=0
GOBI_FIRMWARE_CARRIER_VZW=1
GOBI_FIRMWARE_CARRIER_ATT=2
GOBI_FIRMWARE_CARRIER_SPRINT=3
GOBI_FIRMWARE_CARRIER_TMO=4
GOBI_FIRMWARE_CARRIER_GEN=6
GOBI_FIRMWARE_CARRIER_TELLFON=7
GOBI_FIRMWARE_CARRIER_TELITAL=8
GOBI_FIRMWARE_CARRIER_ORANGE=9
GOBI_FIRMWARE_CARRIER_DOCO=12
GOBI_FIRMWARE_CARRIER_DELLX=15
GOBI_FIRMWARE_CARRIER_OMH=16

# Check for EAPI 2+
case "${EAPI:-0}" in
	4|3|2) ;;
	*) die "unsupported EAPI" ;;
esac

gobi-firmware_install_udev_qcserial_rules() {
   local oem=${GOBI_FIRMWARE_OEM}
   local vid=${GOBI_FIRMWARE_VID}
   local pid=${GOBI_FIRMWARE_PID}
   local file=/etc/udev/rules.d/90-ttyusb-qcserial-${oem}.rules
   cat > ${D}${file} <<EOF
# 90-ttyusb-qcserial-${oem}.rules
# Sets ownership of Gobi ttyusb devices belonging to qcserial.

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ACTION!="add", GOTO="ttyusb_qcserial_${oem}_end"
SUBSYSTEM!="tty", GOTO="ttyusb_qcserial_${oem}_end"
KERNEL!="ttyUSB[0-9]*", GOTO="ttyusb_qcserial_${oem}_end"

ATTRS{idVendor}=="${vid}", ATTRS{idProduct}=="${pid}", \
  OWNER="qdlservice", GROUP="qdlservice"

LABEL="ttyusb_qcserial_${oem}_end"

EOF
}

gobi-firmware_install_udev_qdlservice_rules() {
   local oem=${GOBI_FIRMWARE_OEM}
   local vid=${GOBI_FIRMWARE_VID}
   local pid=${GOBI_FIRMWARE_PID}
   local file=/etc/udev/rules.d/99-qdlservice-${oem}.rules
   cat > ${D}${file} <<EOF
# 99-qdlservice-${oem}.rules
# Emits a signal in response to a Gobi serial device appearing. Upstart will run
# QDLService when it sees this signal.

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ACTION!="add", GOTO="qdlservice_${oem}_end"
SUBSYSTEM!="tty", GOTO="qdlservice_${oem}_end"
KERNEL!="ttyUSB[0-9]*", GOTO="qdlservice_${oem}_end"

ATTRS{idVendor}=="${vid}", ATTRS{idProduct}=="${pid}", \
  RUN+="/sbin/initctl emit gobi_serial_${oem} GOBIDEV=/dev/%k"

LABEL="qdlservice_${oem}_end"
EOF
}

gobi-firmware_install_udev_rules() {
  dodir /etc/udev/rules.d
  gobi-firmware_install_udev_qcserial_rules
  gobi-firmware_install_udev_qdlservice_rules
}

gobi-firmware_install_upstart_scripts() {
  dodir /etc/init
  file=/etc/init/qdlservice-${GOBI_FIRMWARE_OEM}.conf

  cat > ${D}${file} <<EOF

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Starts QDLService if a Gobi ttyusb device appears.

start on gobi_serial_${GOBI_FIRMWARE_OEM}

script
  set +e
  GOBIQDL="/opt/Qualcomm/QDLService2k/QDLService2k${GOBI_FIRMWARE_OEM}"
  ret=1
  attempt=0
  readonly MAX_ATTEMPTS=10
  while [ \$ret -ne 0 -a \$attempt -lt \$MAX_ATTEMPTS ]; do
		# Exponential backoff - wait (2^attempt) - 1 seconds
    sleep \$(((1 << \$attempt) - 1))
    starttime=\$(date +%s%N)
    /sbin/minijail0 -u qdlservice -g qdlservice -- "\$GOBIQDL" "\$GOBIDEV"
    ret=\$?
    endtime=\$(date +%s%N)
    logger -t qdlservice "attempt \$attempt: \$ret"
    attempt=\$((\$attempt + 1))
    if [ \$ret -ne 0 ]; then
      logger -t qdlservice "resetting..."
      /opt/Qualcomm/bin/powercycle-all-gobis
    fi
  done
  download_time=\$(((\$endtime - \$starttime) / 1000000))
  METRICROOT=Network.3G.Gobi.FirmwareDownload
  metrics_client \$METRICROOT.Time \$download_time 0 10000 20
  metrics_client -e \$METRICROOT.Attempts \$attempt \$MAX_ATTEMPTS
  exit \$ret
end script
EOF
}

gobi-firmware_install_firmware_files() {
  local oem=${GOBI_FIRMWARE_OEM}
  local install_qdl=${GOBI_FIRMWARE_QDL}
  local apps_dir=${GOBI_FIRMWARE_APPS_DIR}

  # If the apps directory is not sepcified, then use the carrier
  # directory.  The apps directory should be set to UMTS for most
  # UMTS carriers because they share the same firmware
  if [ -z "${apps_dir}" ] ; then
    apps_dir=${GOBI_FIRMWARE_DEFAULT_CARRIER}
  fi

  #
  # installation directories.
  # We could consider installing to more standard locations
  # except that QDLService expects to find files in
  # /opt/Qualcomm.
  #
  local firmware_install_dir=${D}/opt/Qualcomm/Images2k
  local qdl_install_dir=${D}/opt/Qualcomm/QDLService2k
  local log_install_dir=${D}/var/log/
  local oemlog_filename=QDLService2k${oem}.txt
  local log_filename=QDLService2k.txt

  if [ -d Images2k/${oem} ] ; then
    # We already have the firmware extracted, this is easy
    local base_firmware=Images2k/${oem}

    # Do not install qdl it will be build with SDK
    install_qdl="no"
  else
    [ -z "${GOBI_FIRMWARE_ZIP_FILE}" ] && \
      die "Must specify GOBI_FIRMWARE_ZIP_FILE"
    [ ! -r "${GOBI_FIRMWARE_ZIP_FILE}" ] && \
      die "${GOBI_FIRMWARE_ZIP_FILE} is unreadable"
    mkdir -p "${T}/${oem}"
    unzip ${GOBI_FIRMWARE_ZIP_FILE} -d "${T}/${oem}"

    if [ -d "${T}/${oem}/Images2k/${oem}" ] ; then
      local base_firmware="${T}/${oem}/Images2k/${oem}"
      install_qdl=no
    else
      rpmfile=$(find "${T}/${oem}" -name \*.rpm -print)
      [ -z $rpmfile ] &&
      	die "Could not find an RPM file in ${GOBI_FIRMWARE_ZIP_FILE}"

      # extract the rpm
      if [ -d ${oem}_rpm ] ; then
      	rm -rf ${oem}_rpm
      fi
      mkdir -p ${oem}_rpm
      rpm2tar -O $rpmfile | tar -C ${oem}_rpm -xvf -

      local base_firmware=${oem}_rpm/opt/Qualcomm/Images2k/${oem}
    fi
  fi

  # make directories
  install -d ${firmware_install_dir}/${oem} \
    ${qdl_install_dir} ${udev_rules_install_dir}

  # install firmware
  local flavor_firmware=${base_firmware}_${GOBI_FIRMWARE_FLAVOR}
  for carrier in ${GOBI_FIRMWARE_CARRIERS} UMTS ; do
    # copy the base firmware
    cp -af ${base_firmware}/${carrier} ${firmware_install_dir}/${oem}
    if [ -d ${flavor_firmware}/${carrier} ] ; then
      # overlay spefic xtra/gps flavor files
      cp -af ${flavor_firmware}/${carrier} ${firmware_install_dir}/${oem}
    fi
  done

  # Copy DID file for this device
  cp ${base_firmware}/*.did ${firmware_install_dir}/${oem}

  # Create a DID file for generic GOBI devices
  did_file=$(ls ${base_firmware}/*.did | head -n1)
  if [ ! -x $did_file ] ; then
    # TODO(jglasgow): Move code for 05c6920b to dogfood ebuild
    cp $did_file ${firmware_install_dir}/${oem}/05c6920b.did
  fi

  # Set firmware and directory permissions
  find ${firmware_install_dir}/${oem} -type f -exec chmod 444 {} \;
  find ${firmware_install_dir}/${oem} -type d -exec chmod 555 {} \;

  # install firmware download program, and associated files
  if [ ${install_qdl} == "yes" ] ; then
    local qdl_dir=${oem}_rpm/opt/Qualcomm/QDLService2k
    install -t ${qdl_install_dir} \
      ${qdl_dir}/QDLService2k${oem}

    ln -sf /opt/Qualcomm/QDLService2k/QDLService2k${oem} \
      ${qdl_install_dir}/QDLService2kGeneric
  fi

  # Ensure the default firmware files exists and create Options${oem}.txt
  local image_dir=/opt/Qualcomm/Images2k/${oem}
  local amss_file=${image_dir}/${apps_dir}/amss.mbn
  local apps_file=${image_dir}/${apps_dir}/apps.mbn
  local uqcn_file=${image_dir}/${GOBI_FIRMWARE_DEFAULT_CARRIER}/uqcn.mbn
  for file in $amss_file $apps_file $uqcn_file ; do
    if [ ! -r ${D}${file} ] ; then
      die "Could not find file: $file in ${D}"
    fi
  done

  cat > Options2k${oem}.txt <<EOF
${amss_file}
${apps_file}
${uqcn_file}
EOF
  install -t ${qdl_install_dir} Options2k${oem}.txt
}

gobi-firmware_src_install() {
  # Verify that eclass variables are set
  [ -z "${GOBI_FIRMWARE_DEFAULT_CARRIER}" ] && \
    die "Must specify GOBI_FIRMWARE_DEFAULT_CARRIER"
  [ -z "${GOBI_FIRMWARE_OEM}" ] && \
    die "Must specify GOBI_FIRMWARE_OEM"
  [ -z "${GOBI_FIRMWARE_VID}" ] && \
    die "Must specify GOBI_FIRMWARE_VID"
  [ -z "${GOBI_FIRMWARE_PID}" ] && \
    die "Must specify GOBI_FIRMWARE_PID"
  [ -z "${GOBI_FIRMWARE_CARRIERS}" ] &&
    die "Must specify GOBI_FIRMWARE_CARRIERS"

  gobi-firmware_install_udev_rules
  gobi-firmware_install_upstart_scripts
  gobi-firmware_install_firmware_files
}

EXPORT_FUNCTIONS src_install
