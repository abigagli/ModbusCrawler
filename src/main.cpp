#include "meas_config.h"
#include "meas_executor.h"
#include "meas_reporter.h"
#include "periodic_scheduler.h"
#include "rtu_context.hpp"

#include <chrono>
#include <cinttypes>
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
                    [-r <reporting period = 5min>]
                    [-o(ut folder) = /tmp]

                    |
                    -R
                    [-d <device =/dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <regnum>
                    <regsize ={{1|2|4}{l|b} | Nr}>

                    |
                    -W
                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <regnum>
                    <value [0..65535]>
                })"
              << std::endl;
    return res;
}
} // namespace

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
namespace options {
enum class mode_t
{
    meas_scheduler,
    single_read,
    single_write
};
namespace defaults {
    mode_t mode                   = mode_t::meas_scheduler;
    std::string const device      = "/dev/ttyUSB0";
    std::string const line_config = "9600:8:N:1";
    std::string const log_path    = "";
    auto const answering_time     = 500ms;
    auto const reporting_period   = 5min;
    auto const logrotation_period = 1h;
    std::string const out_folder  = "/tmp";
} // namespace defaults

mode_t mode                             = defaults::mode;
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
std::string out_folder  = defaults::out_folder;
} // namespace options
#pragma clang diagnostic pop

int
single_read(int address, std::string regspec)
{
    assert(!regspec.empty());

    auto const last_char = regspec.back();
    if (regspec.size() < 2 ||
        (last_char != 'l' && last_char != 'b' && last_char != 'r'))
        return usage(-1, "invalid regsize specification: " + regspec);

    if (last_char == 'r')
    {
        // Raw read
        int const num_regs = std::strtol(regspec.c_str(), nullptr, 0);
        modbus::RTUContext ctx(
          options::server_id,
          "Server_" + std::to_string(options::server_id),
          modbus::SerialLine(options::device, options::line_config),
          options::answering_time,
          loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);

        std::vector<uint16_t> registers =
          ctx.read_holding_registers(address, num_regs);

        for (auto r = 0ULL; r != registers.size(); ++r)
        {
            auto const cur_addr = address + r * sizeof(uint16_t);
            LOG_S(INFO) << "RAW READ: " << std::setw(8) << std::hex << cur_addr
                        << ": " << std::setw(8) << registers[r] << " (dec "
                        << std::dec << std::setw(10) << registers[r] << ")";
        }
    }
    else
    {
        int const regsize = regspec[0] - '0';
        modbus::word_endianess word_endianess;

        if (regsize != 1 && regsize != 2 && regsize != 4)
            return usage(-1, "regsize must be <= 4");

        word_endianess = regspec[1] == 'l' ? modbus::word_endianess::little
                                           : modbus::word_endianess::big;

        modbus::RTUContext ctx(
          options::server_id,
          "Server_" + std::to_string(options::server_id),
          modbus::SerialLine(options::device, options::line_config),
          options::answering_time,
          loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);

        int64_t const val =
          ctx.read_holding_registers(address, regsize, word_endianess);

        LOG_S(INFO) << "SINGLE READ REGISTER " << address << ": " << val;
    }
    return 0;
}

int
single_write(int address, intmax_t value)
{
    if (value < 0 || value > std::numeric_limits<uint16_t>::max())
        return usage(-1, "invalid value: must be [0..65535]");

    modbus::RTUContext ctx(
      options::server_id,
      "Server_" + std::to_string(options::server_id),
      modbus::SerialLine(options::device, options::line_config),
      options::answering_time,
      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);

    ctx.write_holding_register(address, value);
    LOG_S(INFO) << "SINGLE WRITE REGISTER " << address << ": " << value;
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
    while ((ch = getopt(argc, argv, "RWhd:c:l:s:a:m:r:t:o:")) != -1)
    {
        switch (ch)
        {
        case 'R':
            options::mode = options::mode_t::single_read;
            break;
        case 'W':
            options::mode = options::mode_t::single_write;
            break;
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

    char log_file[PATH_MAX];
    if (!options::log_path.empty())
    {
        loguru::suggest_log_path(
          options::log_path.c_str(), log_file, sizeof(log_file));
        loguru::add_file(
          log_file, loguru::FileMode::Truncate, loguru::Verbosity_MAX);
    }

    if (options::mode == options::mode_t::single_read)
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1, "missing mandatory parameters for single read");

        int const address   = std::strtol(argv[0], nullptr, 0);
        char const *regspec = argv[1];
        return single_read(address, regspec);
    }
    else if (options::mode == options::mode_t::single_write)
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1, "missing mandatory parameters for single write");

        int const address = std::strtol(argv[0], nullptr, 0);
        intmax_t value    = std::strtoimax(argv[1], nullptr, 0);
        return single_write(address, value);
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