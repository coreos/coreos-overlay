#!/bin/bash
exec awk '
  $1 == "CORE" { core = $NF }
  $1 == "MAJOR" { major = $NF }
  $1 == "MINOR" { minor = $NF }
  END { printf "%s.%s.%s\n", core, major, minor }
' "$1/Makefile"
