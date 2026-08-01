#pragma once

#include <map>
#include <string>

namespace a2fo {

// Returns a lowercase basename for a bare .odf request or a request inside the
// virtual odf directory tree. Other filesystem requests return an empty string.
std::string recursive_odf_basename_key(const std::string& requested_path);

// Looks up a recursive winner by basename. The map values are complete
// virtual paths such as odf\custom\ships\example.odf.
bool find_recursive_odf_alias(
    const std::string& requested_path,
    const std::map<std::string, std::string>& aliases,
    std::string& aliased_path);

}  // namespace a2fo
