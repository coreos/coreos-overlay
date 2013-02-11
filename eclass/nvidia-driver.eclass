# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/nvidia-driver.eclass,v 1.18 2012/11/27 17:39:48 jer Exp $

# @ECLASS: nvidia-driver.eclass
# @MAINTAINER:
# chainsaw@gentoo.org
# @AUTHOR:
# Original author: Doug Goldstein <cardoe@gentoo.org>
# @BLURB: Provide useful messages for nvidia-drivers based on currently installed Nvidia card
# @DESCRIPTION:
# Provide useful messages for nvidia-drivers based on currently installed Nvidia
# card. It inherits versionator.

inherit versionator

DEPEND="sys-apps/pciutils"

# the data below is derived from
# http://us.download.nvidia.com/XFree86/Linux-x86_64/177.13/README/appendix-a.html

drv_96xx="0110 0111 0112 0113 0170 0171 0172 0173 0174 0175 0176 0177 0178 \
0179 017a 017c 017d 0181 0182 0183 0185 0188 018a 018b 018c 01a0 01f0 0200 \
0201 0202 0203 0250 0251 0253 0258 0259 025b 0280 0281 0282 0286 0288 0289 \
028c"

drv_71xx="0020 0028 0029 002c 002d 00a0 0100 0101 0103 0150 0151 0152 0153"

drv_173x="00fa 00fb 00fc 00fd 00fe 0301 0302 0308 0309 0311 0312 0314 031a \
031b 031c 0320 0321 0322 0323 0324 0325 0326 0327 0328 032a 032b 032c 032d \
0330 0331 0332 0333 0334 0338 033f 0341 0342 0343 0344 0347 0348 034c 034e"

drv_304x="0040 0041 0042 0043 0044 0045 0046 0047 0048 004e 0090 0091 0092 \
0093 0095 0098 0099 009d 00c0 00c1 00c2 00c3 00c8 00c9 00cc 00cd 00ce 00f1 \
00f2 00f3 00f4 00f5 00f6 00f8 00f9 0140 0141 0142 0143 0144 0145 0146 0147 \
0148 0149 014a 014c 014d 014e 014f 0160 0161 0162 0163 0164 0165 0166 0167 \
0168 0169 016a 01d0 01d1 01d2 01d3 01d6 01d7 01d8 01da 01db 01dc 01dd 01de \
01de 01df 0211 0212 0215 0218 0221 0222 0240 0241 0242 0244 0245 0247 0290 \
0291 0292 0293 0294 0295 0297 0298 0299 029a 029b 029c 029d 029e 029f 02e0 \
02e1 02e2 02e3 02e4 038b 0390 0391 0392 0393 0394 0395 0397 0398 0399 039c \
039e 03d0 03d1 03d2 03d5 03d6 0531 0533 053a 053b 053e 07e0 07e1 07e2 07e3 \
07e5"

mask_96xx=">=x11-drivers/nvidia-drivers-97.0.0"
mask_71xx=">=x11-drivers/nvidia-drivers-72.0.0"
mask_173x=">=x11-drivers/nvidia-drivers-177.0.0"
mask_304x=">=x11-drivers/nvidia-drivers-305.0.0"

# @FUNCTION: nvidia-driver-get-card
# @DESCRIPTION:
# Retrieve the PCI device ID for each Nvidia video card you have
nvidia-driver-get-card() {
	local NVIDIA_CARD="$(/usr/sbin/lspci -d 10de: -n | \
	awk '/ 0300: /{print $3}' | cut -d: -f2 | tr '\n' ' ')"

	if [ -n "$NVIDIA_CARD" ]; then
		echo "$NVIDIA_CARD";
	else
		echo "0000";
	fi
}

nvidia-driver-get-mask() {
	local NVIDIA_CARDS="$(nvidia-driver-get-card)"
	for card in $NVIDIA_CARDS; do
		for drv in $drv_96xx; do
			if [ "x$card" = "x$drv" ]; then
				echo "$mask_96xx";
				return 0;
			fi
		done

		for drv in $drv_71xx; do
			if [ "x$card" = "x$drv" ]; then
				echo "$mask_71xx";
				return 0;
			fi
		done

		for drv in $drv_173x; do
			if [ "x$card" = "x$drv" ]; then
				echo "$mask_173x";
				return 0;
			fi
		done

		for drv in $drv_304x; do
			if [ "x$card" = "x$drv" ]; then
				echo "$mask_304x";
				return 0;
			fi
		done
	done

	echo "";
	return 1;
}

# @FUNCTION: nvidia-driver-check-warning
# @DESCRIPTION:
# Prints out a warning if the driver does not work w/ the installed video card
nvidia-driver-check-warning() {
	local NVIDIA_MASK="$(nvidia-driver-get-mask)"
	if [ -n "$NVIDIA_MASK" ]; then
		version_compare "${NVIDIA_MASK##*-}" "${PV}"
		r=$?

		if [ "x$r" = "x1" ]; then
			ewarn "***** WARNING *****"
			ewarn
			ewarn "You are currently installing a version of nvidia-drivers that is"
			ewarn "known not to work with a video card you have installed on your"
			ewarn "system. If this is intentional, please ignore this. If it is not"
			ewarn "please perform the following steps:"
			ewarn
			ewarn "Add the following mask entry to /etc/portage/package.mask by"
			if [ -d "${ROOT}/etc/portage/package.mask" ]; then
				ewarn "echo \"$NVIDIA_MASK\" > /etc/portage/package.mask/nvidia-drivers"
			else
				ewarn "echo \"$NVIDIA_MASK\" >> /etc/portage/package.mask"
			fi
			ewarn
			ewarn "Failure to perform the steps above could result in a non-working"
			ewarn "X setup."
			ewarn
			ewarn "For more information please read:"
			ewarn "http://www.nvidia.com/object/IO_32667.html"
		fi
	fi
}
