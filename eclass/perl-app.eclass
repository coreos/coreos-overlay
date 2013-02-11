# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/perl-app.eclass,v 1.12 2011/12/27 17:55:12 fauli Exp $

# Author: Michael Cummings <mcummings@gentoo.org>
# Maintained by the Perl herd <perl@gentoo.org>

inherit perl-module

case "${EAPI:-0}" in
	0|1) EXPORT_FUNCTIONS src_compile ;;
	2)   EXPORT_FUNCTIONS src_configure src_compile ;;
esac

perl-app_src_prep() {
	perl-app_src_configure
}

perl-app_src_configure() {
	perl-module_src_configure
}

perl-app_src_compile() {
	has "${EAPI:-0}" 0 1 && perl-app_src_prep
	perl-module_src_compile
}
