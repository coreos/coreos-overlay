#!/bin/bash
# Update patches from directory containing `git format-patch` output and
# rename ebuild for new PVR.

set -e

new_pvr="$1"
srcdir="$(realpath $2)"

if [[ -z "${new_pvr}" || -z "${srcdir}" ]]; then
    echo "Usage:   $0 <new-PVR> <dir-with-git-format-patch-output>"
    echo "Example: $0 4.9.9-r2 ~/coreos/linux"
    exit 2
fi

old_ebuild=$(echo coreos-sources-*.ebuild)
new_ebuild="coreos-sources-${new_pvr}.ebuild"

if [[ "${old_ebuild}" = 'coreos-sources-*.ebuild' ]]; then
    echo "Must be run from the coreos-sources directory."
    exit 1
fi
if [[ ! -f "${old_ebuild}" ]]; then
    echo "Multiple ebuilds in coreos-sources directory.  Aborting."
    exit 1
fi
if [[ "${old_ebuild}" = "${new_ebuild}" ]]; then
    echo "New PVR must be different from old one."
    exit 1
fi
if [[ ! -f $(echo "${srcdir}"/0001*.patch) ]]; then
    echo "${srcdir} contains no patch files."
    exit 1
fi

old_kernrelease=$(echo "${old_ebuild}" | cut -f3 -d- | cut -f1-2 -d.)
new_kernrelease=$(echo "${new_pvr}" | cut -f1 -d- | cut -f1-2 -d.)

awk '{print} /UNIPATCH_LIST/ {exit 0}' "${old_ebuild}" > "${new_ebuild}"

rm -rf "files/${old_kernrelease}" "files/${new_kernrelease}"
mkdir "files/${new_kernrelease}"
pushd "files/${new_kernrelease}" >/dev/null
mv "${srcdir}"/[0-9]*.patch .
for f in [0-9]*.patch; do
    mv "$f" "z$f"
done
ls z[0-9]*.patch | sed -e 's/^/\t${PATCH_DIR}\//g' -e 's/$/ \\/g' >> \
        "../../${new_ebuild}"
popd >/dev/null

echo '"' >> "${new_ebuild}"
rm "${old_ebuild}"
