#include "meas_config.h"
#include "meas_reporter.h"
#include "meas_scheduler.h"
#include "rtu_context.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace std::chrono_literals;
namespace {
std::string g_prog_name;
int
usage(int res, std::string const &msg = "")
{
    if (!msg.empty())
        std::cout << "\n*** ERROR: " << msg << " ***\n\n";
    std::cout << "Usage:\n";
    std::cout << g_prog_name << R"(
                [-h(help)]
                [-v(erbose)]
                {
                    -m <measconfig_file.json>
                    [-r <reporting period = 60s>]

                    |

                    [-d <device = /dev/ttyUSB0>]
                    [-l <line_config ="9600:8:N:1">]
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
    bool const verbose            = false;
    std::string const line_config = "9600:8:N:1";
    auto const answering_time     = 500ms;
    auto const reporting_period   = 60s;
} // namespace defaults

// Measure mode
std::string measconfig_file;
std::chrono::seconds reporting_period = defaults::reporting_period;

// Single-shot reads
std::string device      = defaults::device;
bool verbose            = defaults::verbose;
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
      options::verbose);

    // if (options::address >= 40000)
    //     options::address -= 40000;

    int64_t const val =
      ctx.read_holding_registers(address, regsize, word_endianess);

    std::cout << "REGISTER " << address << ": " << val << '\n';
    return 0;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "concurrency-mt-unsafe"
int
main(int argc, char *argv[])
{
    g_prog_name = argv[0];
    optind      = 1;
    int ch;
    while ((ch = getopt(argc, argv, "hd:vl:s:a:m:r:")) != -1)
    {
        switch (ch)
        {
        case 'd':
            options::device = optarg;
            break;
        case 'v':
            options::verbose = true;
            break;
        case 'l':
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
        case '?':
            return usage(-1);
        case 'h':
        default:
            return usage(0);
        }
    }

    argc -= optind;
    argv += optind;

    if (options::measconfig_file.empty())
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1, "missing mandatory parameters");

        int const address   = std::stoi(argv[0]);
        char const *regspec = argv[1];
        return single_read(address, regspec);
    }

    auto meas_config = measure::read_config(options::measconfig_file);

    measure::Reporter report;

    for (auto const &el: meas_config)
    {
        auto const &server   = el.second.server;
        auto const &measures = el.second.measures;

        for (auto const &meas: measures)
            report.add_entry({server.name, server.modbus_id},
                             meas.name,
                             {meas.sampling_period,
                              meas.accumulating,
                              meas.report_raw_samples});
    }

    /*********************************************
    measure::Reporter::when_t const nowsecs =
            std::chrono::time_point_cast<measure::Reporter::when_t::duration>(
                    measure::Reporter::when_t::clock::now());
    for (int i = 0; i < 5; ++i)
        report.add_measurement({"RANDOM", 666}, "Value 1", nowsecs +
    std::chrono::seconds(i), 3.14 + i);

    report.close_period();
    *********************************************/

    measure::scheduler scheduler(report, meas_config, options::verbose);

    scheduler.run_loop(options::reporting_period);
    return 0;
}
#pragma clang diagnostic pop