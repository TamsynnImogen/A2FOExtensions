// Native 64-bit Linux telemetry for the 32-bit Armada process under Wine.
// This intentionally stays out of the game process and performs no input or
// window automation. It is a measurement aid, not a runtime feature host.

#include <dirent.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    pid_t pid = 0;
    double interval_seconds = 5.0;
    std::size_t samples = 0;
    double wait_seconds = 600.0;
    bool gpu = false;
    std::string output_path;
};

struct ProcessSample {
    std::uint64_t cpu_ticks = 0;
    std::uint64_t virtual_kib = 0;
    std::uint64_t rss_kib = 0;
    std::uint64_t pss_kib = 0;
    std::uint64_t private_clean_kib = 0;
    std::uint64_t private_dirty_kib = 0;
    std::uint64_t shared_clean_kib = 0;
    std::uint64_t shared_dirty_kib = 0;
    std::uint64_t swap_kib = 0;
    std::uint64_t threads = 0;
    std::uint64_t file_descriptors = 0;
};

struct GpuSample {
    int utilization_percent = -1;
    int memory_used_mib = -1;
    int process_memory_used_mib = -1;
};

using NvmlReturn = int;
using NvmlDevice = void*;
constexpr NvmlReturn kNvmlSuccess = 0;
constexpr NvmlReturn kNvmlInsufficientSize = 7;
constexpr unsigned long long kNvmlValueNotAvailable =
    std::numeric_limits<unsigned long long>::max();

struct NvmlUtilization {
    unsigned int gpu = 0;
    unsigned int memory = 0;
};

struct NvmlMemory {
    unsigned long long total = 0;
    unsigned long long free = 0;
    unsigned long long used = 0;
};

struct NvmlProcessInfo {
    unsigned int pid = 0;
    unsigned long long used_gpu_memory = 0;
    unsigned int gpu_instance_id = 0;
    unsigned int compute_instance_id = 0;
};

class NvmlSampler {
public:
    explicit NvmlSampler(bool requested) {
        if (!requested) return;
        library_ = dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!library_ ||
            !load("nvmlInit_v2", &initialize_) ||
            !load("nvmlShutdown", &shutdown_) ||
            !load("nvmlDeviceGetCount_v2", &device_count_) ||
            !load("nvmlDeviceGetHandleByIndex_v2", &device_at_) ||
            !load("nvmlDeviceGetUtilizationRates", &utilization_) ||
            !load("nvmlDeviceGetMemoryInfo", &memory_info_) ||
            initialize_() != kNvmlSuccess) {
            close();
            return;
        }
        load("nvmlDeviceGetGraphicsRunningProcesses_v3",
             &graphics_processes_);
        initialized_ = true;
    }

    ~NvmlSampler() { close(); }

    NvmlSampler(const NvmlSampler&) = delete;
    NvmlSampler& operator=(const NvmlSampler&) = delete;

    bool available() const { return initialized_; }

    GpuSample sample(pid_t target_pid) const {
        GpuSample result;
        if (!initialized_) return result;
        unsigned int count = 0;
        if (device_count_(&count) != kNvmlSuccess) return result;
        unsigned int maximum_utilization = 0;
        unsigned long long total_used = 0;
        bool utilization_found = false;
        bool memory_found = false;
        unsigned long long process_used = 0;
        bool process_found = false;
        for (unsigned int index = 0; index < count; ++index) {
            NvmlDevice device = nullptr;
            if (device_at_(index, &device) != kNvmlSuccess || !device) {
                continue;
            }
            NvmlUtilization utilization{};
            if (utilization_(device, &utilization) == kNvmlSuccess) {
                maximum_utilization =
                    std::max(maximum_utilization, utilization.gpu);
                utilization_found = true;
            }
            NvmlMemory memory{};
            if (memory_info_(device, &memory) == kNvmlSuccess) {
                total_used += memory.used;
                memory_found = true;
            }
            if (graphics_processes_ && target_pid > 0) {
                unsigned int process_count = 0;
                const NvmlReturn size_result = graphics_processes_(
                    device, &process_count, nullptr);
                if ((size_result == kNvmlInsufficientSize ||
                     size_result == kNvmlSuccess) && process_count != 0) {
                    process_count += 8;
                    std::vector<NvmlProcessInfo> processes(process_count);
                    if (graphics_processes_(device, &process_count,
                                            processes.data()) ==
                        kNvmlSuccess) {
                        for (unsigned int process_index = 0;
                             process_index < process_count;
                             ++process_index) {
                            const NvmlProcessInfo& process =
                                processes[process_index];
                            if (process.pid ==
                                    static_cast<unsigned int>(target_pid) &&
                                process.used_gpu_memory !=
                                    kNvmlValueNotAvailable) {
                                process_used += process.used_gpu_memory;
                                process_found = true;
                            }
                        }
                    }
                }
            }
        }
        if (utilization_found) {
            result.utilization_percent =
                static_cast<int>(maximum_utilization);
        }
        if (memory_found) {
            result.memory_used_mib = static_cast<int>(
                total_used / (1024ull * 1024ull));
        }
        if (process_found) {
            result.process_memory_used_mib = static_cast<int>(
                process_used / (1024ull * 1024ull));
        }
        return result;
    }

private:
    using Initialize = NvmlReturn (*)();
    using Shutdown = NvmlReturn (*)();
    using DeviceCount = NvmlReturn (*)(unsigned int*);
    using DeviceAt = NvmlReturn (*)(unsigned int, NvmlDevice*);
    using Utilization = NvmlReturn (*)(NvmlDevice, NvmlUtilization*);
    using MemoryInfo = NvmlReturn (*)(NvmlDevice, NvmlMemory*);
    using GraphicsProcesses = NvmlReturn (*)(
        NvmlDevice, unsigned int*, NvmlProcessInfo*);

    template <typename Function>
    bool load(const char* name, Function* output) {
        if (!library_ || !output) return false;
        void* symbol = dlsym(library_, name);
        if (!symbol) return false;
        static_assert(sizeof(symbol) == sizeof(*output),
                      "POSIX function and object pointers must match");
        std::memcpy(output, &symbol, sizeof(symbol));
        return *output != nullptr;
    }

    void close() {
        if (initialized_ && shutdown_) shutdown_();
        initialized_ = false;
        if (library_) dlclose(library_);
        library_ = nullptr;
    }

    void* library_ = nullptr;
    Initialize initialize_ = nullptr;
    Shutdown shutdown_ = nullptr;
    DeviceCount device_count_ = nullptr;
    DeviceAt device_at_ = nullptr;
    Utilization utilization_ = nullptr;
    MemoryInfo memory_info_ = nullptr;
    GraphicsProcesses graphics_processes_ = nullptr;
    bool initialized_ = false;
};

bool numeric_name(const char* name) {
    if (!name || !*name) return false;
    for (const unsigned char ch : std::string(name)) {
        if (!std::isdigit(ch)) return false;
    }
    return true;
}

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

std::optional<std::string> read_first_line(const std::string& path) {
    std::ifstream input(path);
    std::string line;
    if (!input || !std::getline(input, line)) return std::nullopt;
    return trim(std::move(line));
}

bool process_exists(pid_t pid) {
    if (pid <= 0) return false;
    const std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream input(path);
    return static_cast<bool>(input);
}

pid_t find_armada_process() {
    DIR* directory = opendir("/proc");
    if (!directory) return 0;
    pid_t selected = 0;
    while (dirent* entry = readdir(directory)) {
        if (!numeric_name(entry->d_name)) continue;
        const auto line = read_first_line(
            std::string("/proc/") + entry->d_name + "/comm");
        if (!line || lower_ascii(*line) != "armadal.exe") continue;
        const long parsed = std::strtol(entry->d_name, nullptr, 10);
        if (parsed > 0 && parsed <= std::numeric_limits<pid_t>::max()) {
            selected = static_cast<pid_t>(parsed);
            break;
        }
    }
    closedir(directory);
    return selected;
}

bool parse_u64(const std::string& value, std::uint64_t* output) {
    if (!output || value.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(
        value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str()) return false;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;
    *output = static_cast<std::uint64_t>(parsed);
    return true;
}

bool read_process_stat(pid_t pid, ProcessSample* sample) {
    if (!sample) return false;
    std::ifstream input(
        "/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if (!input || !std::getline(input, line)) return false;
    const std::size_t close = line.rfind(')');
    if (close == std::string::npos || close + 2 >= line.size()) return false;
    std::istringstream fields(line.substr(close + 2));
    std::vector<std::string> values;
    std::string value;
    while (fields >> value) values.push_back(std::move(value));
    // values[0] is proc field 3 (state).
    if (values.size() <= 21) return false;
    std::uint64_t user_ticks = 0;
    std::uint64_t system_ticks = 0;
    std::uint64_t virtual_bytes = 0;
    std::uint64_t resident_pages = 0;
    if (!parse_u64(values[11], &user_ticks) ||
        !parse_u64(values[12], &system_ticks) ||
        !parse_u64(values[17], &sample->threads) ||
        !parse_u64(values[20], &virtual_bytes) ||
        !parse_u64(values[21], &resident_pages)) {
        return false;
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return false;
    sample->cpu_ticks = user_ticks + system_ticks;
    sample->virtual_kib = virtual_bytes / 1024u;
    sample->rss_kib = resident_pages *
        static_cast<std::uint64_t>(page_size) / 1024u;
    return true;
}

void assign_smaps_value(const std::string& key, std::uint64_t value,
                        ProcessSample* sample) {
    if (key == "Rss") sample->rss_kib = value;
    else if (key == "Pss") sample->pss_kib = value;
    else if (key == "Private_Clean") sample->private_clean_kib = value;
    else if (key == "Private_Dirty") sample->private_dirty_kib = value;
    else if (key == "Shared_Clean") sample->shared_clean_kib = value;
    else if (key == "Shared_Dirty") sample->shared_dirty_kib = value;
    else if (key == "Swap") sample->swap_kib = value;
}

void read_smaps_rollup(pid_t pid, ProcessSample* sample) {
    std::ifstream input(
        "/proc/" + std::to_string(pid) + "/smaps_rollup");
    std::string line;
    while (input && std::getline(input, line)) {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = trim(line.substr(0, colon));
        std::istringstream value(line.substr(colon + 1));
        std::uint64_t kib = 0;
        if (value >> kib) assign_smaps_value(key, kib, sample);
    }
}

std::uint64_t count_file_descriptors(pid_t pid) {
    const std::string path = "/proc/" + std::to_string(pid) + "/fd";
    DIR* directory = opendir(path.c_str());
    if (!directory) return 0;
    std::uint64_t count = 0;
    while (dirent* entry = readdir(directory)) {
        if (std::strcmp(entry->d_name, ".") != 0 &&
            std::strcmp(entry->d_name, "..") != 0) {
            ++count;
        }
    }
    closedir(directory);
    return count;
}

std::optional<ProcessSample> sample_process(pid_t pid) {
    ProcessSample sample;
    if (!read_process_stat(pid, &sample)) return std::nullopt;
    read_smaps_rollup(pid, &sample);
    sample.file_descriptors = count_file_descriptors(pid);
    return sample;
}

bool parse_positive_double(const char* text, double* output) {
    if (!text || !output) return false;
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || value <= 0.0) {
        return false;
    }
    *output = value;
    return true;
}

bool parse_nonnegative_size(const char* text, std::size_t* output) {
    if (!text || !output) return false;
    std::uint64_t value = 0;
    if (!parse_u64(text, &value) ||
        value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    *output = static_cast<std::size_t>(value);
    return true;
}

void usage(const char* program) {
    std::cerr
        << "Usage: " << program
        << " [--pid PID] [--interval SECONDS] [--samples COUNT]\n"
           "       [--wait SECONDS] [--output CSV] [--gpu]\n";
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto next = [&]() -> const char* {
            return index + 1 < argc ? argv[++index] : nullptr;
        };
        if (argument == "--pid") {
            std::size_t value = 0;
            const char* text = next();
            if (!parse_nonnegative_size(text, &value) || value == 0 ||
                value > static_cast<std::size_t>(
                    std::numeric_limits<pid_t>::max())) {
                return std::nullopt;
            }
            options.pid = static_cast<pid_t>(value);
        } else if (argument == "--interval") {
            if (!parse_positive_double(next(), &options.interval_seconds)) {
                return std::nullopt;
            }
        } else if (argument == "--samples") {
            if (!parse_nonnegative_size(next(), &options.samples)) {
                return std::nullopt;
            }
        } else if (argument == "--wait") {
            if (!parse_positive_double(next(), &options.wait_seconds)) {
                return std::nullopt;
            }
        } else if (argument == "--output") {
            const char* text = next();
            if (!text || !*text) return std::nullopt;
            options.output_path = text;
        } else if (argument == "--gpu") {
            options.gpu = true;
        } else if (argument == "--help" || argument == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else {
            return std::nullopt;
        }
    }
    return options;
}

pid_t wait_for_armada(double wait_seconds) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::duration<double>(wait_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        const pid_t pid = find_armada_process();
        if (pid != 0) return pid;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    static_assert(sizeof(void*) == 8,
                  "a2fo_telemetry must be built as a 64-bit helper");
    const auto parsed = parse_options(argc, argv);
    if (!parsed) {
        usage(argv[0]);
        return 2;
    }
    const Options options = *parsed;
    pid_t pid = options.pid;
    if (pid == 0) {
        std::cerr << "Waiting for ArmadaL.exe...\n";
        pid = wait_for_armada(options.wait_seconds);
    }
    if (pid == 0 || !process_exists(pid)) {
        std::cerr << "ArmadaL.exe was not found\n";
        return 1;
    }

    NvmlSampler gpu_sampler(options.gpu);
    if (options.gpu && !gpu_sampler.available()) {
        std::cerr << "NVIDIA telemetry is unavailable; GPU columns will be -1\n";
    }

    std::ofstream file;
    if (!options.output_path.empty()) {
        file.open(options.output_path, std::ios::out | std::ios::trunc);
        if (!file) {
            std::cerr << "Could not open output: "
                      << options.output_path << '\n';
            return 1;
        }
    }
    std::ostream& output = file.is_open() ? file : std::cout;
    output << "elapsed_s,pid,cpu_pct,virtual_kib,rss_kib,pss_kib,"
              "private_clean_kib,private_dirty_kib,shared_clean_kib,"
              "shared_dirty_kib,swap_kib,threads,fds,gpu_util_pct,"
              "gpu_memory_mib,process_gpu_memory_mib\n";
    output.flush();

    const long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) return 1;
    const auto start = std::chrono::steady_clock::now();
    auto previous_time = start;
    std::optional<ProcessSample> previous;
    std::optional<ProcessSample> first;
    std::uint64_t peak_pss_kib = 0;
    std::uint64_t peak_private_kib = 0;
    int peak_gpu_memory_mib = -1;
    int peak_process_gpu_memory_mib = -1;
    double last_elapsed = 0.0;
    std::size_t count = 0;
    while (options.samples == 0 || count < options.samples) {
        const auto current = sample_process(pid);
        if (!current) {
            std::cerr << "ArmadaL.exe exited after " << count
                      << " sample(s)\n";
            break;
        }
        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - start).count();
        double cpu_percent = 0.0;
        if (previous) {
            const double interval =
                std::chrono::duration<double>(now - previous_time).count();
            if (interval > 0.0 && current->cpu_ticks >= previous->cpu_ticks) {
                cpu_percent = 100.0 *
                    static_cast<double>(
                        current->cpu_ticks - previous->cpu_ticks) /
                    static_cast<double>(ticks_per_second) / interval;
            }
        }
        const GpuSample gpu = options.gpu
            ? gpu_sampler.sample(pid) : GpuSample{};
        output << std::fixed << std::setprecision(3) << elapsed << ','
               << pid << ',' << std::setprecision(2) << cpu_percent << ','
               << current->virtual_kib << ',' << current->rss_kib << ','
               << current->pss_kib << ',' << current->private_clean_kib << ','
               << current->private_dirty_kib << ','
               << current->shared_clean_kib << ','
               << current->shared_dirty_kib << ',' << current->swap_kib << ','
               << current->threads << ',' << current->file_descriptors << ','
               << gpu.utilization_percent << ',' << gpu.memory_used_mib << ','
               << gpu.process_memory_used_mib
               << '\n';
        output.flush();
        if (!first) first = current;
        peak_pss_kib = std::max(peak_pss_kib, current->pss_kib);
        peak_private_kib = std::max(
            peak_private_kib,
            current->private_clean_kib + current->private_dirty_kib);
        peak_gpu_memory_mib =
            std::max(peak_gpu_memory_mib, gpu.memory_used_mib);
        peak_process_gpu_memory_mib = std::max(
            peak_process_gpu_memory_mib, gpu.process_memory_used_mib);
        last_elapsed = elapsed;
        previous = current;
        previous_time = now;
        ++count;
        if (options.samples == 0 || count < options.samples) {
            std::this_thread::sleep_for(
                std::chrono::duration<double>(options.interval_seconds));
        }
    }
    if (first && previous) {
        const auto signed_delta = [](std::uint64_t end,
                                     std::uint64_t begin) {
            return static_cast<long long>(end) -
                   static_cast<long long>(begin);
        };
        const std::uint64_t first_private = first->private_clean_kib +
            first->private_dirty_kib;
        const std::uint64_t final_private = previous->private_clean_kib +
            previous->private_dirty_kib;
        std::cerr << "Telemetry summary: " << count << " sample(s), "
                  << std::fixed << std::setprecision(1) << last_elapsed
                  << " s; PSS " << first->pss_kib << " -> "
                  << previous->pss_kib << " KiB (delta "
                  << signed_delta(previous->pss_kib, first->pss_kib)
                  << ", peak " << peak_pss_kib << "); private "
                  << first_private << " -> " << final_private
                  << " KiB (delta "
                  << signed_delta(final_private, first_private)
                  << ", peak " << peak_private_kib << ")";
        if (peak_gpu_memory_mib >= 0) {
            std::cerr << "; global GPU-memory peak "
                      << peak_gpu_memory_mib << " MiB";
        }
        if (peak_process_gpu_memory_mib >= 0) {
            std::cerr << "; process GPU-memory peak "
                      << peak_process_gpu_memory_mib << " MiB";
        }
        std::cerr << '\n';
    }
    return 0;
}
