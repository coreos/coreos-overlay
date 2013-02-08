# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="c04bdd4a43f9a240961c8fb07afc1386fddb5826"
CROS_WORKON_TREE="fb8c4062f8c8ed9467124d53053e7c108d1eab34"
CROS_WORKON_PROJECT="chromiumos/third_party/autotest"

CONFLICT_LIST="chromeos-base/autotest-tests-0.0.1-r335"
inherit toolchain-funcs flag-o-matic cros-workon autotest conflict

DESCRIPTION="Autotest Factory tests"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86 arm amd64"

IUSE="+xset hardened"
# Enable autotest by default.
IUSE="${IUSE} +autotest"

# Factory tests require locally installed deps, which are called out in
# autotest-factory-deps.
RDEPEND="
  chromeos-base/autotest-deps-iotools
  chromeos-base/autotest-deps-libaio
  chromeos-base/autotest-deps-audioloop
  chromeos-base/autotest-deps-glbench
  chromeos-base/autotest-private-board
  chromeos-base/chromeos-factory
  chromeos-base/flimflam-test
  >=chromeos-base/vpd-0.0.1-r11
  dev-python/jsonrpclib
  dev-python/pygobject
  dev-python/pygtk
  dev-python/ws4py
  xset? ( x11-apps/xset )
"

DEPEND="${RDEPEND}"

IUSE_TESTS="
	+tests_dummy_Fail
	+tests_dummy_Pass
	+tests_factory_Antenna
	+tests_factory_Audio
	+tests_factory_AudioInternalLoopback
	+tests_factory_AudioLoop
	+tests_factory_AudioQuality
	+tests_factory_BasicCellular
	+tests_factory_BasicGPS
	+tests_factory_BasicWifi
	+tests_factory_Camera
	+tests_factory_CameraPerformanceAls
	+tests_factory_Cellular
	+tests_factory_Connector
	+tests_factory_DeveloperRecovery
	+tests_factory_Display
	+tests_factory_Dummy
	+tests_factory_ExtDisplay
	+tests_factory_ExternalStorage
	+tests_factory_Fail
	+tests_factory_Finalize
	+tests_factory_HWID
	+tests_factory_Keyboard
	+tests_factory_KeyboardBacklight
	+tests_factory_Leds
	+tests_factory_LidSwitch
	+tests_factory_LightSensor
	+tests_factory_ProbeWifi
	+tests_factory_Prompt
	+tests_factory_RemovableStorage
	+tests_factory_RunScript
	+tests_factory_ScanSN
	+tests_factory_ScriptWrapper
	+tests_factory_Start
	+tests_factory_StressTest
	+tests_factory_SyncEventLogs
	+tests_factory_Touchpad
	+tests_factory_Touchscreen
	+tests_factory_TPM
	+tests_factory_USB
	+tests_factory_VerifyComponents
	+tests_factory_VPD
	+tests_factory_Wifi
	+tests_suite_Factory
"

IUSE="${IUSE} ${IUSE_TESTS}"

CROS_WORKON_LOCALNAME=../third_party/autotest
CROS_WORKON_SUBDIR=files

AUTOTEST_DEPS_LIST=""
AUTOTEST_CONFIG_LIST=""
AUTOTEST_PROFILERS_LIST=""

AUTOTEST_FILE_MASK="*.a *.tar.bz2 *.tbz2 *.tgz *.tar.gz"
