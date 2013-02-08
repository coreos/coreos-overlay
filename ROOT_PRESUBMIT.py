# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium OS.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl and git cl.
"""

import re

_EXCLUDED_PATHS = (
    r"^inherit-review-settings-ok$",
    r".*[\\\/]debian[\\\/]rules$",
)

# These match files that should contain tabs as indentation.
_TAB_OK_PATHS = (
    r"/src/third_party/kernel/",
    r"/src/third_party/kernel-next/",
    r"/src/third_party/u-boot/",
    r"/src/third_party/u-boot-next/",
    r".*\.ebuild$",
    r".*\.eclass$",
)

# These match files that are part of out "next" developemnt flow and as such
# do not require a valid BUG= field to commit, but it's still a good idea.
_NEXT_PATHS = (
    r"/src/third_party/kernel-next",
    r"/src/third_party/u-boot-next",
)

_LICENSE_HEADER = (
     r".*? Copyright \(c\) 20[-0-9]{2,7} The Chromium OS Authors\. All rights "
       r"reserved\." "\n"
     r".*? Use of this source code is governed by a BSD-style license that can "
       "be\n"
     r".*? found in the LICENSE file\."
       "\n"
)


def CheckAndShowLicense(input_api, output_api, source_file_filter=None):
  """Check that the source files have a valid License header.

  The license header must matches our template.  If not also show the
  header that should have been used.

  """
  results = []
  license_check = input_api.canned_checks.CheckLicense(
    input_api, output_api, _LICENSE_HEADER, source_file_filter)
  results.extend(license_check)
  if license_check:
    results.extend([output_api.PresubmitNotifyResult(
      "License header should match the following:",
      long_text=_LICENSE_HEADER)])
  return results


def CheckChangeHasMandatoryBugField(input_api,
                                    output_api,
                                    source_file_filter=None):
  """Check that the commit contains a valid BUG= field."""
  msg = ('Changelist must reference a bug number using BUG=\n'
         'For example, BUG=chromium-os:8205\n'
         'BUG=none is allowed.')

  if (not input_api.AffectedSourceFiles(source_file_filter) or
      input_api.change.BUG):
    return []
  else:
    return [output_api.PresubmitError(msg)]


def CheckChangeHasBugField(input_api, output_api, source_file_filter=None):
  # This function is required because the canned BugField check doesn't
  # take a source filter.
  return input_api.canned_checks.CheckChangeHasBugField(input_api,
                                                        output_api)


def CheckChangeHasTestField(input_api, output_api, source_file_filter=None):
  # This function is required because the canned TestField check doesn't
  # take a source filter.
  return input_api.canned_checks.CheckChangeHasTestField(input_api,
                                                         output_api)


def CheckTreeIsOpen(input_api, output_api, source_file_filter=None):
  """Make sure the tree is 'open'.  If not, don't submit."""
  return input_api.canned_checks.CheckTreeIsOpen(
      input_api,
      output_api,
      json_url='http://chromiumos-status.appspot.com/current?format=json')


def CheckBuildbotPendingBuilds(input_api, output_api, source_file_filter=None):
  """Check to see if there's a backlog on the pending CL queue"""
  return input_api.canned_checks.CheckBuildbotPendingBuilds(
      input_api,
      output_api,
      'http://build.chromium.org/p/chromiumos/json/builders?filter=1',
      6,
      [])


def FilterAbsoluteSourceFile(input_api, affected_file, white_list, black_list):
  """Filters out files that aren't considered "source file".

  The lists will be compiled as regular expression and
  AffectedFile.AbsoluteLocalPath() needs to pass both list.

  Note: This function was coppied from presubmit_support.py and modified to
  check against (AbsoluteLocalPath - PresubmitLocalPath) instead of LocalPath
  because LocalPath doesn't contain enough information to disambiguate kernel,
  u-boot and -next files from the rest of ChromiumOS.

  """
  presubmit_local_path = input_api.PresubmitLocalPath()

  def RelativePath(affected_file):
    absolute_local_path = affected_file.AbsoluteLocalPath()

    assert absolute_local_path.startswith(presubmit_local_path)
    return absolute_local_path[len(presubmit_local_path):]

  def Find(relative_path, items):
    for item in items:
      if re.match(item, relative_path):
        return True

    return False

  relative_path = RelativePath(affected_file)

  return (Find(relative_path, white_list) and
          not Find(relative_path, black_list))

def RunChecklist(input_api, output_api, checklist):
  """Run through a set of checks provided in a checklist.

  The checklist is a list of tuples, each of which contains the check to run
  and a list of regular expressions of paths to ignore for this check

  """
  results = []

  for check, paths in checklist:
    white_list = input_api.DEFAULT_WHITE_LIST

    # Construct a black list from the DEFAULT_BLACK_LIST supplied by
    # depot_tools and the paths that this check should not be applied to.
    #
    # We also remove the third_party rule here because our paterns are
    # matching against the entire path from the root of the ChromiumOS
    # project.  We use the rooted paths because we want to be able to apply
    # some of the presubmit checks to things like the kernel and u-boot that
    # live in the third_party directory.
    black_list = list(input_api.DEFAULT_BLACK_LIST)
    black_list.remove(r".*\bthird_party[\\\/].*")
    black_list.extend(paths)
    sources = lambda path: FilterAbsoluteSourceFile(input_api,
                                                    path,
                                                    white_list,
                                                    black_list)
    results.extend(check(input_api, output_api, source_file_filter=sources))

  return results


def MakeCommonChecklist(input_api):
  return [(input_api.canned_checks.CheckLongLines, _EXCLUDED_PATHS),
          (input_api.canned_checks.CheckChangeHasNoStrayWhitespace,
           _EXCLUDED_PATHS),
          (CheckChangeHasTestField, _EXCLUDED_PATHS),
          (CheckAndShowLicense, _EXCLUDED_PATHS),
          (input_api.canned_checks.CheckChangeHasNoTabs,
           _EXCLUDED_PATHS + _TAB_OK_PATHS)]


def MakeUploadChecklist(input_api):
  return [(CheckChangeHasBugField, _EXCLUDED_PATHS)]


def MakeCommitChecklist(input_api):
  return [(CheckChangeHasMandatoryBugField, _EXCLUDED_PATHS),
          (CheckTreeIsOpen, _EXCLUDED_PATHS),
          (CheckBuildbotPendingBuilds, _EXCLUDED_PATHS)]


def CheckChangeOnUpload(input_api, output_api):
  """On upload we check against the common and upload lists."""
  return RunChecklist(input_api,
                      output_api,
                      MakeCommonChecklist(input_api) +
                      MakeUploadChecklist(input_api))


def CheckChangeOnCommit(input_api, output_api):
  """On commit we check against the common and commit lists."""
  return RunChecklist(input_api,
                      output_api,
                      MakeCommonChecklist(input_api) +
                      MakeCommitChecklist(input_api))


def GetPreferredTrySlaves():
  return ['ChromiumOS x86']
