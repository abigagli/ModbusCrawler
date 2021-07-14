#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "meas_config.h"
#include "meas_executor.h"
#include "meas_reporter.h"
#include "modbus_ops.h"
#include "modbus_types.hpp"
#include "periodic_scheduler.h"

#include <chrono>
#include <cinttypes>
#include <iostream>
#include <loguru.hpp>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace std::chrono_literals;
namespace {
std::string g_prog_name;
int
usage(int res, std::string const &msg = "")
{
    if (!msg.empty())
        std::cerr << "\n*** ERROR: " << msg << " ***\n\n";
    std::cerr << "Usage:\n";
    std::cerr << g_prog_name << R"(
                [-h(help)]
                [-v(erbosity) = INFO]
                [-l(og_path) = "" (disabled)]
                [-t(ime of log rotation) = 1h]
                {
                    -m <measconfig_file.json>
                    [-r <reporting period = 5min>]
                    [-o(ut folder) = /tmp]

                    |
                    -R
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <regsize ={{1|2|4}{l|b} | Nr}>

                    |
                    -W
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <value [0..65535]>

                    |
                    -F
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <filename>

                    |
                    -U
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <filename>
                })"
              << std::endl;
    return res;
}
} // namespace

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
namespace options {
using namespace std::chrono_literals;
enum class mode_t
{
    unknown,
    meas_scheduler,
    single_read,
    single_write,
    file_transfer,
    flash_update,
};
namespace defaults {
    mode_t const mode = mode_t::unknown;

    int const slave_id                             = -1;
    std::string const serial_device                = "/dev/ttyCOM1";
    std::string const serial_config                = "9600:8:N:1";
    std::chrono::milliseconds const answering_time = 500ms;

    std::string const log_path                    = "";
    std::chrono::seconds const logrotation_period = 1h;
    std::string const out_folder                  = "/tmp";
    std::chrono::seconds const reporting_period   = 5min;
} // namespace defaults

auto mode = defaults::mode;

inline namespace rtu_parameters {
    auto slave_id       = defaults::slave_id;
    auto serial_device  = defaults::serial_device;
    auto serial_config  = defaults::serial_config;
    auto answering_time = defaults::answering_time;
} // namespace rtu_parameters

// Single-shot reads specific
auto log_path           = defaults::log_path;
auto logrotation_period = defaults::logrotation_period;

// Measure mode specific
auto out_folder       = defaults::out_folder;
auto reporting_period = defaults::reporting_period;
std::string measconfig_file;
} // namespace options

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma ide diagnostic ignored "concurrency-mt-unsafe"
int
main(int argc, char *argv[])
{
    g_prog_name = argv[0];
    loguru::init(argc, argv);

    optind = 1;
    int ch;
    while ((ch = getopt(argc, argv, "UFRWhd:c:l:s:a:m:r:t:o:")) != -1)
    {
        switch (ch)
        {
        case 'R':
            options::mode = options::mode_t::single_read;
            break;
        case 'W':
            options::mode = options::mode_t::single_write;
            break;
        case 'F':
            options::mode = options::mode_t::file_transfer;
            break;
        case 'U':
            options::mode = options::mode_t::flash_update;
            break;
        case 'd':
            options::serial_device = optarg;
            break;
        case 'l':
            options::log_path = optarg;
            break;
        case 'c':
            options::serial_config = optarg;
            break;
        case 's':
            options::slave_id = std::stoi(optarg);
            break;
        case 'a':
            options::answering_time =
              std::chrono::milliseconds(std::stoi(optarg));
            break;
        case 'm':
            options::mode            = options::mode_t::meas_scheduler;
            options::measconfig_file = optarg;
            break;
        case 'r':
            options::reporting_period = std::chrono::seconds(std::stoi(optarg));
            break;
        case 't':
            options::logrotation_period =
              std::chrono::seconds(std::stoi(optarg));
            break;
        case 'o':
            options::out_folder = optarg;
            break;
        case '?':
            return usage(-1);
        case 'h':
        default:
            return usage(0);
        }
    }

    argc -= optind;
    argv += optind;

    modbus::rtu_parameters rtu_parameters{
      .slave_id       = options::slave_id,
      .serial_device  = options::serial_device,
      .serial_config  = options::serial_config,
      .answering_time = options::answering_time};

    char log_file[PATH_MAX];
    if (!options::log_path.empty())
    {
        loguru::suggest_log_path(
          options::log_path.c_str(), log_file, sizeof(log_file));
        loguru::add_file(
          log_file, loguru::FileMode::Truncate, loguru::Verbosity_MAX);

        std::string current_symlink = options::log_path + "/current_log";
        int rv                      = unlink(current_symlink.c_str());
        rv = symlink(log_file, current_symlink.c_str());

        if (!rv)
        {
            LOG_S(INFO) << "Current log file symlinked from "
                        << current_symlink;
        }
    }

    try
    {
        if (options::mode == options::mode_t::single_read)
        {
            if (rtu_parameters.slave_id < 0 || argc < 2)
                return usage(
                  -1, "missing mandatory parameters for single_read mode");

            int const address   = std::strtol(argv[0], nullptr, 0);
            char const *regspec = argv[1];
            modbus::single_read(rtu_parameters,
                                address,
                                regspec,
                                loguru::g_stderr_verbosity >=
                                  loguru::Verbosity_MAX);
            return 0;
        }
        else if (options::mode == options::mode_t::single_write)
        {
            if (rtu_parameters.slave_id < 0 || argc < 2)
                return usage(
                  -1, "missing mandatory parameters for single_write mode");

            int const address = std::strtol(argv[0], nullptr, 0);
            intmax_t value    = std::strtoimax(argv[1], nullptr, 0);
            modbus::single_write(rtu_parameters,
                                 address,
                                 value,
                                 loguru::g_stderr_verbosity >=
                                   loguru::Verbosity_MAX);
            return 0;
        }
        else if (options::mode == options::mode_t::file_transfer)
        {
            if (rtu_parameters.slave_id < 0 || argc < 2)
                return usage(
                  -1, "missing mandatory parameters for file_transfer mode");

            int const address = std::strtol(argv[0], nullptr, 0);
            modbus::file_transfer(rtu_parameters,
                                  address,
                                  argv[1],
                                  loguru::g_stderr_verbosity >=
                                    loguru::Verbosity_MAX);
            return 0;
        }
        else if (options::mode == options::mode_t::flash_update)
        {
            if (rtu_parameters.slave_id < 0 || argc < 1)
                return usage(
                  -1, "missing mandatory parameters for flash_update mode");

            modbus::flash_update(rtu_parameters,
                                 argv[0],
                                 loguru::g_stderr_verbosity >=
                                   loguru::Verbosity_MAX);
            return 0;
        }
    }
    catch (std::invalid_argument const &e)
    {
        return usage(-1, e.what());
    }

    if (options::measconfig_file.empty())
        return usage(-1, "missing measures config file parameter");

    auto meas_config = measure::read_config(options::measconfig_file);

    measure::Reporter reporter(options::out_folder);

    for (auto const &el: meas_config)
    {
        auto const &server   = el.second.server;
        auto const &measures = el.second.measures;

        for (auto const &meas: measures)
            reporter.configure_measurement({server.name, server.modbus_id},
                                           meas.name,
                                           {meas.sampling_period,
                                            meas.accumulating,
                                            meas.report_raw_samples});
    }

    /*********************************************
    infra::when_t const nowsecs =
            std::chrono::time_point_cast<infra::when_t::duration>(
                    infra::when_t::clock::now());
    for (int i = 0; i < 5; ++i)
        reporter.add_measurement({"RANDOM", 666}, "Value 1", nowsecs +
    std::chrono::seconds(i), 3.14 + i);

    reporter.close_period();
    *********************************************/
    infra::PeriodicScheduler scheduler;

    scheduler.addTask(
      "ReportGenerator",
      options::reporting_period,
      [&reporter](infra::when_t now) { reporter.close_period(now); },
      infra::PeriodicScheduler::TaskMode::execute_at_multiples_of_period);

#if LOGURU_WITH_FILEABS
    if (!options::log_path.empty())
    {
        scheduler.addTask(
          "LogRotator",
          options::logrotation_period,
          [&](infra::when_t)
          {
              static int progr                = 0;
              int constexpr num_rotated_files = 5;
              std::string newname             = std::string(log_file) + "_" +
                                    std::to_string(progr++ % num_rotated_files);
              LOG_S(WARNING) << "Log rotating to " << newname;
              std::rename(log_file, newname.c_str());
          },
          infra::PeriodicScheduler::TaskMode::skip_first_execution);
    }
#endif
    measure::Executor measure_executor(scheduler, reporter, meas_config);

    scheduler.run();
    return 0;
}
#pragma clang diagnostic pop