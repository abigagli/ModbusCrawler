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
                    <register>
                    <regsize ={{1|2|4}{l|b} | Nr}>

                    |
                    -W
                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <value [0..65535]>

                    |
                    -F
                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <filename>

                    |
                    -U
                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <filename>
                })"
              << std::endl;
    return res;
}

std::vector<uint16_t>
registers(std::string const &filename)
{
    std::ifstream ifs(filename, std::ios::binary);

    if (!ifs)
        throw std::runtime_error("invalid filename " + filename);

    std::istreambuf_iterator<char> it{ifs}, ibe;

    std::vector<uint16_t> content;
    size_t bytes = 0;

    enum class byte
    {
        low,
        high
    } phase = byte::high;

    uint8_t high_byte;

    while (it != ibe)
    {
        if (phase == byte::high)
        {
            high_byte = static_cast<uint8_t>(*it++);
            phase     = byte::low;
        }
        else
        {
            uint16_t const low_byte = static_cast<uint8_t>(*it++);

            content.push_back(
              static_cast<uint16_t>((high_byte << 8) | low_byte));
            phase = byte::high;
        }

        ++bytes;
    }

    if (phase == byte::low)
        content.push_back(static_cast<uint16_t>(high_byte << 8));

    LOG_S(INFO) << "read " << bytes << " from " << filename << " into "
                << content.size() << " elements";

    return content;
}
} // namespace

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
namespace options {
enum class mode_t
{
    meas_scheduler,
    single_read,
    single_write,
    file_transfer,
    flash_update,
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

uint32_t
crc32(std::vector<uint16_t> const &registers)
{
    return 0xFEFEDADA;
}

int
file_transfer(int address, std::string filename)
{
    std::vector<uint16_t> content = registers(filename);

    modbus::RTUContext ctx(
      options::server_id,
      "Server_" + std::to_string(options::server_id),
      modbus::SerialLine(options::device, options::line_config),
      options::answering_time,
      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);

    ctx.write_multiple_registers(address, content);
    LOG_S(INFO) << "FILE TRANSFER completed";
    return 0;
}

int
flash_update(std::string filename)
{
    enum class flash_update_registers : int
    {
        total_len_high = 2993,
        total_len_low  = 2994,
        crc32_high     = 2995,
        crc32_low      = 2996,
        offset_high    = 2997,
        offset_low     = 2998,
        chunk_len      = 2999,
        buffer         = 3000,
        cmd            = 3128,
    };

    enum class flash_update_commands : uint16_t
    {
        start         = 0xE05D,
        write_segment = 0xF1A5,
        done          = 0xD01E,
    };

    std::vector<uint16_t> content = registers(filename);

    auto const total_len_bytes = content.size() * sizeof(uint16_t);

    modbus::RTUContext ctx(
      options::server_id,
      "Server_" + std::to_string(options::server_id),
      modbus::SerialLine(options::device, options::line_config),
      options::answering_time,
      loguru::g_stderr_verbosity >= loguru::Verbosity_MAX);

    uint16_t constexpr flash_line_bytes = 256;
    auto const full_lines               = total_len_bytes / flash_line_bytes;

    int constexpr modbus_regs_at_once =
      flash_line_bytes / 2 /
      sizeof(uint16_t); // 1/2 flash line i.e. 64 registers i.e. 128 bytes

    uint32_t flash_offset = 0;
    int buffer_offset     = static_cast<int>(flash_update_registers::buffer);
    auto const *regs      = content.data();

    LOG_S(INFO) << "Sending 'start' command";
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::cmd),
      static_cast<uint16_t>(flash_update_commands::start));

    for (auto flash_line = 0ULL; flash_line != full_lines; ++flash_line)
    {
        LOG_S(INFO) << "FLASH line " << flash_line << " @ 0x" << std::hex
                    << flash_offset << ", REGBUFF @ 0x" << buffer_offset
                    << std::dec << ",  " << flash_line_bytes << " bytes in 2 * "
                    << modbus_regs_at_once << " registers";

        // Send current offset inside the receiver's pre-flash-write buffer,
        // i.e. flash_line * flash_line_bytes
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_high),
          flash_offset >> 16);

        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_low), flash_offset);

        // Send the current flash_line in two modbus' multiple-register writes
        ctx.write_multiple_registers(buffer_offset, regs, modbus_regs_at_once);
        ctx.write_multiple_registers(buffer_offset + modbus_regs_at_once,
                                     regs + modbus_regs_at_once,
                                     modbus_regs_at_once);

        // Send the actual bytes of the current flash line, in this case this is
        // always a full line
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::chunk_len),
          flash_line_bytes);

        // Send the write_segment command
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::cmd),
          static_cast<uint16_t>(flash_update_commands::write_segment));

        regs += 2 * modbus_regs_at_once;
        flash_offset += flash_line_bytes;
    }

    auto const remaining_bytes = total_len_bytes % flash_line_bytes;

    if (remaining_bytes)
    {
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_high),
          flash_offset >> 16);

        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_low), flash_offset);

        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::chunk_len), remaining_bytes);

        auto const remaining_chunks = remaining_bytes / 2 / modbus_regs_at_once;
        auto const remaining_regs = (remaining_bytes / 2) % modbus_regs_at_once;

        if (remaining_chunks)
        {
            LOG_S(INFO) << "FLASH remaining chunk @ 0x" << std::hex
                        << flash_offset << ", REGBUFF @ 0x" << buffer_offset
                        << std::dec << ",  " << modbus_regs_at_once * 2
                        << " bytes in " << modbus_regs_at_once << " registers";

            ctx.write_multiple_registers(
              buffer_offset, regs, modbus_regs_at_once);

            regs += modbus_regs_at_once;
            buffer_offset += modbus_regs_at_once;
        }

        LOG_S(INFO) << "FLASH remaining bytes @ 0x" << std::hex << flash_offset
                    << ", REGBUFF @ 0x" << buffer_offset << std::dec << ",  "
                    << remaining_regs * 2 << " bytes in " << remaining_regs
                    << " registers";

        ctx.write_multiple_registers(buffer_offset, regs, remaining_regs);

        // Send the write_segment command
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::cmd),
          static_cast<uint16_t>(flash_update_commands::write_segment));
    }
    LOG_S(INFO) << "Sending total len " << total_len_bytes;
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::total_len_high),
      total_len_bytes >> 16);
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::total_len_low), total_len_bytes);

    uint32_t const checksum = crc32(content);

    LOG_S(INFO) << "Sending crc32 " << std::hex << checksum << std::dec;
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::crc32_high), checksum >> 16);
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::crc32_low), checksum);

    LOG_S(INFO) << "Sending 'done' command";
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::cmd),
      static_cast<uint16_t>(flash_update_commands::done));

    LOG_S(INFO) << "FLASH UPDATE completed";
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
            return usage(-1,
                         "missing mandatory parameters for single_read mode");

        int const address   = std::strtol(argv[0], nullptr, 0);
        char const *regspec = argv[1];
        return single_read(address, regspec);
    }
    else if (options::mode == options::mode_t::single_write)
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1,
                         "missing mandatory parameters for single_write mode");

        int const address = std::strtol(argv[0], nullptr, 0);
        intmax_t value    = std::strtoimax(argv[1], nullptr, 0);
        return single_write(address, value);
    }
    else if (options::mode == options::mode_t::file_transfer)
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1,
                         "missing mandatory parameters for file_transfer mode");

        int const address = std::strtol(argv[0], nullptr, 0);
        return file_transfer(address, argv[1]);
    }
    else if (options::mode == options::mode_t::flash_update)
    {
        if (options::server_id < 0 || argc < 1)
            return usage(-1,
                         "missing mandatory parameters for flash_update mode");

        return flash_update(argv[0]);
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