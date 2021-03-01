#include "meas_scheduler.h"

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
            if (logging)
            {
                auto const nowsecs =
                  std::chrono::time_point_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now());

                auto const period = meas.sampling_period.count();

                std::cout << nowsecs.time_since_epoch().count() << "->"
                          << period << ' ' << modbus_cxt.name() << "@"
                          << modbus_cxt.id() << " " << meas.name;
            }

            double measurement;
            if (meas.source)
            {
                auto const source_value = meas.source.value();

                if (logging)
                {
                    std::cout << ": reading register " << source_value.address
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
                }
                catch (std::exception &e)
                {
                    std::cerr << "******** FAILED *******\n";
                    return;
                }
                measurement = reg_value * source_value.scale_factor;
            }
            else
            {
                if (logging)
                {
                    std::cout << ": ";
                }
                measurement = modbus_cxt.read_random_value();
            }

            if (logging)
                std::cout << measurement << '\n';
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
int scheduler::run_loop(std::chrono::milliseconds)
{
    return impl_.run();
}
#else
int
scheduler::run_loop(std::chrono::milliseconds update_period)
{
    while (true)
    {
        impl_.Update();
        std::this_thread::sleep_for(update_period);
    }

    return 0;
}
#endif
} // namespace measure
