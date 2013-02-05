get_sysroot_ld_paths() {
	# Default library directories
	local paths="$SYSROOT/usr/lib64:$SYSROOT/lib64:$SYSROOT/usr/lib:$SYSROOT/lib"

	# Only split on newlines
	local IFS="
"

	local line
	for line in $(cat "$SYSROOT"/etc/ld.so.conf
			"$SYSROOT"/etc/ld.so.conf.d/* 2>/dev/null); do
		# Ignore lines not starting with '/', e.g. empty lines or comments
		if [[ "${line:0:1}" != "/" ]]; then
			continue
		fi

		# Prepend $SYSROOT (/build/<board>) to all library paths
		# except for those already prefixed with $SYSROOT
		if [[ "${line:0:${#SYSROOT}}" == "$SYSROOT" ]]; then
			paths="$paths:$line"
		else
			paths="$paths:$SYSROOT$line"
		fi
	done
	echo "$paths"
}

cros_pre_src_test_ldpaths() {
	# Set LD_LIBRARY_PATH to point to libraries in $SYSROOT, so that tests
	# will load libraries from there first
	if [[ -n "$SYSROOT" ]] && [[ "$SYSROOT" != "/" ]]; then
		if [[ -n "$LD_LIBRARY_PATH" ]]; then
			export LD_LIBRARY_PATH="$(get_sysroot_ld_paths):$LD_LIBRARY_PATH"
		else
			export LD_LIBRARY_PATH="$(get_sysroot_ld_paths)"
		fi
	fi
}
