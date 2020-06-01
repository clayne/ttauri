// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/GUI/Instance_vulkan.hpp"
#include <nonstd/span>

namespace TTauri::GUI {

class Instance_vulkan_win32 final: public Instance_vulkan {
public:
    Instance_vulkan_win32(InstanceDelegate *delegate);
    ~Instance_vulkan_win32();

    Instance_vulkan_win32(const Instance_vulkan_win32 &) = delete;
    Instance_vulkan_win32 &operator=(const Instance_vulkan_win32 &) = delete;
    Instance_vulkan_win32(Instance_vulkan_win32 &&) = delete;
    Instance_vulkan_win32 &operator=(Instance_vulkan_win32 &&) = delete;

    vk::ResultValueType<vk::SurfaceKHR>::type createWin32SurfaceKHR(const vk::Win32SurfaceCreateInfoKHR& createInfo) const {
        auto lock = std::scoped_lock(guiMutex);
        return intrinsic.createWin32SurfaceKHR(createInfo);
    }
};

}
