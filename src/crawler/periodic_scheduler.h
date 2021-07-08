#pragma once

#include "infra.hpp"

#if defined(ASIO_STANDALONE)
#    include <asio.hpp>
#else
#    include <boost/asio.hpp>
#endif

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace infra {
#if defined(ASIO_STANDALONE)
using asio::io_context;
using asio::steady_timer;
using asio::system_timer;
using asio::error_code;
#else
using boost::asio::io_context;
using boost::asio::steady_timer;
using boost::asio::system_timer;
using boost::system::error_code;
#endif

using task_t = std::function<void(infra::when_t)>;

class PeriodicScheduler
{
public:
    enum class TaskMode
    {
        execute_at_multiples_of_period,
        execute_at_start,
        skip_first_execution,
    };

private:
    class scheduled_task
    {
        using timer_t = system_timer;

    public:
        // Can't easily move these around, since the constructor immediately
        // posts onto the io_context, so any copy / move operation would happen
        // when something has already been scheduled... NOT-GOOD! Let's make
        // this non-copy/non-move -able
        scheduled_task(scheduled_task const&) = delete;
        scheduled_task& operator=(scheduled_task const&) = delete;

        scheduled_task(io_context& io_context,
                       std::string name,
                       std::chrono::seconds interval,
                       task_t task,
                       TaskMode mode);

        void execute(error_code const& e);

        void start(TaskMode mode);
        void cancel();
        timer_t::clock_type::time_point nextExpiry() const
        {
            return timer_.expiry();
        }

    private:
        void start_wait();

        io_context& io_context_;
        timer_t timer_;
        task_t task_;
        std::string name_;
        std::chrono::seconds interval_;
    };

public:
    unsigned long run();

    void addTask(std::string const& name,
                 std::chrono::seconds interval,
                 task_t const& task,
                 TaskMode mode);

private:
    io_context io_context_;
    // Need to hold periodic_task behind a pointer as they're non-copy/non-move
    std::vector<std::unique_ptr<scheduled_task>> tasks_;
};
} // namespace infra
