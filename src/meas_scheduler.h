#pragma once

#include "meas_config.h"
#include "rtu_context.hpp"

#include <TaskScheduler.hpp>

#include <tuple>

namespace measure {
class scheduler
{
    configuration_map_t configmap_;
    tsc::TaskScheduler impl_;
    std::map<measure::server_id_t, modbus::RTUContext> mbcxts_;
    bool verbose_;

    void add_schedule (modbus::RTUContext &modbus_cxt, std::vector<measure_t> const &measures);

public:
    scheduler(configuration_map_t configmap, bool verbose = false)
      : configmap_(std::move(configmap))
      , verbose_(verbose)
    {
        for (auto const &el: configmap_)
        {
            auto const &server_config = el.second.server;

            auto cxt_insertion_result = mbcxts_.try_emplace(
                //Key
              server_config.modbus_id,
                //Args for RTUContext ctor
              server_config.modbus_id,
              server_config.name,
              modbus::SerialLine(server_config.serial_device,
                                 server_config.line_config),
              server_config.answering_time,
              verbose_);

            
            if (!cxt_insertion_result.second)
                throw std::runtime_error("Failed creating RTUContext for modbus id " + std::to_string(server_config.modbus_id));

            add_schedule(cxt_insertion_result.first->second,
                         el.second.measures);
        }
    }

    int run_loop();
};
} // namespace measure