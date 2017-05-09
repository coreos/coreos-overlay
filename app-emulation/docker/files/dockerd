#!/bin/bash
# Wrapper for launching docker daemons with selinux default on
# This wrapper script has been deprecated (euank: 2017-05-09) and is retained
# for backwards compatibility.

set -e

parse_docker_args() {
    local flag
    while [[ $# -gt 0 ]]; do
        flag="$1"
        shift

        # treat --flag=foo and --flag foo identically
        if [[ "${flag}" == *=* ]]; then
            set -- "${flag#*=}" "$@"
            flag="${flag%=*}"
        fi

        case "${flag}" in
            --selinux-enabled)
                ARG_SELINUX="$1"
                shift
                ;;
            *)
                # ignore everything else
                ;;
        esac
    done
}

parse_docker_args "$@"

USE_SELINUX=""
# Do not override selinux if it is already explicitly configured.
if [[ -z "${ARG_SELINUX}" ]]; then
        # If unspecified, default on
        USE_SELINUX="--selinux-enabled"
fi

exec dockerd "$@" ${USE_SELINUX}
