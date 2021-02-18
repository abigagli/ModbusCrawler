#include "periodic_scheduler.h"

namespace measure {

#if !defined (ASIO_STANDALONE)
    using namespace boost;
#endif

namespace detail {

periodic_task::periodic_task(io_context& io_context
    , std::string const& name
    , std::chrono::seconds interval
    , task_t const &task)
    : io_context_(io_context)
    , timer_(io_context)
    , task_(task)
    , name_(name)
    , interval_(interval)
{
    // Schedule start to be ran by the io_context
    io_context_.post([this](){
                    start();
                    });
}

void periodic_task::execute(error_code const& e)
{
    if (e != asio::error::operation_aborted)
    {
        task_();

        timer_.expires_at(timer_.expires_at() + interval_);
        start_wait();
    }
}

void periodic_task::start()
{
    // Comment if you want to avoid calling the handler on startup (i.e. at time 0)
    task_();

    timer_.expires_from_now(interval_);
    start_wait();
}

void periodic_task::start_wait()
{
    timer_.async_wait([this](error_code const &e) {
                        execute(e);
                        });
}
}// namespace detail

int PeriodicScheduler::run()
{
    return io_context_.run();
}

void PeriodicScheduler::addTask(std::string const& name
    , std::chrono::seconds interval
    , task_t const& task)
{
    tasks_.push_back(
        std::make_unique<detail::periodic_task>(io_context_, name, interval, task));
}

}// namespace measure

