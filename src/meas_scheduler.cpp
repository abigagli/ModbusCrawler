#include "meas_scheduler.h"

#include "meas_reporter.h"

#include <cmath>
#include <sstream>
#include <thread>

constexpr bool logging = true;

namespace measure {

void
scheduler::add_schedule(modbus::RTUContext &modbus_cxt,
                        std::vector<measure_t> const &measures)
{
    for (auto const &meas: measures)
    {
        auto const meas_task = [this, &modbus_cxt, meas]()
        {
            Reporter::when_t const nowsecs =
              std::chrono::time_point_cast<Reporter::when_t::duration>(
                Reporter::when_t::clock::now());

            std::ostringstream msg;
            auto const period = meas.sampling_period.count();

            msg << nowsecs.time_since_epoch().count() << "->" << period << ' '
                << modbus_cxt.name() << "@" << modbus_cxt.id() << " "
                << meas.name;

            double measurement = std::numeric_limits<double>::quiet_NaN();
            if (meas.source)
            {
                // Normal case: reading from a real modbus device
                auto const source_value = meas.source.value();

                msg << ": reading register " << source_value.address << "#"
                    << source_value.size << ": ";

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

                    measurement = static_cast<double>(reg_value) *
                                  source_value.scale_factor;
                }
                catch (std::exception &e)
                {
                    LOG_S(ERROR) << msg.str() << "******** FAILED *******";
                }
            }
            else
            {
                // Testing case: get some random numbers
                msg << ": ";
                measurement = modbus_cxt.read_random_value();
            }

            report_.add_measurement({modbus_cxt.name(), modbus_cxt.id()},
                                    meas.name,
                                    nowsecs,
                                    measurement);

            LOG_IF_S(INFO, !std::isnan(measurement))
              << msg.str() << measurement;
        };

        impl_.addTask("Server_" + std::to_string(modbus_cxt.id()) + "/" +
                        meas.name,
                      meas.sampling_period,
                      meas_task);
    }
}

unsigned long
scheduler::run_loop(std::chrono::seconds reporting_period)
{
    return impl_.run(report_, reporting_period);
}
} // namespace measure
