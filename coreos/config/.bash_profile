# /etc/skel/.bash_profile

# This file is sourced by bash for login shells.  The following line
# runs your .bashrc and is recommended by the bash info pages.
[[ -f ~/.bashrc ]] && . ~/.bashrc

# Chromium OS build environment settings
export CROS_WORKON_SRCROOT="/home/${USER}/trunk"
export PORTAGE_USERNAME="${USER}"
