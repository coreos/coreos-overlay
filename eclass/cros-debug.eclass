# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Set -DNDEBUG if the cros-debug USE flag is not defined.
#

inherit flag-o-matic

IUSE="cros-debug"

cros-debug-add-NDEBUG() {
  use cros-debug || append-flags -DNDEBUG
}
