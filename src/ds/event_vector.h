#pragma once

#include "includes.h"
#include <algorithm>


namespace sim {
    extern thread_local timestamp_t clock;
}

template<typename T, auto FieldPtr>
    class event_vector {
        static_assert(std::is_member_object_pointer_v<decltype(FieldPtr)>,
                      "FieldPtr must be a pointer to a data member of T");
    public:
        event_vector(std::vector<T> &&events)
                : m_events(std::move(events)) {

            std::sort(m_events.begin(), m_events.end(),
                      [](const T &a, const T &b) {
                          return (a.*FieldPtr) < (b.*FieldPtr);
                      });
            m_next = m_events.begin();
        }

        [[nodiscard]] size_t size() const {
            return m_events.size();
        }

        [[nodiscard]] std::optional<T> peek() const {
            if (m_next == m_events.end())
                return std::nullopt;
            return *m_next;
        }

        std::optional<T> check() {
            if (m_next == m_events.end() || ((*m_next).*FieldPtr) > sim::clock)
                return std::nullopt;
            return *m_next++;
        }

    private:
        std::vector<T> m_events;
        typename std::vector<T>::iterator m_next;
    };
