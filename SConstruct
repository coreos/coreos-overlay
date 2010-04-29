# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

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

""" Inputs:
        target: unused
        source: list containing the source .xml file
        env: the scons environment in which we are compiling
    Outputs:
        target: the list of targets we'll emit
        source: the list of sources we'll process"""
def DbusBindingsEmitter(target, source, env):
  output = str(source[0])
  output = output[0:output.rfind('.xml')]
  target = [
    output + '.dbusserver.h',
    output + '.dbusclient.h'
  ]
  return target, source

""" Inputs:
        source: list of sources to process
        target: list of targets to generate
        env: scons environment in which we are working
        for_signature: unused
    Outputs: a list of commands to execute to generate the targets from
             the sources."""
def DbusBindingsGenerator(source, target, env, for_signature):
  commands = []
  for target_file in target:
    if str(target_file).endswith('.dbusserver.h'):
      mode_flag = '--mode=glib-server '
    else:
      mode_flag = '--mode=glib-client '
    cmd = '/usr/bin/dbus-binding-tool %s --prefix=update_engine_service ' \
          '%s > %s' % (mode_flag, str(source[0]), str(target_file))
    commands.append(cmd)
  return commands

dbus_bindings_builder = Builder(generator = DbusBindingsGenerator,
                                emitter = DbusBindingsEmitter,
                                single_source = 1,
                                suffix = 'dbusclient.h')

env = Environment()
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value
for key in Split('CFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key)
  if value != None:
    env[key] = Split(value)

for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]


    # -Wclobbered
    # -Wempty-body
    # -Wignored-qualifiers
    # -Wtype-limits
env['CCFLAGS'] = ' '.join("""-g
                             -fno-exceptions
                             -fno-strict-aliasing
                             -Wall
                             -Wclobbered
                             -Wempty-body
                             -Werror
                             -Wignored-qualifiers
                             -Wmissing-field-initializers
                             -Wsign-compare
                             -Wtype-limits
                             -Wuninitialized
                             -D__STDC_FORMAT_MACROS=1
                             -D_FILE_OFFSET_BITS=64
                             -I/usr/include/libxml2""".split());
env['CCFLAGS'] += (' ' + ' '.join(env['CFLAGS']))

env['LIBS'] = Split("""base
                       bz2
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
env['BUILDERS']['DbusBindings'] = dbus_bindings_builder

# Fix issue with scons not passing pkg-config vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

env.ParseConfig('pkg-config --cflags --libs '
                'dbus-1 dbus-glib-1 gio-2.0 gio-unix-2.0 glib-2.0')
env.ProtocolBuffer('update_metadata.pb.cc', 'update_metadata.proto')

env.DbusBindings('update_engine.dbusclient.h', 'update_engine.xml')

if ARGUMENTS.get('debug', 0):
  env['CCFLAGS'] += ' -fprofile-arcs -ftest-coverage'
  env['LIBS'] += ['bz2', 'gcov']

sources = Split("""action_processor.cc
                   bzip.cc
                   bzip_extent_writer.cc
                   cycle_breaker.cc
                   dbus_service.cc
                   decompressing_file_writer.cc
                   delta_diff_generator.cc
                   delta_performer.cc
                   download_action.cc
                   extent_mapper.cc
                   extent_writer.cc
                   filesystem_copier_action.cc
                   filesystem_iterator.cc
                   file_writer.cc
                   graph_utils.cc
                   gzip.cc
                   libcurl_http_fetcher.cc
                   omaha_hash_calculator.cc
                   omaha_request_prep_action.cc
                   omaha_response_handler_action.cc
                   postinstall_runner_action.cc
                   set_bootable_flag_action.cc
                   split_file_writer.cc
                   subprocess.cc
                   tarjan.cc
                   topological_sort.cc
                   update_attempter.cc
                   update_check_action.cc
		               update_metadata.pb.cc
		               utils.cc""")
main = ['main.cc']

unittest_sources = Split("""action_unittest.cc
                            action_pipe_unittest.cc
                            action_processor_unittest.cc
                            bzip_extent_writer_unittest.cc
                            cycle_breaker_unittest.cc
                            decompressing_file_writer_unittest.cc
                            delta_diff_generator_unittest.cc
                            delta_performer_unittest.cc
                            download_action_unittest.cc
                            extent_mapper_unittest.cc
                            extent_writer_unittest.cc
                            file_writer_unittest.cc
                            filesystem_copier_action_unittest.cc
                            filesystem_iterator_unittest.cc
                            graph_utils_unittest.cc
                            http_fetcher_unittest.cc
                            mock_http_fetcher.cc
                            omaha_hash_calculator_unittest.cc
                            omaha_request_prep_action_unittest.cc
                            omaha_response_handler_action_unittest.cc
                            postinstall_runner_action_unittest.cc
                            set_bootable_flag_action_unittest.cc
                            split_file_writer_unittest.cc
                            subprocess_unittest.cc
                            tarjan_unittest.cc
                            test_utils.cc
                            topological_sort_unittest.cc
                            update_check_action_unittest.cc
                            utils_unittest.cc
                            zip_unittest.cc""")
unittest_main = ['testrunner.cc']

client_main = ['update_engine_client.cc']

delta_generator_main = ['generate_delta_main.cc']

env.Program('update_engine', sources + main)
unittest_cmd = env.Program('update_engine_unittests',
                           sources + unittest_sources + unittest_main)

client_cmd = env.Program('update_engine_client', sources + client_main);

Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))

delta_generator_cmd = env.Program('delta_generator',
                                  sources + delta_generator_main)

http_server_cmd = env.Program('test_http_server', 'test_http_server.cc')
