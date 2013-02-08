# /etc/skel/.bashrc
#
# This file is sourced by all *interactive* bash shells on startup,
# including some apparently interactive shells such as scp and rcp
# that can't tolerate any output.  So make sure this doesn't display
# anything or bad things will happen !


# Test for an interactive shell.  There is no need to set anything
# past this point for scp and rcp, and it's important to refrain from
# outputting anything in those cases.
if [[ $- != *i* ]] ; then
  # Shell is non-interactive.  Be done now!
  return
fi


# Chromium OS interactive shell settings

# Settings necessary for both users and automation belong in bash_profile,
# while user-specific and site-specific settings belong in ~/.cros_chrootrc
# outside the chroot.

export PS1="(cros-chroot) ${PS1}"
[[ -f /usr/share/crosutils/bash_completion ]] &&
  . /usr/share/crosutils/bash_completion
