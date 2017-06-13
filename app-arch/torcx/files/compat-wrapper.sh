#!/bin/bash
set -e

bin=${0##*/}
seal=/run/metadata/torcx

if [ -z "${bin}" ]
then
        echo 'Failed to determine the executed program name.' 1>&2
        exit 1
fi

if [ -s "${seal}" ]
then
        . "${seal}"
else
        echo "The program ${bin} is managed by torcx, which did not run." 1>&2
        exit 1
fi

if [ -z "${TORCX_BINDIR-}" ]
then
        echo "The torcx seal file ${seal} is invalid." 1>&2
        exit 1
fi

if [ ! -x "${TORCX_BINDIR}/${bin}" ]
then
        echo "The current torcx profile did not install a ${bin} program." 1>&2
        exit 1
fi

PATH="${TORCX_BINDIR}${PATH:+:${PATH}}" exec "${TORCX_BINDIR}/${bin}" "$@"
