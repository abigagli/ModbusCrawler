#pragma once

#include "meas_config.h"
#include "rtu_context.hpp"

#if defined(USE_ASIO_BASED_SCHEDULER)
#    include "periodic_scheduler.h"
#else
#    include <TaskScheduler.hpp>
#    include <thread>
#endif

#include <chrono>
#include <unordered_map>

namespace measure {
class scheduler
{
#if defined(USE_ASIO_BASED_SCHEDULER)
    PeriodicScheduler impl_;
#else
    tsc::TaskScheduler impl_;
#endif
    // An unorderd_set would be the right choice, as we're not going to need to
    // search through the container, but that would force const on all
    // the elements and that doesn't work well with the lower level modbus C-api
    // which works with (non-const) modbus_t *
    std::unordered_map<measure::server_id_t, modbus::RTUContext> mbcxts_;
    bool verbose_;

    void add_schedule(modbus::RTUContext &modbus_cxt,
                      std::vector<measure_t> const &measures);

public:
    scheduler(configuration_map_t const &configmap, bool verbose = false)
      : verbose_(verbose)
    {
        for (auto const &el: configmap)
        {
            auto const &server_config = el.second.server;

            auto cxt_insertion_result = mbcxts_.try_emplace(
              // Key
              server_config.modbus_id,
              // Args for RTUContext ctor
              server_config.modbus_id,
              server_config.name,
              modbus::SerialLine(server_config.serial_device,
                                 server_config.line_config),
              server_config.answering_time,
              verbose_);


            if (!cxt_insertion_result.second)
                throw std::runtime_error(
                  "Failed creating RTUContext for modbus id " +
                  std::to_string(server_config.modbus_id));

            add_schedule(cxt_insertion_result.first->second,
                         el.second.measures);
        }
    }

    int run_loop(std::chrono::milliseconds update_period);
};
} // namespace measure
