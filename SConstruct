# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

env = Environment()
env['CCFLAGS'] = '-g -fno-exceptions -Wall -Werror -D_FILE_OFFSET_BITS=64 ' + \
                 '-I/usr/include/libxml2'
env['LIBS'] = Split('curl gtest ssl xml2 z')
env['CPPPATH'] = ['..']
env.ParseConfig('pkg-config --cflags --libs glib-2.0')

if ARGUMENTS.get('debug', 0):
  env['CCFLAGS'] += ' -fprofile-arcs -ftest-coverage'
  env['LIBS'] += ['gcov']

sources = Split("""action_processor.cc
                   decompressing_file_writer.cc
                   download_action.cc
                   libcurl_http_fetcher.cc
                   omaha_hash_calculator.cc
                   update_check_action.cc""")
main = ['main.cc']

unittest_sources = Split("""action_unittest.cc
                            action_pipe_unittest.cc
                            action_processor_unittest.cc
                            decompressing_file_writer_unittest.cc
                            download_action_unittest.cc
                            file_writer_unittest.cc
                            http_fetcher_unittest.cc
                            mock_http_fetcher.cc
                            omaha_hash_calculator_unittest.cc
                            test_utils.cc
                            update_check_action_unittest.cc""")
unittest_main = ['testrunner.cc']

env.Program('update_engine', sources + main)
unittest_cmd = env.Program('update_engine_unittests',
            sources + unittest_sources + unittest_main)

Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))
