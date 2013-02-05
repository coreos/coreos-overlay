# The CF_NCURSES_CONFIG m4 helper from ncurses needs to be migrated from
# AC_PATH_PROGS to AC_CHECK_TOOLS.  But that'll take a while to filter
# throughout the ecosystem ...
cros_pre_src_configure_ncurses_config() {
	[[ $(cros_target) != "board_sysroot" ]] && return 0
	export ac_cv_path_NCURSES_CONFIG=${CROS_BUILD_BOARD_BIN}/${CHOST}-ncurses5-config
}
