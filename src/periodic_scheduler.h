#pragma once

#if defined (ASIO_STANDALONE)
#include <asio.hpp>
#else
#include <boost/asio.hpp>
#endif

#include <functional>
#include <chrono>
#include <vector>
#include <string>


namespace measure {
#if defined (ASIO_STANDALONE)
    using asio::io_context;
    using asio::steady_timer;
    using asio::error_code;
#else
    using boost::asio::io_context;
    using boost::asio::steady_timer;
    using boost::system::error_code;
#endif

using task_t = std::function<void()>;

namespace detail {
class periodic_task
{
public:
    periodic_task (periodic_task const &) = delete;
    periodic_task& operator=(periodic_task const &) = delete;


    periodic_task(io_context& io_context
        , std::string const& name
        , std::chrono::seconds interval
        , task_t const &task);

    void execute(error_code const& e);

    void start();

private:
    void start_wait();

    io_context& io_context_;
    steady_timer timer_;
    task_t task_;
    std::string name_;
    std::chrono::seconds interval_;
};
}// namespace detail

class PeriodicScheduler
{
public:
    int run();

    void addTask(std::string const& name
        , std::chrono::seconds interval
        , task_t const& task);

private:
    io_context io_context_;
    std::vector<std::unique_ptr<detail::periodic_task>> tasks_;
};
}// namespace measure
