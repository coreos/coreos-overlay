# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/profiles/default/linux/x86/10.0/server/profile.bashrc,v 1.1 2009/08/06 07:29:54 ssuominen Exp $

if [[ "${EBUILD_PHASE}" == "setup" ]]
then
	if [[ ! "${I_KNOW_WHAT_I_AM_DOING}" == "yes" ]]
	then
		ewarn "This profile has not been tested thoroughly and is not considered to be"
		ewarn "a supported server profile at this time.  For a supported server"
		ewarn "profile, please check the Hardened project (http://hardened.gentoo.org)."
		echo
		ewarn "This profile is merely a convenience for people who require a more"
		ewarn "minimal profile, yet are unable to use hardened due to restrictions in"
		ewarn "the software being used on the server. This profile should also be used"
		ewarn "if you require GCC 4.1 or Glibc 2.4 support. If you don't know if this"
		ewarn "applies to you, then it doesn't and you should probably be using"
		ewarn "Hardened, instead."
		echo
	fi
fi
