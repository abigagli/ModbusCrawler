#include "meas_scheduler.h"

#include <thread>
#include <iostream>


using namespace tsc;
namespace measure
{
void
scheduler::add_schedule(modbus::RTUContext &modbus_cxt,
                        std::vector<measure_t> const &measures)
{
    for (auto const &meas: measures)
    {
        impl_.Schedule(
          meas.sampling_period,
          modbus_cxt.id(),
          [this, &modbus_cxt, meas](tsc::TaskContext tc) mutable {
              if (true)
              {
                  std::cout << "Server " << modbus_cxt.name() << "@"
                            << modbus_cxt.id() << ": reading register "
                            << meas.source.address << "#" << meas.source.size
                            << " for the " << tc.GetRepeatCounter() << "time: ";
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
                      std::cout << value << '\n';
              }
              catch (std::exception &e)
              {
                std::cerr << "******** FAILED *******\n";
              }

            tc.Repeat();

          });
    }

    }

    int scheduler::run_loop()
    {
    // impl_.Schedule(std::chrono::seconds(3), [](TaskContext context)
    //                    {
    //                    std::cout << "******** EXCUTING PERIOD 3 # " << context.GetRepeatCounter() << " ********\n";

    //                    context.Repeat();
    //                    });

    // impl_.Schedule(std::chrono::seconds(2), [](TaskContext context)
    //                    {
    //                    std::cout << "******** EXCUTING PERIOD 2 # " << context.GetRepeatCounter() << " ********\n";

    //                    context.Repeat();
    //                    });
    while (true)
    {
        std::cout << "SCHEDULING\n";
        impl_.Update();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
    }
}// namespace measure