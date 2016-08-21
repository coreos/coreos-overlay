# General (auto)make options
AM_MAKEFLAGS = --no-print-directory
# remove targets if the command fails
.DELETE_ON_ERROR:

BUILT_SOURCES =
check_DATA =
CLEANFILES = $(BUILT_SOURCES) $(check_DATA)
EXTRA_DIST =

AM_CPPFLAGS = -I$(srcdir)/src -I$(builddir)/src
AM_CFLAGS = -fno-exceptions \
	    -fno-strict-aliasing \
	    -Wall \
	    -Wclobbered \
	    -Wempty-body \
	    -Werror \
	    -Wignored-qualifiers \
	    -Wmissing-field-initializers \
	    -Wsign-compare \
	    -Wtype-limits \
	    -Wuninitialized \
	    -D__STDC_FORMAT_MACROS=1 \
	    -D_FILE_OFFSET_BITS=64 \
	    $(GLIB_CFLAGS) $(DEPS_CFLAGS)
AM_CXXFLAGS = $(AM_CFLAGS) -std=gnu++11
LDADD = $(GLIB_LIBS) $(DEPS_LIBS)

if ENABLE_DEBUG
AM_CFLAGS += -g
else
AM_CPPFLAGS += -DNDEBUG
endif

if ENABLE_GCOV
AM_CFLAGS += -fprofile-arcs -ftest-coverage
LDADD += -lgcov
endif

sbin_PROGRAMS = update_engine
bin_PROGRAMS = update_engine_client
if ENABLE_DELTA_GENERATOR
bin_PROGRAMS += delta_generator
endif

sbin_SCRIPTS = coreos-postinst \
	       coreos-setgoodroot \
	       systemd/update_engine_stub

noinst_LIBRARIES = libupdate_engine.a

check_PROGRAMS = update_engine_unittests test_http_server
TESTS = run_unittests_as_user run_unittests_as_root
EXTRA_DIST += $(TESTS)

update_engine_LDADD = libupdate_engine.a $(LDADD)
update_engine_SOURCES = src/update_engine/main.cc

delta_generator_LDADD = libupdate_engine.a $(LDADD)
delta_generator_SOURCES = src/update_engine/generate_delta_main.cc

update_engine_client_LDADD = libupdate_engine.a $(LDADD)
update_engine_client_SOURCES = src/update_engine/update_engine_client.cc

libupdate_engine_a_SOURCES = \
	src/files/file_enumerator.cc \
	src/files/file_enumerator_posix.cc \
	src/files/file_path.cc \
	src/files/file_path_constants.cc \
	src/files/file_util.cc \
	src/files/file_util_posix.cc \
	src/files/scoped_file.cc \
	src/files/scoped_temp_dir.cc \
	src/strings/string_number_conversions.cc \
	src/strings/string_printf.cc \
	src/strings/string_split.cc \
	src/update_engine/action_processor.cc \
	src/update_engine/bzip.cc \
	src/update_engine/bzip_extent_writer.cc \
	src/update_engine/certificate_checker.cc \
	src/update_engine/cycle_breaker.cc \
	src/update_engine/dbus_service.cc \
	src/update_engine/delta_diff_generator.cc \
	src/update_engine/delta_metadata.cc \
	src/update_engine/delta_performer.cc \
	src/update_engine/download_action.cc \
	src/update_engine/ext2_metadata.cc \
	src/update_engine/extent_mapper.cc \
	src/update_engine/extent_ranges.cc \
	src/update_engine/extent_writer.cc \
	src/update_engine/file_writer.cc \
	src/update_engine/filesystem_copier_action.cc \
	src/update_engine/filesystem_iterator.cc \
	src/update_engine/full_update_generator.cc \
	src/update_engine/graph_utils.cc \
	src/update_engine/http_common.cc \
	src/update_engine/http_fetcher.cc \
	src/update_engine/install_plan.cc \
	src/update_engine/kernel_copier_action.cc \
	src/update_engine/libcurl_http_fetcher.cc \
	src/update_engine/marshal.glibmarshal.c \
	src/update_engine/multi_range_http_fetcher.cc \
	src/update_engine/omaha_hash_calculator.cc \
	src/update_engine/omaha_request_action.cc \
	src/update_engine/omaha_request_params.cc \
	src/update_engine/omaha_response_handler_action.cc \
	src/update_engine/payload_processor.cc \
	src/update_engine/payload_signer.cc \
	src/update_engine/payload_state.cc \
	src/update_engine/postinstall_runner_action.cc \
	src/update_engine/prefs.cc \
	src/update_engine/simple_key_value_store.cc \
	src/update_engine/subprocess.cc \
	src/update_engine/system_state.cc \
	src/update_engine/tarjan.cc \
	src/update_engine/terminator.cc \
	src/update_engine/topological_sort.cc \
	src/update_engine/update_attempter.cc \
	src/update_engine/update_check_scheduler.cc \
	src/update_engine/update_metadata.pb.cc \
	src/update_engine/utils.cc

update_engine_unittests_LDADD = libupdate_engine.a \
				-lgtest -lgmock $(LDADD)
update_engine_unittests_SOURCES = \
	src/update_engine/testrunner.cc \
	src/files/file_path_unittest.cc \
	src/files/file_util_unittest.cc \
	src/files/scoped_file_unittest.cc \
	src/files/scoped_temp_dir_unittest.cc \
	src/strings/string_number_conversions_unittest.cc \
	src/strings/string_printf_unittest.cc \
	src/strings/string_split_unittest.cc \
	src/update_engine/action_pipe_unittest.cc \
	src/update_engine/action_processor_unittest.cc \
	src/update_engine/action_unittest.cc \
	src/update_engine/bzip_extent_writer_unittest.cc \
	src/update_engine/certificate_checker_unittest.cc \
	src/update_engine/cycle_breaker_unittest.cc \
	src/update_engine/delta_diff_generator_unittest.cc \
	src/update_engine/delta_performer_unittest.cc \
	src/update_engine/download_action_unittest.cc \
	src/update_engine/ext2_metadata_unittest.cc \
	src/update_engine/extent_mapper_unittest.cc \
	src/update_engine/extent_ranges_unittest.cc \
	src/update_engine/extent_writer_unittest.cc \
	src/update_engine/file_writer_unittest.cc \
	src/update_engine/filesystem_copier_action_unittest.cc \
	src/update_engine/filesystem_iterator_unittest.cc \
	src/update_engine/full_update_generator_unittest.cc \
	src/update_engine/graph_utils_unittest.cc \
	src/update_engine/http_fetcher_unittest.cc \
	src/update_engine/kernel_copier_action_unittest.cc \
	src/update_engine/mock_http_fetcher.cc \
	src/update_engine/mock_system_state.cc \
	src/update_engine/omaha_hash_calculator_unittest.cc \
	src/update_engine/omaha_request_action_unittest.cc \
	src/update_engine/omaha_request_params_unittest.cc \
	src/update_engine/omaha_response_handler_action_unittest.cc \
	src/update_engine/payload_processor_unittest.cc \
	src/update_engine/payload_signer_unittest.cc \
	src/update_engine/payload_state_unittest.cc \
	src/update_engine/postinstall_runner_action_unittest.cc \
	src/update_engine/prefs_unittest.cc \
	src/update_engine/simple_key_value_store_unittest.cc \
	src/update_engine/subprocess_unittest.cc \
	src/update_engine/tarjan_unittest.cc \
	src/update_engine/terminator_unittest.cc \
	src/update_engine/test_utils.cc \
	src/update_engine/topological_sort_unittest.cc \
	src/update_engine/update_attempter_mock.cc \
	src/update_engine/update_attempter_unittest.cc \
	src/update_engine/update_check_scheduler_unittest.cc \
	src/update_engine/utils_unittest.cc \
	src/update_engine/zip_unittest.cc

test_http_server_LDADD = libupdate_engine.a $(LDADD)
test_http_server_SOURCES = src/update_engine/test_http_server.cc

EXTRA_DIST += src/update_engine/marshal.list
BUILT_SOURCES += src/update_engine/marshal.glibmarshal.c \
		 src/update_engine/marshal.glibmarshal.h
%.glibmarshal.c: %.list
	$(AM_V_at) $(MKDIR_P) $(dir $@)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --body \
	    --prefix=update_engine $< > $@

%.glibmarshal.h: %.list
	$(AM_V_at) $(MKDIR_P) $(dir $@)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --header \
	    --prefix=update_engine $< > $@

EXTRA_DIST += src/update_engine/update_engine.xml
BUILT_SOURCES += src/update_engine/update_engine.dbusclient.h \
		 src/update_engine/update_engine.dbusserver.h
%.dbusclient.h: %.xml
	$(AM_V_at) $(MKDIR_P) $(dir $@)
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --mode=glib-client \
	    --prefix=update_engine_service $< > $@

%.dbusserver.h: %.xml
	$(AM_V_at) $(MKDIR_P) $(dir $@)
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --mode=glib-server \
	    --prefix=update_engine_service $< > $@

EXTRA_DIST += src/update_engine/update_metadata.proto
BUILT_SOURCES += src/update_engine/update_metadata.pb.cc \
		 src/update_engine/update_metadata.pb.h
src/%.pb.cc src/%.pb.h: src/%.proto
	$(AM_V_GEN) $(PROTOC) --proto_path=$(srcdir) --cpp_out=$(builddir) $<

# Only the test private key files are checked in
EXTRA_DIST += unittest_key.pem unittest_key2.pem
check_DATA += unittest_key.pub.pem unittest_key2.pub.pem
%.pub.pem: %.pem
	$(AM_V_at) $(MKDIR_P) $(dir $@)
	$(AM_V_GEN) openssl rsa -in $< -pubout -out $@

# TODO: revisit how to generate useful coverage data or re-add lcov to
# the SDK. It was dropped to prune perl dependencies a while back iirc.
EXTRA_DIST += local_coverage_rate
if ENABLE_GCOV
coverage: $(BUILT_SOURCES)
	lcov --directory . --zerocounters
	$(MAKE) $(AM_MAKEFLAGS) check-am
	lcov --directory . --capture --output-file app.info
	genhtml --no-function-coverage --output-directory html app.info
	./local_coverage_rate
endif
