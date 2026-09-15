#ifndef AIOT_DEVICE_H
#define AIOT_DEVICE_H

#include "Device.hpp"

// Định nghĩa instance toàn cục duy nhất cho C++14 trở xuống
#if __cplusplus < 201703L
#if !defined(AIOT_DEVICE_INSTANCE_DEFINED)
#define AIOT_DEVICE_INSTANCE_DEFINED
AIoTDeviceManager AIoT_Device;
#endif
#endif

#endif /* AIOT_DEVICE_H */

