#define LOG_MAILER_NO_MAIN
#include "../modules/A2FOLogMailer/helper.cpp"
#include "../modules/A2FOLogMailer/module.cpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
FILETIME shifted(FILETIME value, long long seconds) {
    const auto ticks = log_mailer::ticks(value) + seconds * 10000000LL;
    return {DWORD(ticks), DWORD(ticks >> 32)};
}
std::wstring fixture_root() {
    wchar_t directory[MAX_PATH]{}, name[MAX_PATH]{};
    require(GetTempPathW(MAX_PATH, directory) != 0, "temp path");
    require(GetTempFileNameW(directory, L"alm", 0, name) != 0, "temp name");
    require(DeleteFileW(name) && CreateDirectoryW(name, nullptr), "fixture directory");
    return name;
}
void file(const std::wstring& root, const std::wstring& name,
          const std::string& content, FILETIME when) {
    const auto path = log_mailer::join(root, name);
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(handle != INVALID_HANDLE_VALUE, "fixture file open");
    DWORD count = 0;
    require(WriteFile(handle, content.data(), DWORD(content.size()), &count, nullptr) &&
            count == content.size(), "fixture file write");
    require(SetFileTime(handle, nullptr, nullptr, &when), "fixture timestamp");
    CloseHandle(handle);
}
const log_mailer::Attachment* find(const std::vector<log_mailer::Attachment>& files,
                                   const std::string& name) {
    for (const auto& entry : files) if (entry.name == name) return &entry;
    return nullptr;
}
void remove_fixture(const std::wstring& root) {
    WIN32_FIND_DATAW found{};
    const HANDLE search = FindFirstFileW(log_mailer::join(root, L"*").c_str(), &found);
    require(search != INVALID_HANDLE_VALUE, "fixture cleanup search");
    do {
        const auto path = log_mailer::join(root, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (std::wstring(found.cFileName) != L"." &&
                std::wstring(found.cFileName) != L"..") remove_fixture(path);
        } else {
            require(DeleteFileW(path.c_str()), "fixture file cleanup");
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
    require(RemoveDirectoryW(root.c_str()), "fixture directory cleanup");
}
void setting(const std::wstring& root, const wchar_t* key, const wchar_t* value) {
    const auto path = log_mailer::config_path(root);
    require(WritePrivateProfileStringW(L"Reporting", key, value, path.c_str()),
            "fixture config write");
}
void configured(const std::wstring& root, const wchar_t* mode) {
    setting(root, L"Enabled", L"1");
    setting(root, L"RelayUrl", L"https://example.test/v1/crash-reports");
    setting(root, L"SubmissionToken", L"test-token");
    setting(root, L"Mode", mode);
}
int sends = 0;
std::string sent_payload;
bool record_upload(const log_mailer::Config&, const std::string& json) {
    ++sends;
    sent_payload = json;
    return false;
}
struct LockProbe {
    const std::wstring* root;
    HANDLE started;
    HANDLE finished;
};
DWORD WINAPI read_under_lock(void* opaque) {
    auto& probe = *static_cast<LockProbe*>(opaque);
    SetEvent(probe.started);
    log_mailer::load_config(*probe.root);
    SetEvent(probe.finished);
    return 0;
}
void watch_case(const wchar_t* mode, DWORD exit_code, bool cancel_before_watch,
                bool expect_send) {
    const auto root = fixture_root();
    configured(root, mode);
    wchar_t executable[MAX_PATH]{};
    require(GetModuleFileNameW(nullptr, executable, MAX_PATH) != 0,
            "fixture executable path");
    auto command = log_mailer::quote(executable) + L" --fixture-exit " +
                   std::to_wstring(exit_code);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    require(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, root.c_str(), &startup, &child),
            "fixture child launch");
    CloseHandle(child.hThread);
    HANDLE cancel = CreateEventW(nullptr, TRUE, cancel_before_watch, nullptr);
    require(cancel != nullptr, "fixture cancel event");
    sends = 0;
    sent_payload.clear();
    const int result = log_mailer::watch(child.hProcess, cancel, root, record_upload);
    const bool outcome = (expect_send && result == 4 && sends == 1 &&
             sent_payload.find("session-log") == std::string::npos &&
             sent_payload.find("c2Vzc2lvbi1sb2c=") != std::string::npos &&
             (sent_payload.find("dxwrapper-armadal.log") != std::string::npos) ==
                 (std::wcscmp(mode, L"all_sessions") == 0)) ||
            (!expect_send && result == 0 && sends == 0);
    if (!outcome) {
        FILETIME created{}, exited{}, kernel{}, user{}, observed{};
        const BOOL times_ok = GetProcessTimes(child.hProcess, &created, &exited, &kernel, &user);
        GetSystemTimeAsFileTime(&observed);
        WIN32_FILE_ATTRIBUTE_DATA log_data{};
        const BOOL log_ok = GetFileAttributesExW(log_mailer::join(root, L"A2FOExtensions.log").c_str(),
                                                GetFileExInfoStandard, &log_data);
        const auto config = log_mailer::load_config(root);
        const auto selected = times_ok ?
            log_mailer::collect(root, created, observed, config.all_sessions) :
            std::vector<log_mailer::Attachment>{};
        const DWORD reports = GetFileAttributesW(log_mailer::join(root, L"CrashReports").c_str());
        std::fwprintf(stderr,
            L"watch case mode=%ls exit=%lu cancel=%d expect=%d result=%d sends=%d "
            L"config_enabled=%d config_ready=%d config_all=%d times_ok=%d "
            L"created=%llu log_ok=%d log_write=%llu now=%llu fresh=%d "
            L"collected=%zu reports_present=%d payload_has_log=%d payload_has_wrapper=%d\n",
            mode, static_cast<unsigned long>(exit_code), int(cancel_before_watch),
            int(expect_send), result, sends, int(config.enabled), int(config.ready()),
            int(config.all_sessions), int(times_ok), times_ok ? log_mailer::ticks(created) : 0ULL,
            int(log_ok), log_ok ? log_mailer::ticks(log_data.ftLastWriteTime) : 0ULL,
            log_mailer::ticks(observed), int(times_ok && log_ok &&
                log_mailer::fresh(log_data.ftLastWriteTime, created, observed)),
            selected.size(), int(reports != INVALID_FILE_ATTRIBUTES),
            int(sent_payload.find("c2Vzc2lvbi1sb2c=") != std::string::npos),
            int(sent_payload.find("dxwrapper-armadal.log") != std::string::npos));
    }
    require(outcome, "watch mode/cancel outcome");
    require(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0,
            "fixture child completion");
    CloseHandle(cancel);
    CloseHandle(child.hProcess);
    remove_fixture(root);
}
std::string root_ansi;
std::string message;
const char* A2FO_CALL get_root() { return root_ansi.c_str(); }
void A2FO_CALL record(const char*, const char* text) { message = text; }
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wcscmp(argv[1], L"--fixture-exit") == 0) {
        wchar_t root[MAX_PATH]{};
        require(GetCurrentDirectoryW(MAX_PATH, root) != 0, "child fixture root");
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        file(root, L"A2FOExtensions.log", "session-log", now);
        file(root, L"dxwrapper-armadal.log", "wrapper-log", now);
        Sleep(500);
        ExitProcess(std::wcstoul(argv[2], nullptr, 10));
    }
    require(argc == 1, "test arguments");
    using namespace log_mailer;
    const auto root = fixture_root();
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const FILETIME start = shifted(now, -60);
    const FILETIME old = shifted(now, -120);
    const FILETIME recent = shifted(now, -10);

    require(!fresh(old, start, now) && fresh(start, start, now) &&
            fresh(now, start, now) && !fresh(shifted(now, 1), start, now),
            "fresh time boundaries");
    require(!fresh_native(root, start, now), "missing native report");
    file(root, L"bugreport.txt", "old", old);
    require(!fresh_native(root, start, now), "stale native report");
    file(root, L"bugreport.txt", std::string(max_file_bytes, 'B') + "report-end", recent);
    file(root, L"A2FOExtensions.log", "log-start" + std::string(max_file_bytes, 'L'), recent);
    file(root, L"a2fo_extra.log", "extra", recent);
    file(root, L"a2fo_stale.log", "stale", old);
    file(root, L"other.log", "unrelated", recent);
    file(root, L"dxwrapper-armadal.log", "wrapper", recent);
    file(root, L"a2fo_bad!.log", "unsafe", recent);
    require(fresh_native(root, start, now), "fresh native report");
    auto files = collect(root, start, now);
    require(files.size() == 3 && files[0].name == "bugreport.txt" &&
            files[1].name == "A2FOExtensions.log", "selection and priority");
    require(files[0].bytes == std::string(max_file_bytes, 'B') &&
            files[1].bytes == std::string(max_file_bytes, 'L'),
            "per-file cap and report head/log tail");
    require(find(files, "a2fo_extra.log") && !find(files, "a2fo_stale.log") &&
            !find(files, "other.log") && !find(files, "dxwrapper-armadal.log"),
            "crash-only selection skips stale and unrelated files");
    const auto all_logs = collect(root, start, now, true);
    require(find(all_logs, "other.log") && find(all_logs, "dxwrapper-armadal.log") &&
            !find(all_logs, "a2fo_stale.log") && !find(all_logs, "a2fo_bad!.log"),
            "all-sessions includes safe game logs but excludes stale/unsafe files");
    const auto json = payload(0xc0000005u, {{"a2fo_x.log", std::string("\0\xff", 2)}});
    require(json == "{\"version\":1,\"exit_code\":3221225477,\"files\":[{\"name\":\"a2fo_x.log\",\"content_base64\":\"AP8=\"}]}",
            "payload encoding");

    const auto absent = load_config(root);
    require(!absent.enabled && !absent.all_sessions && absent.ready() &&
            absent.url == distribution::relay_url &&
            absent.token == distribution::submission_token &&
            !upload(absent, "{}"),
            "absent config has ready defaults but disables upload without network");

    const auto preferences_root = fixture_root();
    std::wstring empty_section;
    require(preferences_section(preferences_root, empty_section),
            "empty preferences snapshot");
    require(save_preferences(preferences_root, true, false, empty_section),
            "opt-in preferences save without relay keys");
    const auto opted_in = load_config(preferences_root);
    require(opted_in.enabled && !opted_in.all_sessions && opted_in.ready() &&
            opted_in.url == distribution::relay_url &&
            opted_in.token == distribution::submission_token &&
            read_setting(config_path(preferences_root), L"RelayUrl").empty() &&
            read_setting(config_path(preferences_root), L"SubmissionToken").empty(),
            "opt-in uses distribution defaults without writing relay keys");
    remove_fixture(preferences_root);

    setting(root, L"RelayUrl", L"");
    setting(root, L"SubmissionToken", L"");
    const auto blank_template = load_config(root);
    require(!blank_template.enabled && !blank_template.all_sessions &&
            blank_template.ready() &&
            blank_template.url == distribution::relay_url &&
            blank_template.token == distribution::submission_token,
            "blank template relay pair uses distribution defaults");
    configured(root, L"crash_only");
    setting(root, L"Enabled", L"0");
    const auto disabled = load_config(root);
    require(!disabled.enabled && !disabled.all_sessions && disabled.ready() &&
            !upload(disabled, "{}"),
            "disabled valid config still blocks upload");
    setting(root, L"Enabled", L"1");
    const auto ready = load_config(root);
    require(ready.enabled && !ready.all_sessions && ready.ready() &&
            ready.url == L"https://example.test/v1/crash-reports" &&
            ready.token == L"test-token",
            "explicit custom relay pair overrides distribution defaults");
    setting(root, L"SubmissionToken", L"");
    const auto missing_token = load_config(root);
    require(missing_token.enabled && !missing_token.ready() &&
            missing_token.url == ready.url && missing_token.token.empty(),
            "custom relay does not borrow distribution token");
    setting(root, L"RelayUrl", L"");
    setting(root, L"SubmissionToken", L"test-token");
    const auto missing_url = load_config(root);
    require(missing_url.enabled && !missing_url.ready() &&
            missing_url.url.empty() && missing_url.token == ready.token,
            "custom token does not borrow distribution relay");
    const std::wstring oversized(2048, L'x');
    setting(root, L"RelayUrl", oversized.c_str());
    setting(root, L"SubmissionToken", oversized.c_str());
    const auto oversized_pair = load_config(root);
    require(oversized_pair.enabled && !oversized_pair.ready() &&
            !oversized_pair.url.empty() && !oversized_pair.token.empty() &&
            oversized_pair.url != distribution::relay_url &&
            oversized_pair.token != distribution::submission_token,
            "two oversized overrides cannot select distribution defaults");
    setting(root, L"SubmissionToken", L"");
    const auto oversized_url = load_config(root);
    require(oversized_url.enabled && !oversized_url.ready() &&
            !oversized_url.url.empty() && oversized_url.token.empty(),
            "oversized relay and blank token remain invalid");
    setting(root, L"RelayUrl", L"");
    setting(root, L"SubmissionToken", oversized.c_str());
    const auto oversized_token = load_config(root);
    require(oversized_token.enabled && !oversized_token.ready() &&
            oversized_token.url.empty() && !oversized_token.token.empty(),
            "blank relay and oversized token remain invalid");
    setting(root, L"RelayUrl", ready.url.c_str());
    setting(root, L"SubmissionToken", ready.token.c_str());
    setting(root, L"Mode", L"all_sessions");
    require(load_config(root).all_sessions, "all-sessions mode parsed");
    require(!Config{true, L"http://example.test/v1/crash-reports", L"token"}.ready() &&
            !Config{true, L"https://example.test/v1/crash-reports", L"bad\r\ntoken"}.ready() &&
            !Config{true, L"https://user:pass@example.test/v1/crash-reports", L"token"}.ready() &&
            !Config{true, L"https://example.test/v1/crash-reports?x=1", L"token"}.ready() &&
            !Config{true, L"https://example.test/wrong", L"token"}.ready(),
            "unsafe config rejected");
    require(quote(L"a b") == L"\"a b\"" &&
            quote(L"a\\\"b\\") == L"\"a\\\\\\\"b\\\\\"",
            "command-line quote escaping");
    setting(root, L"UnrelatedSetting", L"retain-me");
    std::wstring previous;
    require(preferences_section(root, previous), "preferences snapshot");
    require(save_preferences(root, false, false, previous), "preferences save");
    const auto changed = load_config(root);
    require(!changed.enabled && !changed.all_sessions && changed.ready() &&
            changed.url == ready.url && changed.token == ready.token &&
            read_setting(config_path(root), L"UnrelatedSetting") == L"retain-me",
            "preferences roundtrip preserves relay and unknown setting");
    require(write_section(root, previous), "preferences rollback");
    const auto restored = load_config(root);
    require(restored.enabled && restored.all_sessions && restored.ready() &&
            read_setting(config_path(root), L"UnrelatedSetting") == L"retain-me",
            "preferences rollback restores consent and mode");
    HANDLE started = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE finished = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    require(started && finished, "lock probe events");
    LockProbe probe{&root, started, finished};
    HANDLE reader = nullptr;
    {
        PreferencesLock lock;
        require(lock.held, "preferences lock acquired");
        reader = CreateThread(nullptr, 0, read_under_lock, &probe, 0, nullptr);
        require(reader != nullptr, "lock probe thread");
        require(WaitForSingleObject(started, 5000) == WAIT_OBJECT_0,
                "lock probe entered");
        require(WaitForSingleObject(finished, 50) == WAIT_TIMEOUT,
                "concurrent config read waits for preferences lock");
    }
    require(WaitForSingleObject(finished, 5000) == WAIT_OBJECT_0,
            "concurrent config read resumes after release");
    require(WaitForSingleObject(reader, 5000) == WAIT_OBJECT_0,
            "lock probe thread completion");
    CloseHandle(reader);
    CloseHandle(started);
    CloseHandle(finished);

    // The module must remain passive with no configuration, even when a helper
    // executable is absent. Its success and log message exercise that branch.
    const auto module_root = fixture_root();
    char ansi[MAX_PATH]{};
    require(WideCharToMultiByte(CP_ACP, 0, module_root.c_str(), -1,
                                ansi, MAX_PATH, nullptr, nullptr) != 0,
            "fixture ANSI root");
    root_ansi = ansi;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.root_directory = get_root;
    api.log = record;
    require(A2FO_ModuleInit(&api) && message.find("Disabled") != std::string::npos,
            "module disabled without helper");
    A2FO_ModuleShutdown();

    // Saturate both limits with independent roots to keep file enumeration
    // and total-byte decisions observable.
    const auto count_root = fixture_root();
    for (unsigned i = 0; i < max_files + 3; ++i)
        file(count_root, L"a2fo_" + std::to_wstring(i) + L".log", "x", recent);
    require(collect(count_root, start, now).size() == max_files, "file count cap");
    const auto size_root = fixture_root();
    for (unsigned i = 0; i < 17; ++i)
        file(size_root, L"a2fo_" + std::to_wstring(i) + L".log",
             std::string(max_file_bytes, 'x'), recent);
    auto capped = collect(size_root, start, now);
    unsigned total = 0;
    for (const auto& entry : capped) total += unsigned(entry.bytes.size());
    require(total == max_total_bytes && capped.size() == 16, "total byte cap");
    watch_case(L"crash_only", 0, false, false);
    watch_case(L"all_sessions", 0, false, true);
    watch_case(L"crash_only", 0xc0000005u, false, true);
    watch_case(L"all_sessions", 0, true, false);
    remove_fixture(root);
    remove_fixture(module_root);
    remove_fixture(count_root);
    remove_fixture(size_root);
    std::puts("log_mailer_native_test: PASS");
    return 0;
}
