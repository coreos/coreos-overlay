# General (auto)make options
AM_MAKEFLAGS = --no-print-directory
# remove targets if the command fails
.DELETE_ON_ERROR:

BUILT_SOURCES =
check_DATA =
CLEANFILES = $(BUILT_SOURCES) $(check_DATA)
EXTRA_DIST =

AM_CPPFLAGS = -I$(srcdir)/..
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
	    -Wno-error=unused-local-typedefs \
	    -D__STDC_FORMAT_MACROS=1 \
	    -D_FILE_OFFSET_BITS=64 \
	    $(GLIB_CFLAGS) $(DEPS_CFLAGS)
AM_CXXFLAGS = $(AM_CFLAGS)
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
update_engine_SOURCES = main.cc \
			update_engine.dbusserver.h

delta_generator_LDADD = libupdate_engine.a $(LDADD)
delta_generator_SOURCES = generate_delta_main.cc

update_engine_client_LDADD = libupdate_engine.a $(LDADD)
update_engine_client_SOURCES = update_engine_client.cc \
			       update_engine.dbusclient.h

libupdate_engine_a_SOURCES = \
	action.h \
	action_pipe.h \
	action_processor.cc \
	action_processor.h \
	bzip.cc \
	bzip.h \
	bzip_extent_writer.cc \
	bzip_extent_writer.h \
	certificate_checker.cc \
	certificate_checker.h \
	cycle_breaker.cc \
	cycle_breaker.h \
	dbus_constants.h \
	dbus_interface.h \
	dbus_service.cc \
	dbus_service.h \
	delta_diff_generator.cc \
	delta_diff_generator.h \
	delta_performer.cc \
	delta_performer.h \
	download_action.cc \
	download_action.h \
	extent_mapper.cc \
	extent_mapper.h \
	extent_ranges.cc \
	extent_ranges.h \
	extent_writer.cc \
	extent_writer.h \
	file_writer.cc \
	file_writer.h \
	filesystem_copier_action.cc \
	filesystem_copier_action.h \
	filesystem_iterator.cc \
	filesystem_iterator.h \
	full_update_generator.cc \
	full_update_generator.h \
	graph_types.h \
	graph_utils.cc \
	graph_utils.h \
	http_common.cc \
	http_common.h \
	http_fetcher.cc \
	http_fetcher.h \
	install_plan.cc \
	install_plan.h \
	libcurl_http_fetcher.cc \
	libcurl_http_fetcher.h \
	marshal.glibmarshal.c \
	marshal.glibmarshal.h \
	metadata.cc \
	metadata.h \
	multi_range_http_fetcher.cc \
	multi_range_http_fetcher.h \
	omaha_hash_calculator.cc \
	omaha_hash_calculator.h \
	omaha_request_action.cc \
	omaha_request_action.h \
	omaha_request_params.cc \
	omaha_request_params.h \
	omaha_response.h \
	omaha_response_handler_action.cc \
	omaha_response_handler_action.h \
	payload_signer.cc \
	payload_signer.h \
	payload_state.cc \
	payload_state.h \
	payload_state_interface.h \
	postinstall_runner_action.cc \
	postinstall_runner_action.h \
	prefs.cc \
	prefs.h \
	prefs_interface.h \
	real_system_state.h \
	simple_key_value_store.cc \
	simple_key_value_store.h \
	subprocess.cc \
	subprocess.h \
	system_state.cc \
	system_state.h \
	tarjan.cc \
	tarjan.h \
	terminator.cc \
	terminator.h \
	topological_sort.cc \
	topological_sort.h \
	update_attempter.cc \
	update_attempter.h \
	update_check_scheduler.cc \
	update_check_scheduler.h \
	update_metadata.pb.cc \
	update_metadata.pb.h \
	utils.cc \
	utils.h

update_engine_unittests_LDADD = libupdate_engine.a \
				-lgtest -lgmock $(LDADD)
update_engine_unittests_SOURCES = \
	testrunner.cc \
	action_mock.h \
	action_pipe_unittest.cc \
	action_processor_mock.h \
	action_processor_unittest.cc \
	action_unittest.cc \
	bzip_extent_writer_unittest.cc \
	certificate_checker_mock.h \
	certificate_checker_unittest.cc \
	cycle_breaker_unittest.cc \
	delta_diff_generator_unittest.cc \
	delta_performer_unittest.cc \
	download_action_unittest.cc \
	extent_mapper_unittest.cc \
	extent_ranges_unittest.cc \
	extent_writer_unittest.cc \
	file_writer_mock.h \
	file_writer_unittest.cc \
	filesystem_copier_action_unittest.cc \
	filesystem_iterator_unittest.cc \
	full_update_generator_unittest.cc \
	graph_utils_unittest.cc \
	http_fetcher_unittest.cc \
	http_fetcher_unittest.h \
	metadata_unittest.cc \
	mock_dbus_interface.h \
	mock_file_writer.h \
	mock_http_fetcher.cc \
	mock_http_fetcher.h \
	mock_payload_state.h \
	mock_system_state.cc \
	mock_system_state.h \
	omaha_hash_calculator_unittest.cc \
	omaha_request_action_unittest.cc \
	omaha_request_params_unittest.cc \
	omaha_response_handler_action_unittest.cc \
	payload_signer_unittest.cc \
	payload_state_unittest.cc \
	postinstall_runner_action_unittest.cc \
	prefs_mock.h \
	prefs_unittest.cc \
	simple_key_value_store_unittest.cc \
	subprocess_unittest.cc \
	tarjan_unittest.cc \
	terminator_unittest.cc \
	test_utils.cc \
	test_utils.h \
	topological_sort_unittest.cc \
	update_attempter_mock.h \
	update_attempter_unittest.cc \
	update_check_scheduler_unittest.cc \
	utils_unittest.cc \
	zip_unittest.cc

test_http_server_SOURCES = \
	test_http_server.cc \
	http_common.cc \
	http_common.h \
	http_fetcher_unittest.h

EXTRA_DIST += marshal.list
BUILT_SOURCES += marshal.glibmarshal.c marshal.glibmarshal.h
%.glibmarshal.c: %.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --body \
	    --prefix=update_engine $< > $@

%.glibmarshal.h: %.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --header \
	    --prefix=update_engine $< > $@

EXTRA_DIST += update_engine.xml
BUILT_SOURCES += update_engine.dbusclient.h update_engine.dbusserver.h
%.dbusclient.h: %.xml
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --mode=glib-client \
	    --prefix=update_engine_service $< > $@

%.dbusserver.h: %.xml
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --mode=glib-server \
	    --prefix=update_engine_service $< > $@

EXTRA_DIST += update_metadata.proto
BUILT_SOURCES += update_metadata.pb.cc update_metadata.pb.h
%.pb.cc %.pb.h: %.proto
	$(AM_V_GEN) $(PROTOC) --proto_path=$(srcdir) --cpp_out=$(builddir) $<

# Only the test private key files are checked in
EXTRA_DIST += unittest_key.pem unittest_key2.pem
check_DATA += unittest_key.pub.pem unittest_key2.pub.pem
%.pub.pem: %.pem
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
