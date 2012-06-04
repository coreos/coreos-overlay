// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>
#include <string>

#include "update_engine/flimflam_proxy.h"
#include "update_engine/mock_dbus_interface.h"

using ::testing::_;
using ::testing::AnyNumber;
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
 public:
  FlimFlamProxyTest()
      : kMockFlimFlamManagerProxy_(NULL),
        kMockFlimFlamServiceProxy_(NULL),
        kServicePath_(NULL) {}

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
};

// static
const char* FlimFlamProxyTest::kGetPropertiesMethod = "GetProperties";

void FlimFlamProxyTest::SetupMocks(const char *service_path) {
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

void FlimFlamProxyTest::SetManagerReply(gconstpointer reply_value,
                                        const GType &reply_type) {
  // Initialize return value for D-Bus call to Manager object.
  GHashTable* manager_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

  GArray* array = g_array_new(FALSE, FALSE, sizeof(const char *));
  ASSERT_TRUE(array != NULL);

  EXPECT_EQ(array, g_array_append_val(array, reply_value));
  GValue* array_as_value = g_new0(GValue, 1);
  EXPECT_EQ(array_as_value,
            g_value_init(array_as_value, reply_type));
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
                                   StrEq(kFlimFlamDbusService),
                                   StrEq(kFlimFlamDbusManagerPath),
                                   StrEq(kFlimFlamDbusManagerInterface),
                                   _))
      .WillOnce(Return(kMockFlimFlamManagerProxy_));
  EXPECT_CALL(dbus_iface_, ProxyUnref(kMockFlimFlamManagerProxy_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .RetiresOnSaturation();
}

void FlimFlamProxyTest::SetServiceReply(const char *service_type) {
  // Initialize return value for D-Bus call to Service object.
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
                                   StrEq(kFlimFlamDbusService),
                                   StrEq(kServicePath_),
                                   StrEq(kFlimFlamDbusServiceInterface),
                                   _))
      .WillOnce(Return(kMockFlimFlamServiceProxy_));
  EXPECT_CALL(dbus_iface_, ProxyUnref(kMockFlimFlamServiceProxy_));
  EXPECT_CALL(dbus_iface_, BusGet(DBUS_BUS_SYSTEM, _))
      .RetiresOnSaturation();
}

void FlimFlamProxyTest::TestWithServiceType(
    const char* service_type,
    NetworkConnectionType expected_type) {

  SetupMocks("/service/guest-network");
  SetManagerReply(kServicePath_, DBUS_TYPE_G_OBJECT_PATH_ARRAY);
  SetServiceReply(service_type);

  NetworkConnectionType type;
  EXPECT_TRUE(FlimFlamProxy::GetConnectionType(&dbus_iface_, &type));
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
  EXPECT_FALSE(FlimFlamProxy::IsExpensiveConnectionType(kNetWimax));
  EXPECT_TRUE(FlimFlamProxy::IsExpensiveConnectionType(kNetBluetooth));
  EXPECT_TRUE(FlimFlamProxy::IsExpensiveConnectionType(kNetCellular));
}

TEST_F(FlimFlamProxyTest, StringForConnectionTypeTest) {
  EXPECT_STREQ(kFlimFlamNetTypeEthernet,
               FlimFlamProxy::StringForConnectionType(kNetEthernet));
  EXPECT_STREQ(kFlimFlamNetTypeWifi,
               FlimFlamProxy::StringForConnectionType(kNetWifi));
  EXPECT_STREQ(kFlimFlamNetTypeWimax,
               FlimFlamProxy::StringForConnectionType(kNetWimax));
  EXPECT_STREQ(kFlimFlamNetTypeBluetooth,
               FlimFlamProxy::StringForConnectionType(kNetBluetooth));
  EXPECT_STREQ(kFlimFlamNetTypeCellular,
               FlimFlamProxy::StringForConnectionType(kNetCellular));
  EXPECT_STREQ("Unknown",
               FlimFlamProxy::StringForConnectionType(kNetUnknown));
  EXPECT_STREQ("Unknown",
               FlimFlamProxy::StringForConnectionType(
                   static_cast<NetworkConnectionType>(999999)));
}

TEST_F(FlimFlamProxyTest, MalformedServiceList) {
  SetupMocks("/service/guest-network");
  std::string service_name(kServicePath_);
  SetManagerReply(&service_name, DBUS_TYPE_G_STRING_ARRAY);

  NetworkConnectionType type;
  EXPECT_FALSE(FlimFlamProxy::GetConnectionType(&dbus_iface_, &type));
}

}  // namespace chromeos_update_engine
