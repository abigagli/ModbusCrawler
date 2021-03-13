#include "meas_executor.h"

#include "meas_reporter.h"
#include "periodic_scheduler.h"

#include <cassert>
#include <cmath>
#include <loguru.hpp>
#include <sstream>

namespace measure {

void
Executor::add_schedule(infra::PeriodicScheduler &scheduler,
                       Reporter &reporter,
                       modbus::RTUContext &modbus_cxt,
                       std::vector<measure_t> const &measures)
{
    for (auto const &meas: measures)
    {
        assert(meas.enabled);

        auto const meas_task = [this, &reporter, &modbus_cxt, meas]()
        {
            Reporter::when_t const nowsecs =
              std::chrono::time_point_cast<Reporter::when_t::duration>(
                Reporter::when_t::clock::now());

            std::ostringstream msg;
            auto const period = meas.sampling_period.count();

            msg << nowsecs.time_since_epoch().count() << "->" << period << '|'
                << modbus_cxt.name() << "@" << modbus_cxt.id() << '|'
                << meas.name;

            double measurement = std::numeric_limits<double>::quiet_NaN();
            if (meas.source)
            {
                // Normal case: reading from a real modbus device
                auto const source_value = meas.source.value();
                auto const reg_size = modbus::reg_size(meas.value_type.value());
                auto const value_signed =
                  modbus::value_signed(meas.value_type.value());

                msg << '|' << source_value.address << "#" << reg_size
                    << (value_signed ? 'I' : 'U');

                int64_t reg_value;
                try
                {
                    LOG_SCOPE_F(1, "Reading register");
                    if (source_value.type == modbus::regtype::holding)
                        reg_value = modbus_cxt.read_holding_registers(
                          source_value.address,
                          reg_size,
                          source_value.endianess);
                    else
                        reg_value = modbus_cxt.read_input_registers(
                          source_value.address,
                          reg_size,
                          source_value.endianess);

                    msg << '|' << std::hex << reg_value << std::dec;

                    if (value_signed)
                    {
                        measurement = static_cast<double>(reg_value) *
                                      source_value.scale_factor;
                    }
                    else
                    {
                        measurement = static_cast<double>(
                                        static_cast<uint64_t>(reg_value)) *
                                      source_value.scale_factor;
                    }
                }
                catch (std::exception &e)
                {
                    LOG_S(ERROR) << msg.str() << "|FAILED:" << e.what();
                }
            }
            else
            {
                // Testing case: get some random numbers
                LOG_SCOPE_F(1, "Reading random");
                measurement = modbus_cxt.read_random_value();
            }

            reporter.add_measurement({modbus_cxt.name(), modbus_cxt.id()},
                                     meas.name,
                                     nowsecs,
                                     measurement);

            LOG_IF_S(INFO, !std::isnan(measurement))
              << msg.str() << '|' << measurement;
        };

        scheduler.addTask("Server_" + std::to_string(modbus_cxt.id()) + "/" +
                            meas.name,
                          meas.sampling_period,
                          meas_task,
                          infra::PeriodicScheduler::TaskMode::execute_at_start);
    }
}
} // namespace measure
