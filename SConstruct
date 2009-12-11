# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Protobuffer compilation
""" Inputs:
        target: list of targets to compile to
        source: list of sources to compile
        env: the scons environment in which we are compiling
    Outputs:
        target: the list of targets we'll emit
        source: the list of sources we'll compile"""
def ProtocolBufferEmitter(target, source, env):
  output = str(source[0])
  output = output[0:output.rfind('.proto')]
  target = [
    output + '.pb.cc',
    output + '.pb.h',
  ]
  return target, source

""" Inputs:
        source: list of sources to process
        target: list of targets to generate
        env: scons environment in which we are working
        for_signature: unused
    Outputs: a list of commands to execute to generate the targets from
             the sources."""
def ProtocolBufferGenerator(source, target, env, for_signature):
  commands = [
    '/usr/bin/protoc '
    ' --proto_path . ${SOURCES} --cpp_out .']
  return commands

proto_builder = Builder(generator = ProtocolBufferGenerator,
                        emitter = ProtocolBufferEmitter,
                        single_source = 1,
                        suffix = '.pb.cc')

env = Environment()
env['CCFLAGS'] = ' '.join("""-g
                             -fno-exceptions
                             -Wall
                             -Werror
                             -Wclobbered
                             -Wempty-body
                             -Wignored-qualifiers
                             -Wmissing-field-initializers
                             -Wsign-compare
                             -Wtype-limits
                             -Wuninitialized
                             -D_FILE_OFFSET_BITS=64
                             -I/usr/include/libxml2""".split());

env['LIBS'] = Split("""base
                       curl
                       gflags
                       glib-2.0
                       gtest
                       gthread-2.0
                       libpcrecpp
                       protobuf
                       pthread
                       ssl
                       xml2
                       z""")
env['CPPPATH'] = ['..', '../../third_party/chrome/files', '../../common']
env['LIBPATH'] = ['../../third_party/chrome']
env['BUILDERS']['ProtocolBuffer'] = proto_builder
env.ParseConfig('pkg-config --cflags --libs glib-2.0')
env.ProtocolBuffer('update_metadata.pb.cc', 'update_metadata.proto')

if ARGUMENTS.get('debug', 0):
  env['CCFLAGS'] += ' -fprofile-arcs -ftest-coverage'
  env['LIBS'] += ['gcov']



sources = Split("""action_processor.cc
                   decompressing_file_writer.cc
                   delta_diff_parser.cc
                   download_action.cc
                   filesystem_copier_action.cc
                   filesystem_iterator.cc
                   file_writer.cc
                   gzip.cc
                   install_action.cc
                   libcurl_http_fetcher.cc
                   omaha_hash_calculator.cc
                   omaha_request_prep_action.cc
                   omaha_response_handler_action.cc
                   postinstall_runner_action.cc
                   set_bootable_flag_action.cc
                   subprocess.cc
                   update_check_action.cc
		               update_metadata.pb.cc
		               utils.cc""")
main = ['main.cc']

unittest_sources = Split("""action_unittest.cc
                            action_pipe_unittest.cc
                            action_processor_unittest.cc
                            decompressing_file_writer_unittest.cc
                            delta_diff_generator_unittest.cc
                            download_action_unittest.cc
                            file_writer_unittest.cc
                            filesystem_copier_action_unittest.cc
                            filesystem_iterator_unittest.cc
                            gzip_unittest.cc
                            http_fetcher_unittest.cc
                            install_action_unittest.cc
                            integration_unittest.cc
                            mock_http_fetcher.cc
                            omaha_hash_calculator_unittest.cc
                            omaha_request_prep_action_unittest.cc
                            omaha_response_handler_action_unittest.cc
                            postinstall_runner_action_unittest.cc
                            set_bootable_flag_action_unittest.cc
                            subprocess_unittest.cc
                            test_utils.cc
                            update_check_action_unittest.cc
                            utils_unittest.cc""")
unittest_main = ['testrunner.cc']

delta_generator_sources = Split("""delta_diff_generator.cc""")
delta_generator_main = ['generate_delta_main.cc']

test_installer_main = ['test_installer_main.cc']

env.Program('update_engine', sources + main)
unittest_cmd = env.Program('update_engine_unittests',
                           sources + delta_generator_sources +
                           unittest_sources + unittest_main)

test_installer_cmd = env.Program('test_installer',
                                 sources + delta_generator_sources +
                                 unittest_sources + test_installer_main)

Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))

delta_generator_cmd = env.Program('delta_generator',
                                  sources + delta_generator_sources +
                                  delta_generator_main)

http_server_cmd = env.Program('test_http_server', 'test_http_server.cc')
