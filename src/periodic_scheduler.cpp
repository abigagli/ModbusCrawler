#include "periodic_scheduler.h"

#include "meas_report.h"

#include <iostream>

namespace measure {

#if !defined(ASIO_STANDALONE)
using namespace boost;
#endif
namespace detail {

    periodic_task::periodic_task(io_context& io_context,
                                 std::string name,
                                 std::chrono::seconds interval,
                                 task_t task,
                                 bool execute_at_start /* = true */)
      : io_context_(io_context)
      , timer_(io_context)
      , task_(std::move(task))
      , name_(std::move(name))
      , interval_(interval)
    {
        // Schedule start to be ran by the io_context
        io_context_.post(
          [this, execute_at_start]() { start(execute_at_start); });
    }

    void periodic_task::execute(error_code const& e)
    {
        if (e != asio::error::operation_aborted)
        {
            task_();

            timer_.expires_at(timer_.expires_at() + interval_);
            start_wait();
        }
        else
            std::cout << "Periodic task " << name_ << " CANCELLED\n";
    }

    void periodic_task::start(bool execute_at_start)
    {
        if (execute_at_start)
            task_();

        timer_.expires_from_now(interval_);
        start_wait();
    }

    void periodic_task::start_wait()
    {
        timer_.async_wait([this](error_code const& e) { execute(e); });
    }

    void periodic_task::cancel() { timer_.cancel(); }
} // namespace detail

unsigned long
PeriodicScheduler::run(Reporter& report, std::chrono::seconds reporting_period)
{
    /*
    detail::periodic_task killer(
      io_context_,
      "REPORTER",
      std::chrono::seconds(6),
      [this] {
          for (auto& t: tasks_)
          {
              auto const nowsecs =
                std::chrono::time_point_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now());

              std::cout << nowsecs.time_since_epoch().count() << ": CANCELLING"
                        << std::endl;
              t->cancel();
          }
          for (auto& t: tasks_)
          {
              io_context_.post([&t]() {
                  auto const nowsecs =
                    std::chrono::time_point_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now());

                  std::cout << nowsecs.time_since_epoch().count()
                            << ": REARMING" << std::endl;
                  t->start(true);
              });
          }
      },
      false);
    */
    detail::periodic_task report_generator(
      io_context_,
      "ReportGenerator",
      reporting_period,
      [&report]() { report.close_period(); },
      false);

    return io_context_.run();
}

void
PeriodicScheduler::addTask(std::string const& name,
                           std::chrono::seconds interval,
                           task_t const& task)
{
    tasks_.push_back(std::make_unique<detail::periodic_task>(
      io_context_, name, interval, task));
}

} // namespace measure
