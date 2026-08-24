#pragma once

// Keep this human-readable fingerprint in both the core and renderer-helper
// logs. Test deployments are copied between several Windows systems, so a
// timestamped log alone cannot prove which binary actually ran.
constexpr const char* A2FO_BUILD_ID =
    "20260822-renderer-system-isolation-03";
