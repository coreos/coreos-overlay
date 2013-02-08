#!/bin/sh
exec gawk -F, '{print gensub(/[[\]]/, "", "g", $2); exit}' "$1"/configure.ac
