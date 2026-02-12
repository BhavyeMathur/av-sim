#pragma once

#include "includes.h"
#include <ringbuffer.h>


class rolling_counter {
public:
    rolling_counter(duration_t window, duration_t resolution);

    void up();

    void down();

    [[nodiscard]] uint32_t count() const;

private:
    struct Event {
        timestamp_t at;
        uint32_t count;
    };

    duration_t m_window;
    duration_t m_resolution;

    uint32_t m_count = 0;
    ringbuffer<Event> m_events;

    void _evict();
};
