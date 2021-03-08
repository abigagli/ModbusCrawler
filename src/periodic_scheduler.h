#pragma once

#if defined(ASIO_STANDALONE)
#    include <asio.hpp>
#else
#    include <boost/asio.hpp>
#endif

#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include <memory>


namespace infra {
#if defined(ASIO_STANDALONE)
using asio::io_context;
using asio::steady_timer;
using asio::error_code;
#else
using boost::asio::io_context;
using boost::asio::steady_timer;
using boost::system::error_code;
#endif

using task_t = std::function<void()>;

class PeriodicScheduler
{
    class scheduled_task
    {
    public:
        // Can't easily move these around, since the constructor immediately posts
        // onto the io_context, so any copy / move operation would happen when something
        // has already been scheduled... NOT-GOOD!
        // Let's make this non-copy / non-moveable
        scheduled_task(scheduled_task const &) = delete;
        scheduled_task&operator=(scheduled_task const &) = delete;

        scheduled_task(io_context& io_context,
                      std::string name,
                      std::chrono::seconds interval,
                      task_t task,
                      bool execute_at_start);

        void execute(error_code const& e);

        void start(bool execute_at_start);
        void cancel();

    private:
        void start_wait();

        io_context& io_context_;
        steady_timer timer_;
        task_t task_;
        std::string name_;
        std::chrono::seconds interval_;
    };
public:
    unsigned long run();

    void addTask(std::string const& name,
                 std::chrono::seconds interval,
                 task_t const& task,
                 bool execute_at_start);

private:
    io_context io_context_;
    // Need to hold periodic_task behind a pointer as they're non-copyable / non-moveable
    std::vector<std::unique_ptr<scheduled_task>> tasks_;
};
} // namespace infra
