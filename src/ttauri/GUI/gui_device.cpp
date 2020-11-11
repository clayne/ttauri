// Copyright 2019 Pokitec
// All rights reserved.

#include "gui_device.hpp"
#include "Window_base.hpp"
#include <fmt/format.h>
#include <tuple>
#include <vector>

namespace tt {

using namespace std;

gui_device::gui_device(gui_system &system) noexcept : system(system)
{
}

gui_device::~gui_device()
{
    windows.clear();
}

std::string gui_device::string() const noexcept
{
    ttlet lock = std::scoped_lock(gui_system_mutex);

    return fmt::format("{0:04x}:{1:04x} {2} {3}", vendorID, deviceID, deviceName, deviceUUID.UUIDString());
}

void gui_device::initializeDevice(Window_base const &window)
{
    ttlet lock = std::scoped_lock(gui_system_mutex);

    state = State::READY_TO_DRAW;
}

void gui_device::add(std::shared_ptr<Window_base> window)
{
    ttlet lock = std::scoped_lock(gui_system_mutex);

    if (state == State::NO_DEVICE) {
        initializeDevice(*window);
    }

    window->setDevice(this);
    windows.push_back(std::move(window));
}

void gui_device::remove(Window_base &window) noexcept
{
    ttlet lock = std::scoped_lock(gui_system_mutex);

    window.unsetDevice();
    windows.erase(std::find_if(windows.begin(), windows.end(), [&](auto &x) {
        return x.get() == &window;
    }));
}

}
