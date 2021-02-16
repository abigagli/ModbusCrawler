#pragma once
#include <boost/asio.hpp>

#include <functional>
#include <chrono>
#include <vector>
#include <string>


namespace measure {

using task_t = std::function<void()>;

namespace detail {
class periodic_task
{
public:
    periodic_task (periodic_task const &) = delete;
    periodic_task& operator=(periodic_task const &) = delete;


    periodic_task(boost::asio::io_context& io_context
        , std::string const& name
        , std::chrono::seconds interval
        , task_t const &task);

    void execute(boost::system::error_code const& e);

    void start();

private:
    void start_wait();

    boost::asio::io_context& io_context_;
    boost::asio::steady_timer timer_;
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
    boost::asio::io_context io_context_;
    std::vector<std::unique_ptr<detail::periodic_task>> tasks_;
};
}// namespace measure
