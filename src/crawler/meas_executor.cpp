#include "meas_executor.h"

#include "infra.hpp"
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
                       modbus::slave &slave,
                       std::vector<measure_t> const &measures)
{
    for (auto const &meas: measures)
    {
        assert(meas.enabled);

        auto const meas_task =
          [this, &reporter, &slave, meas](infra::when_t nowsecs)
        {
            std::ostringstream msg;
            auto const period = meas.sampling_period.count();

            msg << nowsecs.time_since_epoch().count() << "->" << period << '|'
                << slave.name() << "@" << slave.id() << '|' << meas.name;

            Reporter::SampleType sample_type =
              Reporter::SampleType::read_failure;
            double measurement = std::numeric_limits<double>::quiet_NaN();

            auto const &source_value = meas.source;
            auto const reg_size = modbus::reg_size(source_value.value_type);
            auto const value_signed =
              modbus::value_signed(source_value.value_type);

            msg << '|' << source_value.address << "#" << reg_size
                << (value_signed ? 'I' : 'U');

            intmax_t reg_value;
            try
            {
                LOG_SCOPE_F(1, "Reading register");
                if (source_value.reg_type == modbus::regtype::holding)
                    reg_value = slave.read_holding_registers(
                      source_value.address, reg_size, source_value.endianess);
                else
                    reg_value = slave.read_input_registers(
                      source_value.address, reg_size, source_value.endianess);

                msg << '|' << reg_value << '(' << std::hex << reg_value
                    << std::dec << ')';

                if (value_signed)
                {
                    intmax_t const min_threshold =
                      source_value.min_read_value.as_signed();
                    intmax_t const max_threshold =
                      source_value.max_read_value.as_signed();
                    if (reg_value < min_threshold)
                    {
                        sample_type = Reporter::SampleType::underflow;
                        LOG_S(WARNING)
                          << msg.str() << "|UNDERFLOW: " << reg_value << " < "
                          << min_threshold;
                    }
                    else if (reg_value > max_threshold)
                    {
                        sample_type = Reporter::SampleType::overflow;
                        LOG_S(WARNING)
                          << msg.str() << "|OVERFLOW: " << reg_value << " > "
                          << max_threshold;
                    }
                    else
                    {
                        sample_type = Reporter::SampleType::regular;
                        measurement = static_cast<double>(reg_value) *
                                      source_value.scale_factor;
                    }
                }
                else
                {
                    uintmax_t const min_threshold =
                      source_value.min_read_value.as_unsigned();
                    uintmax_t const max_threshold =
                      source_value.max_read_value.as_unsigned();
                    uintmax_t const unsigned_value =
                      static_cast<uintmax_t>(reg_value);

                    if (unsigned_value < min_threshold)
                    {
                        sample_type = Reporter::SampleType::underflow;
                        LOG_S(WARNING)
                          << msg.str() << "|UNDERFLOW: " << unsigned_value
                          << " < " << min_threshold;
                    }
                    else if (unsigned_value > max_threshold)
                    {
                        sample_type = Reporter::SampleType::overflow;
                        LOG_S(WARNING)
                          << msg.str() << "|OVERFLOW: " << unsigned_value
                          << " > " << max_threshold;
                    }
                    else
                    {
                        sample_type = Reporter::SampleType::regular;
                        measurement = static_cast<double>(unsigned_value) *
                                      source_value.scale_factor;
                    }
                }
            }
            catch (std::exception &e)
            {
                sample_type = Reporter::SampleType::read_failure;
                LOG_S(ERROR) << msg.str() << "|FAILED:" << e.what();
            }

            reporter.add_measurement({slave.name(), slave.id()},
                                     meas.name,
                                     nowsecs,
                                     measurement,
                                     sample_type);

            LOG_IF_S(INFO, sample_type == Reporter::SampleType::regular)
              << msg.str() << '|' << measurement;
        };

        scheduler.addTask("Server_" + std::to_string(slave.id()) + "/" +
                            meas.name,
                          meas.sampling_period,
                          meas_task,
                          infra::PeriodicScheduler::TaskMode::execute_at_start);
    }
}
} // namespace measure
