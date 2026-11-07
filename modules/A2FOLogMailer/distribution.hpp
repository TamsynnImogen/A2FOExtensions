#pragma once
// Public distribution defaults for the optional log mailer. This submission
// token ships to players and is not a confidential mailbox credential. Gmail
// credentials remain exclusively in the relay's server-side secret storage.
namespace log_mailer::distribution {
inline constexpr wchar_t relay_url[] = L"https://a2fo-log-relay.aclaydon1.workers.dev/v1/crash-reports";
inline constexpr wchar_t submission_token[] = L"080b26377c5182a60daa9df4c51e5132ee58fa06e59fc53d";
} // namespace log_mailer::distribution
