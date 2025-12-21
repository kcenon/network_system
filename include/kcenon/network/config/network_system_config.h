#pragma once

#include <memory>

#include "kcenon/network/config/network_config.h"

namespace kcenon::common::interfaces {
class IExecutor;
class ILogger;
class IMonitor;
} // namespace kcenon::common::interfaces

namespace kcenon::network::config {

struct network_system_config {
    network_config runtime{network_config::production()};
    std::shared_ptr<kcenon::common::interfaces::IExecutor> executor;
    std::shared_ptr<kcenon::common::interfaces::ILogger> logger;
    std::shared_ptr<kcenon::common::interfaces::IMonitor> monitor;
};

} // namespace kcenon::network::config
