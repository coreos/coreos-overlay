function print_ld_paths() {
  # Default library directories
  local paths="$SYSROOT/usr/lib:$SYSROOT/lib"

  # Only split on newlines
  local IFS="
"

  for line in $(cat "$SYSROOT"/etc/ld.so.conf
                    "$SYSROOT"/etc/ld.so.conf.d/* 2>/dev/null); do
    if [[ "${line:0:1}" != "/" ]]; then
      continue
    fi
    if [[ "${line:0:${#SYSROOT}}" == "$SYSROOT" ]]; then
      paths="$paths:$line"
    else
      paths="$paths:$SYSROOT$line"
    fi
  done
  echo "$paths"
}

# Given the path to ld-linux.so.2, determine it's version.
function libc_version() {
  local ld_path=$1
  echo $(readlink -f $1/ld-linux.so.2  | sed 's/.*ld-\(.*\).so/\1/g')
}

function cros_pre_src_test_ldpaths() {
  # HACK(raymes): If host/target libc versions are different,
  # prepend host library path to work around
  # http://code.google.com/p/chromium-os/issues/detail?id=19936
  local host_library_path=""
  local host_libc=$(libc_version /lib32)
  local target_libc=$(libc_version "$SYSROOT/lib")
  if [[ "$host_libc" != "$target_libc" ]]; then
    host_library_path=/lib32
    ewarn "Host/target libc mismatch. Using host libs for src_test!"
  fi

  # Set LD_LIBRARY_PATH to point to libraries in $SYSROOT, so that tests
  # will load libraries from there first
  if [[ -n "$SYSROOT" ]] && [[ "$SYSROOT" != "/" ]]; then
    if [[ -n "$LD_LIBRARY_PATH" ]]; then
      export LD_LIBRARY_PATH="$host_library_path:$(print_ld_paths):$LD_LIBRARY_PATH"
    else
      export LD_LIBRARY_PATH="$host_library_path:$(print_ld_paths)"
    fi
  fi
}
