#pragma once

#include "meas_config.h"
#include "modbus_slave.hpp"

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
    std::unordered_map<modbus::slave_id_t, modbus::slave> slaves_;

    void add_schedule(infra::PeriodicScheduler &scheduler,
                      Reporter &reporter,
                      modbus::slave &slave,
                      std::vector<measure_t> const &measures);

public:
    Executor(infra::PeriodicScheduler &scheduler,
             Reporter &reporter,
             configuration_map_t const &configmap)
    {
        for (auto const &el: configmap)
        {
            auto const &server_config = el.second.server;

            auto slave_insertion_result = [&]()
            {
                if (server_config.serial_device.empty())
                {
                    // A RANDOM measurements generator for testing purposes....

                    // Collect the random generator MEAN/STDEV parameters
                    std::map<int, modbus::RandomSlave::random_params>
                      random_params;

                    for (auto const &m: el.second.measures)
                        random_params.try_emplace(m.source.address,
                                                  m.source.random_mean_dev);

                    return slaves_.try_emplace(
                      // Key
                      server_config.modbus_id,
                      // Args for RANDOM Slave
                      compiler::undeduced<modbus::RandomSlave>{},
                      server_config.modbus_id,
                      server_config.name,
                      random_params,
                      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);
                }
                else
                {
                    // The real modbus slave data-source...

                    return slaves_.try_emplace(
                      // Key
                      server_config.modbus_id,
                      // Args for MODBUS Slave
                      compiler::undeduced<modbus::RTUSlave>{},
                      server_config.modbus_id,
                      server_config.name,
                      modbus::RTUSlave::serial_line(server_config.serial_device,
                                                    server_config.line_config),
                      server_config.answering_time,
                      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);
                }
            }();


            if (!slave_insertion_result.second)
                throw std::runtime_error(
                  "Failed creating modbus slave for modbus id " +
                  std::to_string(server_config.modbus_id));

            add_schedule(scheduler,
                         reporter,
                         slave_insertion_result.first->second,
                         el.second.measures);
        }
    }
};
} // namespace measure
