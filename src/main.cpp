#include "meas_config.h"
#include "meas_executor.h"
#include "meas_reporter.h"
#include "periodic_scheduler.h"
#include "rtu_context.hpp"

#include <chrono>
#include <iostream>
#include <loguru.hpp>
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
                    [-r <reporting period = 60s>]

                    |

                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <regnum>
                    <regsize <=4l/4b}>
                })"
              << std::endl;
    return res;
}
} // namespace

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
namespace options {
namespace defaults {
    std::string const device      = "/dev/ttyUSB0";
    std::string const line_config = "9600:8:N:1";
    std::string const log_path    = "";
    auto const answering_time     = 500ms;
    auto const reporting_period   = 60s;
    auto const logrotation_period = 1h;
} // namespace defaults

std::string log_path                    = defaults::log_path;
std::chrono::seconds logrotation_period = defaults::logrotation_period;
// Measure mode specific
std::string measconfig_file;
std::chrono::seconds reporting_period = defaults::reporting_period;

// Single-shot reads specific
std::string device      = defaults::device;
std::string line_config = defaults::line_config;
int server_id           = -1;
auto answering_time     = defaults::answering_time;
} // namespace options
#pragma clang diagnostic pop

int
single_read(int address, std::string regspec)
{
    int const regsize = regspec[0] - '0';
    modbus::word_endianess word_endianess;

    if (regsize > 1)
    {
        if (regsize > 4)
            return usage(-1, "regsize must be <= 4");

        if (regspec.size() != 2 || (regspec[1] != 'l' && regspec[1] != 'b'))
            return usage(-1, "invalid regsize specification: " + regspec);

        word_endianess = regspec[1] == 'l' ? modbus::word_endianess::little
                                           : modbus::word_endianess::big;
    }
    else
        word_endianess = modbus::word_endianess::dontcare;

    modbus::RTUContext ctx(
      options::server_id,
      "Server_" + std::to_string(options::server_id),
      modbus::SerialLine(options::device, options::line_config),
      options::answering_time,
      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);

    int64_t const val =
      ctx.read_holding_registers(address, regsize, word_endianess);

    LOG_S(INFO) << "REGISTER " << address << ": " << val;
    return 0;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "concurrency-mt-unsafe"
int
main(int argc, char *argv[])
{
    loguru::init(argc, argv);

    g_prog_name = argv[0];
    optind      = 1;
    int ch;
    while ((ch = getopt(argc, argv, "hd:c:l:s:a:m:r:t:")) != -1)
    {
        switch (ch)
        {
        case 'd':
            options::device = optarg;
            break;
        case 'l':
            options::log_path = optarg;
            break;
        case 'c':
            options::line_config = optarg;
            break;
        case 's':
            options::server_id = std::stoi(optarg);
            break;
        case 'a':
            options::answering_time =
              std::chrono::milliseconds(std::stoi(optarg));
            break;
        case 'm':
            options::measconfig_file = optarg;
            break;
        case 'r':
            options::reporting_period = std::chrono::seconds(std::stoi(optarg));
            break;
        case 't':
            options::logrotation_period =
              std::chrono::seconds(std::stoi(optarg));
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

    char log_file[PATH_MAX];
    if (!options::log_path.empty())
    {
        loguru::suggest_log_path(
          options::log_path.c_str(), log_file, sizeof(log_file));
        loguru::add_file(
          log_file, loguru::FileMode::Truncate, loguru::Verbosity_MAX);
    }

    if (options::measconfig_file.empty())
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1, "missing mandatory parameters");

        int const address   = std::stoi(argv[0]);
        char const *regspec = argv[1];
        return single_read(address, regspec);
    }

    auto meas_config = measure::read_config(options::measconfig_file);

    measure::Reporter reporter;

    for (auto const &el: meas_config)
    {
        auto const &server   = el.second.server;
        auto const &measures = el.second.measures;

        for (auto const &meas: measures)
            reporter.configure_measurement({server.name, server.modbus_id},
                                           meas.name,
                                           {meas.sampling_period,
                                            meas.value_type,
                                            meas.accumulating,
                                            meas.report_raw_samples});
    }

    /*********************************************
    measure::Reporter::when_t const nowsecs =
            std::chrono::time_point_cast<measure::Reporter::when_t::duration>(
                    measure::Reporter::when_t::clock::now());
    for (int i = 0; i < 5; ++i)
        reporter.add_measurement({"RANDOM", 666}, "Value 1", nowsecs +
    std::chrono::seconds(i), 3.14 + i);

    reporter.close_period();
    *********************************************/
    infra::PeriodicScheduler scheduler;

    scheduler.addTask(
      "ReportGenerator",
      options::reporting_period,
      [&reporter]() { reporter.close_period(); },
      false);

#if LOGURU_WITH_FILEABS
    if (!options::log_path.empty())
    {
        scheduler.addTask(
          "LogRotator",
          options::logrotation_period,
          [&]()
          {
              static int progr                = 0;
              int constexpr num_rotated_files = 5;
              std::string newname             = std::string(log_file) + "_" +
                                    std::to_string(progr++ % num_rotated_files);
              LOG_S(WARNING) << "Log rotating to " << newname;
              std::rename(log_file, newname.c_str());
          },
          false);
    }
#endif
    measure::Executor measure_executor(scheduler, reporter, meas_config);

    scheduler.run();
    return 0;
}
#pragma clang diagnostic pop