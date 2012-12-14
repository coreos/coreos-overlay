// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <string>

#include "update_engine/connection_manager.h"
#include "update_engine/mock_dbus_interface.h"
#include "update_engine/mock_system_state.h"

using std::set;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;

namespace chromeos_update_engine {

class ConnectionManagerTest : public ::testing::Test {
 public:
  ConnectionManagerTest()
      : kMockFlimFlamManagerProxy_(NULL),
        kMockFlimFlamServiceProxy_(NULL),
        kServicePath_(NULL),
        cmut_(&mock_system_state_) {
    mock_system_state_.set_connection_manager(&cmut_);
  }

 protected:
  void SetupMocks(const char* service_path);
  void SetManagerReply(gconstpointer value, const GType& type);
  void SetServiceReply(const char* service_type);
  void TestWithServiceType(
      const char* service_type, NetworkConnectionType expected_type);

  static const char* kGetPropertiesMethod;
  DBusGProxy* kMockFlimFlamManagerProxy_;
  DBusGProxy* kMockFlimFlamServiceProxy_;
  DBusGConnection* kMockSystemBus_;
  const char* kServicePath_;
  MockDbusGlib dbus_iface_;
  ConnectionManager cmut_;  // ConnectionManager under test.
  MockSystemState mock_system_state_;
};

// static
const char* ConnectionManagerTest::kGetPropertiesMethod = "GetProperties";

void ConnectionManagerTest::SetupMocks(const char* service_path) {
  int number = 1;
  kMockSystemBus_ = reinterpret_cast<DBusGConnection*>(number++);
  kMockFlimFlamManagerProxy_ = reinterpret_cast<DBusGProxy*>(number++);
  kMockFlimFlamServiceProxy_ = reinterpret_cast<DBusGProxy*>(number++);
  ASSERT_NE(kMockSystemBus_, reinterpret_cast<DBusGConnection*>(NULL));

  kServicePath_ = service_path;

  ON_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .WillByDefault(Return(kMockSystemBus_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .Times(AnyNumber());
}

void ConnectionManagerTest::SetManagerReply(gconstpointer reply_value,
                                            const GType& reply_type) {
  // Initialize return value for D-Bus call to Manager object.
  // TODO (jaysri): Free the objects allocated here.
  GHashTable* manager_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

  GArray* array = g_array_new(FALSE, FALSE, sizeof(const char*));
  ASSERT_TRUE(array != NULL);

  EXPECT_EQ(array, g_array_append_val(array, reply_value));
  GValue* array_as_value = g_new0(GValue, 1);
  EXPECT_EQ(array_as_value, g_value_init(array_as_value, reply_type));
  g_value_take_boxed(array_as_value, array);
  g_hash_table_insert(manager_hash_table,
                      const_cast<char*>("Services"),
                      array_as_value);

  // Plumb return value into mock object.
  EXPECT_CALL(dbus_iface_, ProxyCall(kMockFlimFlamManagerProxy_,
                                     StrEq(kGetPropertiesMethod),
                                     _,
                                     G_TYPE_INVALID,
                                     dbus_g_type_get_map("GHashTable",
                                                         G_TYPE_STRING,
                                                         G_TYPE_VALUE),
                                     _,
                                     G_TYPE_INVALID))
      .WillOnce(DoAll(SetArgumentPointee<5>(manager_hash_table), Return(TRUE)));

  // Set other expectations.
  EXPECT_CALL(dbus_iface_,
              ProxyNewForNameOwner(kMockSystemBus_,
                                   StrEq(flimflam::kFlimflamServiceName),
                                   StrEq(flimflam::kFlimflamServicePath),
                                   StrEq(flimflam::kFlimflamManagerInterface),
                                   _))
      .WillOnce(Return(kMockFlimFlamManagerProxy_));
  EXPECT_CALL(dbus_iface_, ProxyUnref(kMockFlimFlamManagerProxy_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .RetiresOnSaturation();
}

void ConnectionManagerTest::SetServiceReply(const char* service_type) {
  // Initialize return value for D-Bus call to Service object.
  // TODO (jaysri): Free the objects allocated here.
  GHashTable* service_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

  GValue* service_type_value = g_new0(GValue, 1);
  EXPECT_EQ(service_type_value,
            g_value_init(service_type_value, G_TYPE_STRING));
  g_value_set_static_string(service_type_value, service_type);

  g_hash_table_insert(service_hash_table,
                      const_cast<char*>("Type"),
                      service_type_value);

  // Plumb return value into mock object.
  EXPECT_CALL(dbus_iface_, ProxyCall(kMockFlimFlamServiceProxy_,
                                    StrEq(kGetPropertiesMethod),
                                    _,
                                    G_TYPE_INVALID,
                                    dbus_g_type_get_map("GHashTable",
                                                        G_TYPE_STRING,
                                                        G_TYPE_VALUE),
                                    _,
                                    G_TYPE_INVALID))
      .WillOnce(DoAll(SetArgumentPointee<5>(service_hash_table), Return(TRUE)));

  // Set other expectations.
  EXPECT_CALL(dbus_iface_,
      ProxyNewForNameOwner(kMockSystemBus_,
                             StrEq(flimflam::kFlimflamServiceName),
                             StrEq(kServicePath_),
                             StrEq(flimflam::kFlimflamServiceInterface),
                             _))
      .WillOnce(Return(kMockFlimFlamServiceProxy_));
  EXPECT_CALL(dbus_iface_, ProxyUnref(kMockFlimFlamServiceProxy_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .RetiresOnSaturation();
}

void ConnectionManagerTest::TestWithServiceType(
    const char* service_type,
    NetworkConnectionType expected_type) {

  SetupMocks("/service/guest-network");
  SetManagerReply(kServicePath_, DBUS_TYPE_G_OBJECT_PATH_ARRAY);
  SetServiceReply(service_type);

  NetworkConnectionType type;
  EXPECT_TRUE(cmut_.GetConnectionType(&dbus_iface_, &type));
  EXPECT_EQ(expected_type, type);
}

TEST_F(ConnectionManagerTest, SimpleTest) {
  TestWithServiceType(flimflam::kTypeEthernet, kNetEthernet);
  TestWithServiceType(flimflam::kTypeWifi, kNetWifi);
  TestWithServiceType(flimflam::kTypeWimax, kNetWimax);
  TestWithServiceType(flimflam::kTypeBluetooth, kNetBluetooth);
  TestWithServiceType(flimflam::kTypeCellular, kNetCellular);
}

TEST_F(ConnectionManagerTest, UnknownTest) {
  TestWithServiceType("foo", kNetUnknown);
}

TEST_F(ConnectionManagerTest, AllowUpdatesOverEthernetTest) {
  EXPECT_CALL(mock_system_state_, device_policy()).Times(0);

  // Updates over Ethernet are allowed even if there's no policy.
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetEthernet));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOverWifiTest) {
  EXPECT_CALL(mock_system_state_, device_policy()).Times(0);
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWifi));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOverWimaxTest) {
  EXPECT_CALL(mock_system_state_, device_policy()).Times(0);
  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetWimax));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOverBluetoothTest) {
  EXPECT_CALL(mock_system_state_, device_policy()).Times(0);
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetBluetooth));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOnlyOver3GPerPolicyTest) {
  policy::MockDevicePolicy allow_3g_policy;

  EXPECT_CALL(mock_system_state_, device_policy())
      .Times(1)
      .WillOnce(Return(&allow_3g_policy));

  // This test tests 3G being the only connection type being allowed.
  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetCellular));

  EXPECT_CALL(allow_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(true)));

  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetCellular));
}

TEST_F(ConnectionManagerTest, AllowUpdatesOver3GAndOtherTypesPerPolicyTest) {
  policy::MockDevicePolicy allow_3g_policy;

  EXPECT_CALL(mock_system_state_, device_policy())
      .Times(1)
      .WillOnce(Return(&allow_3g_policy));

  // This test tests multiple connection types being allowed, with
  // 3G one among them.
  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetEthernet));
  allowed_set.insert(cmut_.StringForConnectionType(kNetCellular));
  allowed_set.insert(cmut_.StringForConnectionType(kNetWifi));

  EXPECT_CALL(allow_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(true)));

  EXPECT_TRUE(cmut_.IsUpdateAllowedOver(kNetCellular));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOver3GByDefaultTest) {
  EXPECT_CALL(mock_system_state_, device_policy()).Times(1);
  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOver3GPerPolicyTest) {
  policy::MockDevicePolicy block_3g_policy;

  EXPECT_CALL(mock_system_state_, device_policy())
      .Times(1)
      .WillOnce(Return(&block_3g_policy));

  // Test that updates for 3G are blocked while updates are allowed
  // over several other types.
  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetEthernet));
  allowed_set.insert(cmut_.StringForConnectionType(kNetWifi));
  allowed_set.insert(cmut_.StringForConnectionType(kNetWimax));

  EXPECT_CALL(block_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(true)));

  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular));
}

TEST_F(ConnectionManagerTest, BlockUpdatesOver3GIfErrorInPolicyFetchTest) {
  policy::MockDevicePolicy allow_3g_policy;

  EXPECT_CALL(mock_system_state_, device_policy())
      .Times(1)
      .WillOnce(Return(&allow_3g_policy));

  set<string> allowed_set;
  allowed_set.insert(cmut_.StringForConnectionType(kNetCellular));

  // Return false for GetAllowedConnectionTypesForUpdate and see
  // that updates are still blocked for 3G despite the value being in
  // the string set above.
  EXPECT_CALL(allow_3g_policy, GetAllowedConnectionTypesForUpdate(_))
      .Times(1)
      .WillOnce(DoAll(SetArgumentPointee<0>(allowed_set), Return(false)));

  EXPECT_FALSE(cmut_.IsUpdateAllowedOver(kNetCellular));
}

TEST_F(ConnectionManagerTest, StringForConnectionTypeTest) {
  EXPECT_STREQ(flimflam::kTypeEthernet,
               cmut_.StringForConnectionType(kNetEthernet));
  EXPECT_STREQ(flimflam::kTypeWifi,
               cmut_.StringForConnectionType(kNetWifi));
  EXPECT_STREQ(flimflam::kTypeWimax,
               cmut_.StringForConnectionType(kNetWimax));
  EXPECT_STREQ(flimflam::kTypeBluetooth,
               cmut_.StringForConnectionType(kNetBluetooth));
  EXPECT_STREQ(flimflam::kTypeCellular,
               cmut_.StringForConnectionType(kNetCellular));
  EXPECT_STREQ("Unknown",
               cmut_.StringForConnectionType(kNetUnknown));
  EXPECT_STREQ("Unknown",
               cmut_.StringForConnectionType(
                   static_cast<NetworkConnectionType>(999999)));
}

TEST_F(ConnectionManagerTest, MalformedServiceList) {
  SetupMocks("/service/guest-network");
  string service_name(kServicePath_);
  SetManagerReply(&service_name, DBUS_TYPE_G_STRING_ARRAY);

  NetworkConnectionType type;
  EXPECT_FALSE(cmut_.GetConnectionType(&dbus_iface_, &type));
}

}  // namespace chromeos_update_engine
