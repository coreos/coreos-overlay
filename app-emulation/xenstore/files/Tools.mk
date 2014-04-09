# Prefix and install folder
prefix              := /usr
PREFIX              := $(prefix)
exec_prefix         := ${prefix}
libdir              := ${exec_prefix}/lib
LIBDIR              := $(libdir)

# A debug build of tools?
debug               := n

# Tools path
BISON               :=
FLEX                :=
PYTHON              :=
PYTHON_PATH         :=
PERL                :=
CURL_CONFIG         :=
XML2_CONFIG         :=
BASH                :=
XGETTTEXT           :=
AS86                :=
LD86                :=
BCC                 :=
IASL                :=
FETCHER             :=
SEABIOS_PATH        :=
OVMF_PATH           :=

# Extra folder for libs/includes
PREPEND_INCLUDES    :=
PREPEND_LIB         :=
APPEND_INCLUDES     :=
APPEND_LIB          :=

PTHREAD_CFLAGS      := -pthread
PTHREAD_LDFLAGS     := -pthread
PTHREAD_LIBS        :=

PTYFUNCS_LIBS       :=

# Download GIT repositories via HTTP or GIT's own protocol?
# GIT's protocol is faster and more robust, when it works at all (firewalls
# may block it). We make it the default, but if your GIT repository downloads
# fail or hang, please specify GIT_HTTP=y in your environment.
GIT_HTTP            := n

# Optional components
XENSTAT_XENTOP      := n
LIBXENAPI_BINDINGS  := n
OCAML_TOOLS         := n
FLASK_POLICY        := n
CONFIG_OVMF         := n
CONFIG_ROMBIOS      := n
CONFIG_SEABIOS      := n
CONFIG_QEMU_TRAD    := n
CONFIG_QEMU_XEN     := n
CONFIG_XEND         := n
CONFIG_BLKTAP1      := n

#System options
ZLIB                :=
CONFIG_LIBICONV     := n
CONFIG_GCRYPT       := n
EXTFS_LIBS          :=
CURSES_LIBS         :=

FILE_OFFSET_BITS    :=
