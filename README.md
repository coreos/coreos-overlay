# Overview

This overlay contains Container Linux specific packages and Gentoo packages
that differ from their upstream Gentoo versions.

See the [portage-stable](https://github.com/coreos/portage-stable) repo
for packages which do not have Container Linux specific changes.

Licensing information can be found in the respective files, so consult
them directly. Most ebuilds are licensed under the GPL version 2.

Upstream Gentoo sources: https://gitweb.gentoo.org/repo/gentoo.git

# Important packages

`coreos-base/coreos` is the package responsible for everything that gets
built into a production image and is not OEM specific.

`coreos-base/coreos-dev` is the package responsible for everything that
gets built into a developer image and is not OEM specific.

`coreos-devel/sdk-depends` is the package responsible for everything that
gets built into the Container Linux SDK.

`coreos-devel/board-packages` is everything that could be built into a
development or production image, plus any OEM specific packages.

`coreos-base/oem-*` are the OEM specific packages. They mostly install things
that belong in the OEM partition. Any RDEPENDS from these packages should
be copied to the RDEPENDS in `board-packages` to ensure they are built.

`coreos-base/coreos-oem-*` are metapackages for OEM specific ACIs. 
