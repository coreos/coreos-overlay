# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Makefile for disable_echo utility
#

CFLAGS += -D_GNU_SOURCE -Wall -Werror
OBJS = disable_echo.o
COMMAND = disable_echo

all: $(COMMAND)

clean:
	rm -f $(COMMAND) $(OBJS)
