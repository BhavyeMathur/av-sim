#pragma once

#include <string>
#include <iomanip>
#include <chrono>

std::string utc_now_iso8601() {
    using clock = std::chrono::system_clock;

    const auto now = clock::now();
    const auto t = clock::to_time_t(now);

    std::tm tm{};
    #if defined(_WIN32)
    gmtime_s(&tm, &t);
    #else
    gmtime_r(&t, &tm);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
