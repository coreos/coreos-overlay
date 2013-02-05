# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS-VARIABLE: CONFLICT_LIST
# @DESCRIPTION:
# Atoms mentioned in CONFLICT_LIST need to either be unmerged or upgraded
# prior to this package being installed, but we don't want to have an explicit
# dependency on that package. So instead, we do the following:
#   1. When we are installed, ensure that the old version is not installed.
#   2. If old version is installed, ask emerge to consider upgrading it.
#      This consideration is listed as PDEPEND so that we don't add an
#      explicit dependency on the other package.
for atom in $CONFLICT_LIST; do
	DEPEND="$DEPEND !!<=$atom"
	RDEPEND="$RDEPEND !!<=$atom"
	PDEPEND="$PDEPEND || ( >$atom !!<=$atom )"
done
