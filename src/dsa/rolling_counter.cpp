#include "rolling_counter.h"


rolling_counter::rolling_counter(uint32_t window, uint32_t resolution)
        : m_window(window),
          m_resolution(resolution),
          m_events(static_cast<uint32_t>(ceil(window / resolution))) {
}

void rolling_counter::up(uint32_t clock) {
    _evict(clock);

    m_count++;
    if (m_events.empty())
        return m_events.push_back({clock, 1});

    auto &last = m_events.back();
    assert(clock >= last.at);

    if (clock <= last.at + m_resolution) {
        last.count++;
        return;
    }
    m_events.push_back({clock, 1});
}

void rolling_counter::down(uint32_t clock) {
    _evict(clock);

    if (m_events.empty())
        return;

    auto &last = m_events.back();
    assert(clock >= last.at);

    if (clock <= last.at + m_resolution and last.count > 0) {
        last.count--;
        m_count--;
    }
}

void rolling_counter::_evict(uint32_t clock) {
    const auto cutoff = clock - m_window;
    while (!m_events.empty() and m_events.front().at < cutoff) {
        m_count -= m_events.front().count;
        m_events.pop_front();
    }
}

uint32_t rolling_counter::count() const {
    return m_count;
}
