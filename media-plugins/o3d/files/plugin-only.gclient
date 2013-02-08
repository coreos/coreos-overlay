# An element of this array (a "solution") describes a repository directory
# that will be checked out into your working copy.  Each solution may
# optionally define additional dependencies (via its DEPS file) to be
# checked out alongside the solution's directory.  A solution may also
# specify custom dependencies (via the "custom_deps" property) that
# override or augment the dependencies specified by the DEPS file.
# If a "safesync_url" is specified, it is assumed to reference the location of
# a text file which contains nothing but the last known good SCM revision to
# sync against. It is fetched if specified and used unless --head is passed

solutions = [
  { "name"        : "o3d",
    "url"         : "http://src.chromium.org/svn/trunk/o3d",
    "custom_deps" : {
      # To use the trunk of a component instead of what's in DEPS:
      #"component": "https://svnserver/component/trunk/",
      # To exclude a component from your working copy:
      #"data/really_large_component": None,

      # Don't need the following for npo3dautoplugin
      "chrome/third_party/wtl": None,
      "ipc": None,
      "third_party/jsdoctoolkit": None,
      "third_party/pdiff": None,
      "third_party/selenium_rc": None,
      "third_party/sqlite": None,
      "tools/data_pack": None,
      "tools/grit": None,
    },
    "safesync_url": ""
  },
]
