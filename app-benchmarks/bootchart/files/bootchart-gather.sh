#!/bin/sh -e
# bootchart-gather
#
# Copyright © 2009 Canonical Ltd.
# Author: Scott James Remnant <scott@netsplit.com>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Gather output of the bootchart collector into a tarball that bootchart
# can process itself.
TARBALL="$1"
if [ -z "$TARBALL" ]; then
    echo "Usage: $0 TARBALL [DIR]" 1>&2
    exit 1
fi

if [ "${TARBALL#/}" = "$TARBALL" ]; then
    TARBALL="$(pwd)/$TARBALL"
fi

DIR="$2"
[ -n "$DIR" ] || DIR="."


cd $DIR

# Output the header file with information about the system
{
    echo "version = $(dpkg-query -f'${Version}' -W bootchart)"
    echo "title = Boot chart for $(hostname) ($(date))"
    echo "system.uname = $(uname -srvm)"
    echo "system.release = $(lsb_release -sd)"
    case `uname -m` in
    arm*)
        echo -n "system.cpu = $(grep '^Processor' /proc/cpuinfo)"
        if [ `grep -c '^processor' /proc/cpuinfo` -gt 0 ]; then
            echo " ($(grep -c '^processor' /proc/cpuinfo))"
        else
            echo " (1)"
        fi
        ;;
    *)
        echo "system.cpu = $(grep '^model name' /proc/cpuinfo)"\
             "($(grep -c '^model name' /proc/cpuinfo))"
        ;;
    esac
    echo "system.kernel.options = $(sed q /proc/cmdline)"
} > header

# Create a tarball of the logs and header which can be parsed into the chart
tar czf $TARBALL header proc_stat.log proc_diskstats.log proc_ps.log

exit 0
