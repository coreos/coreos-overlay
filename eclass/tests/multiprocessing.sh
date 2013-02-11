#!/bin/bash

source tests-common.sh

inherit multiprocessing

tbegin "simple"
MAKEOPTS="-j1" multijob_init
multijob_child_init ls -d / >/dev/null || die "fail!"
multijob_finish
tend $?

tbegin "less simple"
multijob_init -j3
multijob_child_init true  || die "fail!"
multijob_child_init false || die "fail!"
multijob_child_init true  || die "fail!"
multijob_finish
tend $(( $? == 1 ? 0 : 1 ))

tbegin "less less simple"
multijob_init -j1
multijob_child_init true  || die "fail!"
multijob_child_init false || die "fail!"
multijob_child_init true  && die "fail!"
multijob_finish
tend $?

tbegin "less less less simple"
multijob_init -j10
multijob_child_init true  || die "fail!"
multijob_finish_one       || die "fail!"
multijob_child_init false || die "fail!"
multijob_finish_one       && die "fail!"
multijob_child_init true  || die "fail!"
multijob_finish_one       || die "fail!"
multijob_finish
tend $?

texit
