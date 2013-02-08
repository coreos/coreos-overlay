# Fixes up login defs for PAM by commenting all non-PAM options and adding a
# comment that it is not supported with PAM.
#
# Call with lib/getdef.c and etc/login.defs as args in the root source directory
# of shadow, ie:
#
#   gawk -f login_defs.awk lib/getdef.c etc/login.defs > login.defs.new
#

(FILENAME == "lib/getdef.c") {
	if ($2 == "USE_PAM")
		start_printing = 1
	else if ($1 == "#endif")
		nextfile
	else if (start_printing == 1)
		VARS[count++] = substr($1, 3, length($1) - 4)
}

(FILENAME != "lib/getdef.c") {
	print_line = 1
	for (x in VARS) {
		regex = "(^|#)" VARS[x]
		if ($0 ~ regex) {
			print_line = 0
			printf("%s%s\t(NOT SUPPORTED WITH PAM)\n",
				($0 ~ /^#/) ? "" : "#", $0)
		}
	}
	if (print_line)
		print $0
}

