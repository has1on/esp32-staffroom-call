#pragma once
// Pure C++ policy: also exercised by the desktop regression tests.
#include <cstdint>
#include <string>
namespace door {
enum class Outcome { Accepted, PlaybackFailed, Rejected, Unknown };
constexpr uint32_t SAFE_RETRY_MS = 9U * 60U * 1000U;
inline bool retryWindowOpen(uint32_t now, uint32_t started) {
    return uint32_t(now - started) <= SAFE_RETRY_MS;
}
inline Outcome classifyReply(int status, bool correlated,
                             const std::string &state, const std::string &error) {
    if (status == 200 || status == 202) {
        if (!correlated) return Outcome::Unknown;
        if (state == "queued" || state == "playing" || state == "done")
            return Outcome::Accepted;
        if (state == "failed") return Outcome::PlaybackFailed;
        return Outcome::Unknown; // Unknown states must never look like success.
    }
    // Only the Audio protocol's definite pre-acceptance refusals are retryable
    // as NEW requests. Timeout, restart, conflict, malformed reply etc. lock UI.
    if ((status == 400 && (error == "invalid_id" || error == "invalid_json")) ||
        (status == 403 && (error == "unauthorized" || error == "wrong_system")) ||
        (status == 404 && error == "id_not_found") ||
        (status == 422 && error == "wav_missing_or_invalid") ||
        (status == 429 && (error == "queue_full" || error == "cooldown_30s" ||
                          error == "history_full_retry_later")))
        return Outcome::Rejected;
    return Outcome::Unknown;
}
} // namespace door
