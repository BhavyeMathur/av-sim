#pragma once

#include <ringbuffer.h>

class rolling_counter {
public:
    rolling_counter(uint32_t window, uint32_t resolution);

    void up(uint32_t clock);

    void down(uint32_t clock);

    [[nodiscard]] uint32_t count() const;

private:
    struct Event {
        uint32_t at;
        uint32_t count;
    };

    uint32_t m_window;
    uint32_t m_resolution;

    uint32_t m_count = 0;
    ringbuffer<Event> m_events;

    void _evict(uint32_t clock);
};
