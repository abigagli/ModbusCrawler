#include "meas_scheduler.h"

#include <thread>
#include <iostream>


using namespace tsc;
namespace measure
{
    int scheduler::run()
    {
    impl_.Schedule(std::chrono::seconds(3), [](TaskContext context)
                       {
                       std::cout << "******** EXCUTING PERIOD 3 # " << context.GetRepeatCounter() << " ********\n";

                       context.Repeat();
                       });

    impl_.Schedule(std::chrono::seconds(2), [](TaskContext context)
                       {
                       std::cout << "******** EXCUTING PERIOD 2 # " << context.GetRepeatCounter() << " ********\n";

                       context.Repeat();
                       });
    while (true)
    {
        std::cout << "SCHEDULING\n";
        impl_.Update();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
    }
}// namespace measure