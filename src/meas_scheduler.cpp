#include "meas_scheduler.h"

#include "meas_report.h"

#include <cmath>
#include <iostream>
#include <thread>

constexpr bool logging = true;

namespace measure {

void
scheduler::add_schedule(modbus::RTUContext &modbus_cxt,
                        std::vector<measure_t> const &measures)
{
    for (auto const &meas: measures)
    {
        auto const meas_task = [this, &modbus_cxt, meas]() {
            Report::when_t const nowsecs =
              std::chrono::time_point_cast<Report::when_t::duration>(
                Report::when_t::clock::now());

            if (logging)
            {
                auto const period = meas.sampling_period.count();

                std::cerr << nowsecs.time_since_epoch().count() << "->"
                          << period << ' ' << modbus_cxt.name() << "@"
                          << modbus_cxt.id() << " " << meas.name;
            }

            double measurement = std::numeric_limits<double>::quiet_NaN();
            if (meas.source)
            {
                // Normal case: reading from a real modbus device
                auto const source_value = meas.source.value();

                if (logging)
                {
                    std::cerr << ": reading register " << source_value.address
                              << "#" << source_value.size << ": ";
                }

                int64_t reg_value;
                try
                {
                    if (source_value.type == modbus::regtype::holding)
                        reg_value = modbus_cxt.read_holding_registers(
                          source_value.address,
                          source_value.size,
                          source_value.endianess);
                    else
                        reg_value = modbus_cxt.read_input_registers(
                          source_value.address,
                          source_value.size,
                          source_value.endianess);

                    measurement = reg_value * source_value.scale_factor;
                }
                catch (std::exception &e)
                {
                    std::cerr << "******** FAILED *******\n";
                }
            }
            else
            {
                // Testing case: get some random numbers
                if (logging)
                {
                    std::cerr << ": ";
                }
                measurement = modbus_cxt.read_random_value();
            }

            report_.add_measurement({modbus_cxt.name(), modbus_cxt.id()},
                                    meas.name,
                                    nowsecs,
                                    measurement);
            if (logging)
                std::cerr << measurement << '\n';
        };

#if defined(USE_ASIO_BASED_SCHEDULER)
        impl_.addTask("Server_" + std::to_string(modbus_cxt.id()) + "/" +
                        meas.name,
                      meas.sampling_period,
                      meas_task);
#else
        impl_.Schedule(meas.sampling_period,
                       modbus_cxt.id(),
                       [meas_task](tsc::TaskContext tc) {
                           meas_task();
                           tc.Repeat();
                       });
#endif
    }
}

#if defined(USE_ASIO_BASED_SCHEDULER)
int
scheduler::run_loop(std::chrono::seconds reporting_period)
{
    return impl_.run(report_, reporting_period);
}
#else
int scheduler::run_loop(std::chrono::seconds)
{
    while (true)
    {
        impl_.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
#endif
} // namespace measure
