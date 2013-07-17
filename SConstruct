# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# Protobuffer compilation
def ProtocolBufferEmitter(target, source, env):
  """ Inputs:
          target: list of targets to compile to
          source: list of sources to compile
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll compile"""
  output = str(source[0])
  output = output[0:output.rfind('.proto')]
  target = [
    output + '.pb.cc',
    output + '.pb.h',
  ]
  return target, source

def ProtocolBufferGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
  commands = [
    '/usr/bin/protoc '
    ' --proto_path . ${SOURCES} --cpp_out .']
  return commands

proto_builder = Builder(generator = ProtocolBufferGenerator,
                        emitter = ProtocolBufferEmitter,
                        single_source = 1,
                        suffix = '.pb.cc')

def DbusBindingsEmitter(target, source, env):
  """ Inputs:
          target: unused
          source: list containing the source .xml file
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll process"""
  output = str(source[0])
  output = output[0:output.rfind('.xml')]
  target = [
    output + '.dbusserver.h',
    output + '.dbusclient.h'
  ]
  return target, source

def DbusBindingsGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
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

def GlibMarshalEmitter(target, source, env):
  """ Inputs:
          target: unused
          source: list containing the source .list file
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll process"""
  output = str(source[0])
  output = output[0:output.rfind('.list')]
  target = [
    output + '.glibmarshal.c',
    output + '.glibmarshal.h'
  ]
  return target, source

def GlibMarshalGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
  commands = []
  for target_file in target:
    if str(target_file).endswith('.glibmarshal.h'):
      mode_flag = '--header '
    else:
      mode_flag = '--body '
    cmd = '/usr/bin/glib-genmarshal %s --prefix=update_engine ' \
          '%s > %s' % (mode_flag, str(source[0]), str(target_file))
    commands.append(cmd)
  return commands

glib_marshal_builder = Builder(generator = GlibMarshalGenerator,
                               emitter = GlibMarshalEmitter,
                               single_source = 1,
                               suffix = 'glibmarshal.c')

# Public key generation
def PublicKeyEmitter(target, source, env):
  """ Inputs:
          target: list of targets to compile to
          source: list of sources to compile
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll compile"""
  targets = []
  for source_file in source:
    output = str(source_file)
    output = output[0:output.rfind('.pem')]
    output += '.pub.pem'
    targets.append(output)
  return targets, source

def PublicKeyGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
  commands = []
  for source_file in source:
    output = str(source_file)
    output = output[0:output.rfind('.pem')]
    output += '.pub.pem'
    cmd = '/usr/bin/openssl rsa -in %s -pubout -out %s' % (source_file, output)
    commands.append(cmd)
  return commands

public_key_builder = Builder(generator = PublicKeyGenerator,
                             emitter = PublicKeyEmitter,
                             suffix = '.pub.pem')

env = Environment()
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value
for key in Split('CFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key)
  if value != None:
    env[key] = Split(value)

for key in Split('PATH PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]


env['CCFLAGS'] = ' '.join("""-g
                             -fno-exceptions
                             -fno-strict-aliasing
                             -Wall
                             -Wclobbered
                             -Wclobbered
                             -Wempty-body
                             -Wempty-body
                             -Werror
                             -Wignored-qualifiers
                             -Wignored-qualifiers
                             -Wmissing-field-initializers
                             -Wno-format
                             -Wsign-compare
                             -Wtype-limits
                             -Wtype-limits
                             -Wuninitialized
                             -D__STDC_FORMAT_MACROS=1
                             -D_FILE_OFFSET_BITS=64
                             -I/usr/include/libxml2""".split());
env['CCFLAGS'] += (' ' + ' '.join(env['CFLAGS']))

BASE_VER = os.environ.get('BASE_VER', '180609')
env['LIBS'] = Split("""bz2
                       crypto
                       curl
                       ext2fs
                       gflags
                       glib-2.0
                       gthread-2.0
                       libpcrecpp
                       metrics
                       policy-%s
                       protobuf
                       pthread
                       rootdev
                       ssl
                       udev
                       xml2""" % BASE_VER)
env['CPPPATH'] = ['..']
env['BUILDERS']['ProtocolBuffer'] = proto_builder
env['BUILDERS']['DbusBindings'] = dbus_bindings_builder
env['BUILDERS']['GlibMarshal'] = glib_marshal_builder
env['BUILDERS']['PublicKey'] = public_key_builder

# Fix issue with scons not passing pkg-config vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

pkgconfig = os.environ.get('PKG_CONFIG', 'pkg-config')

env.ParseConfig(pkgconfig + ' --cflags --libs '
                'dbus-1 dbus-glib-1 gio-2.0 gio-unix-2.0 glib-2.0 libchrome-%s '
                'libchromeos-%s' % (BASE_VER, BASE_VER))
env.ProtocolBuffer('update_metadata.pb.cc', 'update_metadata.proto')
env.PublicKey('unittest_key.pub.pem', 'unittest_key.pem')
env.PublicKey('unittest_key2.pub.pem', 'unittest_key2.pem')

env.DbusBindings('update_engine.dbusclient.h', 'update_engine.xml')

env.GlibMarshal('marshal.glibmarshal.c', 'marshal.list')

if ARGUMENTS.get('debug', 0):
  env['CCFLAGS'] += ['-fprofile-arcs', '-ftest-coverage']
  env['LIBS'] += ['bz2', 'gcov']

sources = Split("""action_processor.cc
                   bzip.cc
                   bzip_extent_writer.cc
                   certificate_checker.cc
                   chrome_browser_proxy_resolver.cc
                   chrome_proxy_resolver.cc
                   connection_manager.cc
                   cycle_breaker.cc
                   dbus_service.cc
                   delta_diff_generator.cc
                   delta_performer.cc
                   download_action.cc
                   extent_mapper.cc
                   extent_ranges.cc
                   extent_writer.cc
                   filesystem_copier_action.cc
                   filesystem_iterator.cc
                   file_descriptor.cc
                   file_writer.cc
                   full_update_generator.cc
                   gpio_handler.cc
                   graph_utils.cc
                   http_common.cc
                   http_fetcher.cc
                   install_plan.cc
                   libcurl_http_fetcher.cc
                   marshal.glibmarshal.c
                   metadata.cc
                   multi_range_http_fetcher.cc
                   omaha_hash_calculator.cc
                   omaha_request_action.cc
                   omaha_request_params.cc
                   omaha_response_handler_action.cc
                   payload_signer.cc
                   payload_state.cc
                   postinstall_runner_action.cc
                   prefs.cc
                   proxy_resolver.cc
                   simple_key_value_store.cc
                   subprocess.cc
                   system_state.cc
                   tarjan.cc
                   terminator.cc
                   topological_sort.cc
                   update_attempter.cc
                   update_check_scheduler.cc
                   update_metadata.pb.cc
                   utils.cc""")
main = ['main.cc']

unittest_sources = Split("""action_unittest.cc
                            action_pipe_unittest.cc
                            action_processor_unittest.cc
                            bzip_extent_writer_unittest.cc
                            certificate_checker_unittest.cc
                            chrome_browser_proxy_resolver_unittest.cc
                            chrome_proxy_resolver_unittest.cc
                            connection_manager_unittest.cc
                            cycle_breaker_unittest.cc
                            delta_diff_generator_unittest.cc
                            delta_performer_unittest.cc
                            download_action_unittest.cc
                            extent_mapper_unittest.cc
                            extent_ranges_unittest.cc
                            extent_writer_unittest.cc
                            file_writer_unittest.cc
                            filesystem_copier_action_unittest.cc
                            filesystem_iterator_unittest.cc
                            full_update_generator_unittest.cc
                            gpio_handler_unittest.cc
                            gpio_mock_file_descriptor.cc
                            gpio_mock_udev_interface.cc
                            graph_utils_unittest.cc
                            http_fetcher_unittest.cc
                            metadata_unittest.cc
                            mock_http_fetcher.cc
                            mock_system_state.cc
                            omaha_hash_calculator_unittest.cc
                            omaha_request_action_unittest.cc
                            omaha_request_params_unittest.cc
                            omaha_response_handler_action_unittest.cc
                            payload_signer_unittest.cc
                            payload_state_unittest.cc
                            postinstall_runner_action_unittest.cc
                            prefs_unittest.cc
                            simple_key_value_store_unittest.cc
                            subprocess_unittest.cc
                            tarjan_unittest.cc
                            terminator_unittest.cc
                            test_utils.cc
                            topological_sort_unittest.cc
                            update_attempter_unittest.cc
                            update_check_scheduler_unittest.cc
                            utils_unittest.cc
                            zip_unittest.cc""")
unittest_main = ['testrunner.cc']

client_main = ['update_engine_client.cc']

delta_generator_main = ['generate_delta_main.cc']

# Hack to generate header files first. They are generated as a side effect
# of generating other files (usually their corresponding .c(c) files),
# so we make all sources depend on those other files.
all_sources = []
all_sources.extend(sources)
all_sources.extend(unittest_sources)
all_sources.extend(main)
all_sources.extend(unittest_main)
all_sources.extend(client_main)
all_sources.extend(delta_generator_main)
for source in all_sources:
  if source.endswith('_unittest.cc'):
    env.Depends(source, 'unittest_key.pub.pem')
  if source.endswith('.glibmarshal.c') or source.endswith('.pb.cc'):
    continue
  env.Depends(source, 'update_metadata.pb.cc')
  env.Depends(source, 'marshal.glibmarshal.c')
  env.Depends(source, 'update_engine.dbusclient.h')

update_engine_core = env.Library('update_engine_core', sources)
env.Prepend(LIBS=[update_engine_core])

env.Program('update_engine', main)

client_cmd = env.Program('update_engine_client', client_main);

delta_generator_cmd = env.Program('delta_generator',
                                  delta_generator_main)

http_server_cmd = env.Program('test_http_server', 'test_http_server.cc')

unittest_env = env.Clone()
unittest_env.Append(LIBS=['gmock', 'gtest'])
unittest_cmd = unittest_env.Program('update_engine_unittests',
                           unittest_sources + unittest_main)
Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))
