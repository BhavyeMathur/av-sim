#include "rolling_counter.h"

namespace sim {
    extern thread_local timestamp_t clock;
}

rolling_counter::rolling_counter(duration_t window, duration_t resolution)
        : m_window(window),
          m_resolution(resolution),
          m_events(static_cast<size_t>(ceil(window / resolution))) {
}

void rolling_counter::up() {
    _evict();

    m_count++;
    if (m_events.empty())
        return m_events.push_back({sim::clock, 1});

    auto &last = m_events.back();
    assert(sim::clock >= last.at);

    if (sim::clock <= last.at + m_resolution) {
        last.count++;
        return;
    }
    m_events.push_back({sim::clock, 1});
}

void rolling_counter::down() {
    _evict();

    if (m_events.empty())
        return;

    auto &last = m_events.back();
    assert(sim::clock >= last.at);

    if (sim::clock <= last.at + m_resolution and last.count > 0) {
        last.count--;
        m_count--;
    }
}

void rolling_counter::_evict() {
    const timestamp_t cutoff = sim::clock - m_window;
    while (!m_events.empty() and m_events.front().at < cutoff) {
        m_count -= m_events.front().count;
        m_events.pop_front();
    }
}

uint32_t rolling_counter::count() const {
    return m_count;
}
