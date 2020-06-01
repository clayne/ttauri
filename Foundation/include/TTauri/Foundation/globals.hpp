// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Foundation/exceptions.hpp"
#include <nonstd/span>
#include <string>
#include <unordered_map>
#include <cstddef>
#include <thread>
#include <mutex>

namespace TTauri {

/** The system timezone.
 */
inline date::time_zone const *timeZone = nullptr;

/** Thread id of the main thread.
 */
inline std::thread::id mainThreadID;

/** Function which will pass a given function to the main thread.
 */
inline std::function<void(std::function<void()>)> mainThreadRunner;

/** The global configuration.
 */
inline datum configuration;

/** The name of the application.
 */
inline std::string applicationName;

/** Reference counter to determine the amount of startup/shutdowns.
*/
inline std::atomic<uint64_t> startupCount = 0;

/** Add a static resource.
*/
void addStaticResource(std::string const &key, nonstd::span<std::byte const> value) noexcept;

/** Request a static resource.
*/
nonstd::span<std::byte const> getStaticResource(std::string const &key);

/** Stop the maintenance thread.
 */
void stopMaintenanceThread() noexcept;

/** Startup the Foundation library.
*/
void startup();

/** Shutdown the Foundation library.
*/
void shutdown();

}
