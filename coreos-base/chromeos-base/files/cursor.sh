# We disable vt cursors by default on the kernel command line
# (so that it doesn't flash when doing boot splash and such).
#
# Re-enable it when launching a login shell.  This should only
# happen when logging in via vt or crosh or ssh and those are
# all fine.  Login shells shouldn't get launched normally.
setterm -cursor on
