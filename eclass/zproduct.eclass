# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/zproduct.eclass,v 1.28 2011/12/27 17:55:13 fauli Exp $
# Author: Jason Shoemaker <kutsuya@gentoo.org>

# This eclass is designed to streamline the construction of
# ebuilds for new zope products

EXPORT_FUNCTIONS src_install pkg_prerm pkg_postinst pkg_config

DESCRIPTION="This is a zope product"

RDEPEND="net-zope/zope
	app-admin/zprod-manager"

IUSE=""
SLOT="0"
S=${WORKDIR}

ZI_DIR="${ROOT}/var/lib/zope/"
ZP_DIR="${ROOT}/usr/share/zproduct"
DOT_ZFOLDER_FPATH="${ZP_DIR}/${PF}/.zfolder.lst"

zproduct_src_install() {
	## Assume that folders or files that shouldn't be installed
	#  in the zproduct directory have been already been removed.
	## Assume $S set to the parent directory of the zproduct(s).

	debug-print-function ${FUNCNAME} ${*}
	[ -n "${ZPROD_LIST}" ] || die "ZPROD_LIST isn't defined."
	[ -z "${1}" ] && zproduct_src_install all

	# set defaults
	into ${ZP_DIR}
	dodir ${ZP_DIR}/${PF}

	while [ -n "$1" ] ; do
		case ${1} in
			do_zpfolders)
				## Create .zfolders.lst from $ZPROD_LIST.
				debug-print-section do_zpfolders
				for N in ${ZPROD_LIST} ; do
					echo ${N} >> "${D}"/${DOT_ZFOLDER_FPATH}
				done
				;;
			do_docs)
				#*Moves txt docs
				debug-print-section do_docs
				docs_move
				for ZPROD in ${ZPROD_LIST} ; do
					docs_move ${ZPROD}/
				done
				;;
			do_install)
				debug-print-section do_install
				# Copy everything that's left to ${D}${ZP_DIR}
				# modified to not copy ownership (QA)
				cp --recursive --no-dereference --preserve=timestamps,mode,links "${S}"/* "${D}"/${ZP_DIR}/${PF}
				;;
			all)
				debug-print-section all
				zproduct_src_install do_zpfolders do_docs do_install ;;
		esac
		shift
	done
	debug-print "${FUNCNAME}: result is ${RESULT}"
}

docs_move() {
	# if $1 == "/", then this breaks.
	if [ -n "$1" ] ; then
		docinto $1
	else
		docinto /
	fi
	dodoc $1HISTORY.txt $1README{.txt,} $1INSTALL{.txt,} > /dev/null 2>/dev/null
	dodoc $1AUTHORS $1COPYING $1CREDITS.txt $1TODO{.txt,} > /dev/null 2>/dev/null
	dodoc $1LICENSE{.GPL,.txt,} $1CHANGES{.txt,} > /dev/null 2>/dev/null
	dodoc $1DEPENDENCIES.txt $1FAQ.txt $1UPGRADE.txt > /dev/null 2>/dev/null
	for item in ${MYDOC} ; do
		dodoc ${1}${item} > /dev/null 2>/dev/null
	done
}

zproduct_pkg_postinst() {
	#*check for multiple zinstances, if several display install help msg.

	#*Use zprod-update to install this zproduct to the default zinstance.
	debug-print-function ${FUNCNAME} ${*}

	# this is a shared directory, so root should be owner;
	# zprod-manager or whatever is used to copy products into the
	# instances has to take care of setting the right permissions in
	# the target directory

	chown -R root:root ${ZP_DIR}/${PF}
	# make shure there is nothing writable in the new dir, and all is readable
	chmod -R go-w,a+rX ${ZP_DIR}/${PF}

	einfo "Attention: ${PF} was not installed in any instance! Use 'zprod-manager add'"
	#disabled by radek@20061228 - contact me in case of any question!
	#${ROOT}/usr/sbin/zprod-manager add ${ZP_DIR}/${PF}
}

zproduct_pkg_prerm() {
	# checks how many times product is installed and informs about it
	# it does not remove it (change in behaviour done by radek@20061228)
	debug-print-function ${FUNCNAME} ${*}
	ZINST_LST=$(ls /var/lib/zope/)
	if [ "${ZINST_LST}" ] ; then
		# first check and warn on any installed products into instances
		ARE_INSTALLED=0
		for N in ${ZINST_LST} ; do
			if [ -s $DOT_ZFOLDER_FPATH ]
			then
				# check only if installed product has non empty folder lists
				#
				# for every fodler inside product ...
				for PFOLD in `cat $DOT_ZFOLDER_FPATH`
				do
					# ... check if its in instance.
					if [ -d "${ZI_DIR}${N}/Products/${PFOLD}" ]
					then
						ARE_INSTALLED=$[ARE_INSTALLED + 1]
					fi
				done
			fi
		done
		if [ $ARE_INSTALLED -gt 0 ]
		then
			ewarn "Detected at least $ARE_INSTALLED copies of product being unmerged."
			ewarn "Please manually remove it from instances using 'zprod-manager del'"
			ewarn "Product is removed from ${ZP_DIR} but not from instances!"
		fi
	fi
}

zproduct_pkg_config() {
	einfo "To add zproducts to zope instances use:"
	einfo "\tzprod-manager add"
}
