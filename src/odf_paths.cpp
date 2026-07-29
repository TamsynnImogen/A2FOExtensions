#include "odf_paths.hpp"

#include <algorithm>
#include <cctype>

namespace a2fo {
namespace {

std::string normalize_virtual_path(std::string path) {
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) {
        if (ch == '/' || ch == '\\') {
            return '\\';
        }
        return static_cast<char>(std::tolower(ch));
    });
    return path;
}

bool has_odf_extension(const std::string& basename) {
    return basename.size() >= 4 &&
           basename.compare(basename.size() - 4, 4, ".odf") == 0;
}

}  // namespace

std::string recursive_odf_basename_key(const std::string& requested_path) {
    // ParameterDB normally asks FOFS_ItemGet for a bare basename. Supporting
    // odf\... paths too keeps the helper useful at adjacent filesystem hooks,
    // while rejecting texture/SOD paths that happen to share a basename.
    const std::string normalized = normalize_virtual_path(requested_path);
    const std::size_t slash = normalized.find_last_of('\\');
    if (slash == std::string::npos) {
        return has_odf_extension(normalized) ? normalized : std::string{};
    }
    if (slash + 1 >= normalized.size()) {
        return {};
    }

    const std::string directory = normalized.substr(0, slash);
    if (directory != "odf" && directory.compare(0, 4, "odf\\") != 0) {
        return {};
    }

    const std::string basename = normalized.substr(slash + 1);
    if (!has_odf_extension(basename)) {
        return {};
    }
    return basename;
}

bool find_recursive_odf_alias(
    const std::string& requested_path,
    const std::map<std::string, std::string>& aliases,
    std::string& aliased_path) {
    // Kept separate from the injected hook so path normalization and fallback
    // behavior can be exercised by ordinary host unit tests.
    aliased_path.clear();
    const std::string basename = recursive_odf_basename_key(requested_path);
    if (basename.empty()) {
        return false;
    }
    const auto found = aliases.find(basename);
    if (found == aliases.end() || found->second.empty() ||
        normalize_virtual_path(found->second) ==
            normalize_virtual_path(requested_path)) {
        return false;
    }
    aliased_path = found->second;
    return true;
}

}  // namespace a2fo
