#include "meas_scheduler.h"

#include <thread>
#include <iostream>

namespace measure
{
void
scheduler::add_schedule(modbus::RTUContext &modbus_cxt,
                        std::vector<measure_t> const &measures)
{
    for (auto const &meas: measures)
    {
#if defined(USE_ASIO_BASED_SCHEDULER)
        impl_.addTask(
          "Server_" + std::to_string(modbus_cxt.id()) + "/" + meas.name,
          meas.sampling_period,
          [this, &modbus_cxt, meas]() {
              if (true)
              {
                  auto const nowsecs =
                    std::chrono::time_point_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now());

                  std::cout << nowsecs.time_since_epoch().count() << ": "
                            << modbus_cxt.name() << "@" << modbus_cxt.id()
                            << ": reading register " << meas.source.address
                            << "#" << meas.source.size << ": ";
              }

              int64_t value;
              try
              {
                  if (meas.source.type == modbus::regtype::holding)
                      value = modbus_cxt.read_holding_registers(
                        meas.source.address,
                        meas.source.size,
                        meas.source.endianess);
                  else
                      value =
                        modbus_cxt.read_input_registers(meas.source.address,
                                                        meas.source.size,
                                                        meas.source.endianess);

                  if (true)
                      std::cout << value * meas.source.scale_factor << '\n';
              }
              catch (std::exception &e)
              {
                  std::cerr << "******** FAILED *******\n";
              }
          });
#else
        impl_.Schedule(
          meas.sampling_period,
          modbus_cxt.id(),
          [this, &modbus_cxt, meas](tsc::TaskContext tc) mutable {
              if (true)
              {
                  auto const nowsecs =
                    std::chrono::time_point_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now());

                  std::cout << nowsecs.time_since_epoch().count() << ": "
                            << modbus_cxt.name() << "@" << modbus_cxt.id()
                            << ": reading register " << meas.source.address
                            << "#" << meas.source.size << " repeat #"
                            << tc.GetRepeatCounter() << ": ";
              }

              int64_t value;
              try
              {
                  if (meas.source.type == modbus::regtype::holding)
                      value = modbus_cxt.read_holding_registers(
                        meas.source.address,
                        meas.source.size,
                        meas.source.endianess);
                  else
                      value =
                        modbus_cxt.read_input_registers(meas.source.address,
                                                        meas.source.size,
                                                        meas.source.endianess);

                  if (true)
                      std::cout << value * meas.source.scale_factor << '\n';
              }
              catch (std::exception &e)
              {
                  std::cerr << "******** FAILED *******\n";
              }

              tc.Repeat();
          });
#endif
    }
}

#if defined (USE_ASIO_BASED_SCHEDULER)
    int scheduler::run_loop(std::chrono::milliseconds)
    {
        return impl_.run();
    }
#else
    int scheduler::run_loop(std::chrono::milliseconds update_period)
    {
        while (true)
        {
            impl_.Update();
            std::this_thread::sleep_for(update_period);
        }

        return 0;
    }
#endif
}// namespace measure
