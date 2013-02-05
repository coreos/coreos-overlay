#!/bin/bash

source tests-common.sh

inherit appid

valid_uuids=(
	'{01234567-89AB-CDEF-0123-456789ABCDEF}'
	'{11111111-1111-1111-1111-111111111111}'
	'{DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD}'
	$(grep -hs doappid ../../../../{private-,}overlays/overlay-*/chromeos-base/chromeos-bsp-*/*.ebuild | \
		gawk '{print gensub(/"/, "", "g", $2)}')
)
invalid_uuids=(
	''
	'01234567-89AB-CDEF-0123-4567-89ABCDEF0123'
	' {01234567-89AB-CDEF-0123-4567-89ABCDEF0123} '
	' {01234567-89AB-CDEF-0123-4567-89ABCDEF0123}'
	'{01234567-89AB-CDEF-0123-4567-89ABCDEF0123}	'
	'{01234567-89AB-CDEF-0123-4567-89abcDEF0123}'
	'{GGGGGGGG-GGGG-GGGG-GGGG-GGGG-GGGGGGGGGGGG}'
)

tbegin "no args"
! (doappid) >&/dev/null
tend $?

tbegin "too many args"
! (doappid "${valid_uuids[0]}" 1234) >&/dev/null
tend $?

tbegin "invalid appids"
for uuid in "${invalid_uuids[@]}" ; do
	if (doappid "${uuid}") >&/dev/null ; then
		tend 1 "not caught: ${uuid}"
	fi
	rm -rf "${D}"
done
tend $?

tbegin "valid appids"
for uuid in "${valid_uuids[@]}" ; do
	if ! (doappid "${uuid}") ; then
		tend 1 "not accepted: ${uuid}"
	fi
	rm -rf "${D}"
done
tend $?

texit
