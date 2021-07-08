#pragma once

#include "meas_config.h"
#include "rtu_context.hpp"

#include <chrono>
#include <loguru.hpp>
#include <unordered_map>
namespace infra {
class PeriodicScheduler;
}
namespace measure {

class Reporter;

class Executor
{
    // An unorderd_set would be the right choice, as we're not going to need to
    // search through the container, but that would force const on all
    // the elements and that doesn't work well with the lower level modbus C-api
    // which works with (non-const) modbus_t *
    std::unordered_map<measure::server_id_t, modbus::RTUContext> mbcxts_;

    void add_schedule(infra::PeriodicScheduler &scheduler,
                      Reporter &reporter,
                      modbus::RTUContext &modbus_cxt,
                      std::vector<measure_t> const &measures);

public:
    Executor(infra::PeriodicScheduler &scheduler,
             Reporter &reporter,
             configuration_map_t const &configmap)
    {
        for (auto const &el: configmap)
        {
            auto const &server_config = el.second.server;

            auto cxt_insertion_result = [&]()
            {
                if (server_config.serial_device.empty())
                {
                    // A for-testing-only RANDOM measurements generator

                    return mbcxts_.try_emplace(
                      // Key
                      server_config.modbus_id,
                      // Args for RANDOM-sourced-RTUContext ctor
                      server_config.modbus_id,
                      server_config.name,
                      modbus::RandomParams(server_config.line_config),
                      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);
                }
                else
                {
                    // The real-modbus data source

                    return mbcxts_.try_emplace(
                      // Key
                      server_config.modbus_id,
                      // Args for MODBUS-sourced-RTUContext ctor
                      server_config.modbus_id,
                      server_config.name,
                      modbus::SerialLine(server_config.serial_device,
                                         server_config.line_config),
                      server_config.answering_time,
                      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);
                }
            }();


            if (!cxt_insertion_result.second)
                throw std::runtime_error(
                  "Failed creating RTUContext for modbus id " +
                  std::to_string(server_config.modbus_id));

            add_schedule(scheduler,
                         reporter,
                         cxt_insertion_result.first->second,
                         el.second.measures);
        }
    }
};
} // namespace measure
