// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include "update_engine/flimflam_proxy.h"
#include "update_engine/mock_dbus_interface.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace chromeos_update_engine {

template<typename T, void F(T)>
class ScopedRelease {
 public:
  ScopedRelease(T obj) : obj_(obj) {}
  ~ScopedRelease() {
    F(obj_);
  }

 private:
  T obj_;
};

class FlimFlamProxyTest : public ::testing::Test {
 protected:
  void TestWithServiceType(
      const char* service_type, NetworkConnectionType expected_type);
};

void FlimFlamProxyTest::TestWithServiceType(
    const char* service_type,
    NetworkConnectionType expected_type) {
  int number = 1;
  DBusGConnection* kMockSystemBus =
      reinterpret_cast<DBusGConnection*>(number++);
  DBusGProxy* kMockFlimFlamManagerProxy =
      reinterpret_cast<DBusGProxy*>(number++);
  DBusGProxy* kMockFlimFlamServiceProxy =
      reinterpret_cast<DBusGProxy*>(number++);
  ASSERT_NE(kMockSystemBus, reinterpret_cast<DBusGConnection*>(NULL));
  const char* kServicePath = "/foo/service";
  const char kGetPropertiesMethod[] = "GetProperties";
  
  MockDbusGlib dbus_iface;
  
  EXPECT_CALL(dbus_iface,
              ProxyNewForNameOwner(kMockSystemBus,
                                   StrEq(kFlimFlamDbusService),
                                   StrEq(kFlimFlamDbusManagerPath),
                                   StrEq(kFlimFlamDbusManagerInterface),
                                   _))
      .WillOnce(Return(kMockFlimFlamManagerProxy));
  EXPECT_CALL(dbus_iface,
              ProxyNewForNameOwner(kMockSystemBus,
                                   StrEq(kFlimFlamDbusService),
                                   StrEq(kServicePath),
                                   StrEq(kFlimFlamDbusServiceInterface),
                                   _))
      .WillOnce(Return(kMockFlimFlamServiceProxy));
      
  EXPECT_CALL(dbus_iface, ProxyUnref(kMockFlimFlamManagerProxy));
  EXPECT_CALL(dbus_iface, ProxyUnref(kMockFlimFlamServiceProxy));

  EXPECT_CALL(dbus_iface, BusGet(DBUS_BUS_SYSTEM, _))
      .Times(2)
      .WillRepeatedly(Return(kMockSystemBus));
  
  // Set up return value for first dbus call (to Manager object).
  GHashTable* manager_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
  ScopedRelease<GHashTable*, g_hash_table_unref> manager_hash_table_unref(
      manager_hash_table);
  
  GArray* array = g_array_new(FALSE, FALSE, sizeof(const char*));
  ASSERT_TRUE(array != NULL);
  
  EXPECT_EQ(array, g_array_append_val(array, kServicePath));
  GValue array_value = {0, {{0}}};
  EXPECT_EQ(&array_value, g_value_init(&array_value, G_TYPE_ARRAY));
  g_value_take_boxed(&array_value, array);
  g_hash_table_insert(manager_hash_table,
                      const_cast<char*>("Services"),
                      &array_value);

  // Set up return value for second dbus call (to Service object).

  GHashTable* service_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
  ScopedRelease<GHashTable*, g_hash_table_unref> service_hash_table_unref(
      service_hash_table);
      
  GValue service_type_value = {0, {{0}}};
  EXPECT_EQ(&service_type_value,
            g_value_init(&service_type_value, G_TYPE_STRING));
  g_value_set_static_string(&service_type_value, service_type);
  
  g_hash_table_insert(service_hash_table,
                      const_cast<char*>("Type"),
                      &service_type_value);

  EXPECT_CALL(dbus_iface, ProxyCall(kMockFlimFlamManagerProxy,
                                    StrEq(kGetPropertiesMethod),
                                    _,
                                    G_TYPE_INVALID,
                                    dbus_g_type_get_map("GHashTable",
                                                        G_TYPE_STRING,
                                                        G_TYPE_VALUE),
                                    _,
                                    G_TYPE_INVALID))
      .WillOnce(DoAll(SetArgumentPointee<5>(manager_hash_table), Return(TRUE)));

  EXPECT_CALL(dbus_iface, ProxyCall(kMockFlimFlamServiceProxy,
                                    StrEq(kGetPropertiesMethod),
                                    _,
                                    G_TYPE_INVALID,
                                    dbus_g_type_get_map("GHashTable",
                                                        G_TYPE_STRING,
                                                        G_TYPE_VALUE),
                                    _,
                                    G_TYPE_INVALID))
      .WillOnce(DoAll(SetArgumentPointee<5>(service_hash_table), Return(TRUE)));

  NetworkConnectionType type;
  
  EXPECT_TRUE(FlimFlamProxy::GetConnectionType(&dbus_iface, &type));
  EXPECT_EQ(expected_type, type);
}

TEST_F(FlimFlamProxyTest, SimpleTest) {
  TestWithServiceType(kFlimFlamNetTypeEthernet, kNetEthernet);
  TestWithServiceType(kFlimFlamNetTypeWifi, kNetWifi);
  TestWithServiceType(kFlimFlamNetTypeWimax, kNetWimax);
  TestWithServiceType(kFlimFlamNetTypeBluetooth, kNetBluetooth);
  TestWithServiceType(kFlimFlamNetTypeCellular, kNetCellular);
}

TEST_F(FlimFlamProxyTest, UnknownTest) {
  TestWithServiceType("foo", kNetUnknown);
}

TEST_F(FlimFlamProxyTest, ExpensiveConnectionsTest) {
  EXPECT_FALSE(FlimFlamProxy::IsExpensiveConnectionType(kNetEthernet));
  EXPECT_FALSE(FlimFlamProxy::IsExpensiveConnectionType(kNetWifi));
  EXPECT_TRUE(FlimFlamProxy::IsExpensiveConnectionType(kNetWimax));
  EXPECT_TRUE(FlimFlamProxy::IsExpensiveConnectionType(kNetBluetooth));
  EXPECT_TRUE(FlimFlamProxy::IsExpensiveConnectionType(kNetCellular));
}

TEST_F(FlimFlamProxyTest, StringForConnectionTypeTest) {
  EXPECT_EQ(kFlimFlamNetTypeEthernet,
            FlimFlamProxy::StringForConnectionType(kNetEthernet));
  EXPECT_EQ(kFlimFlamNetTypeWifi,
            FlimFlamProxy::StringForConnectionType(kNetWifi));
  EXPECT_EQ(kFlimFlamNetTypeWimax,
            FlimFlamProxy::StringForConnectionType(kNetWimax));
  EXPECT_EQ(kFlimFlamNetTypeBluetooth,
            FlimFlamProxy::StringForConnectionType(kNetBluetooth));
  EXPECT_EQ(kFlimFlamNetTypeCellular,
            FlimFlamProxy::StringForConnectionType(kNetCellular));
  EXPECT_EQ("Unknown", FlimFlamProxy::StringForConnectionType(kNetUnknown));
  EXPECT_EQ("Unknown", FlimFlamProxy::StringForConnectionType(
      static_cast<NetworkConnectionType>(999999)));
}

}  // namespace chromeos_update_engine
