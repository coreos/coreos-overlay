# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium OS.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl and git cl.
"""

import difflib
import os
import re

_EBUILD_FILES = (
    r".*\.ebuild",
)

def _IsCrosWorkonEbuild(ebuild_contents):
  pattern = re.compile('^ *inherit[-\._a-z0-9 ]*cros-workon')
  for line in ebuild_contents:
    if pattern.match(line):
      return True
  return False

def Check9999Updated(input_api, output_api, source_file_filter=None):
  """Checks that the 9999 ebuild was also modified."""
  output = []
  inconsistent = set()
  missing_9999 = set()
  for f in input_api.AffectedSourceFiles(source_file_filter):
    ebuild_contents = f.NewContents()
    # only look at non-9999
    if f.LocalPath().endswith('-9999.ebuild'):
      continue
    if _IsCrosWorkonEbuild(ebuild_contents):
      dir = os.path.dirname(f.AbsoluteLocalPath())
      ebuild = os.path.basename(dir)
      devebuild_path =  os.path.join(dir, ebuild + '-9999.ebuild')
      # check if 9999 ebuild exists
      if not os.path.isfile(devebuild_path):
        missing_9999.add(ebuild)
        continue
      diff = difflib.ndiff(ebuild_contents,
                           open(devebuild_path).read().splitlines())
      for line in diff:
        if line.startswith('+') or line.startswith('-'):
          # ignore empty-lines
          if len(line) == 2:
            continue
          if not (line[2:].startswith('KEYWORDS=') or
                  line[2:].startswith('CROS_WORKON_COMMIT=')):
            inconsistent.add(f.LocalPath())

  if missing_9999:
    output.append(output_api.PresubmitPromptWarning(
        'Missing 9999 for these cros-workon ebuilds:', items=missing_9999))
  if inconsistent:
    output.append(output_api.PresubmitPromptWarning(
        'Following ebuilds are inconsistent with 9999:', items=inconsistent))
  return output

def CheckChange(input_api, output_api, committing):
    ebuilds = lambda x: input_api.FilterSourceFile(x, white_list=_EBUILD_FILES)
    results = []
    results += Check9999Updated(input_api, output_api,
                                source_file_filter=ebuilds)
    return results

def CheckChangeOnUpload(input_api, output_api):
    return CheckChange(input_api, output_api, False)

def CheckChangeOnCommit(input_api, output_api):
    return CheckChange(input_api, output_api, True)
