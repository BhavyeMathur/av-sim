#include <iostream>
#include "tqdm.h"

using namespace std;


bar::bar(unsigned width, char fill)
        : m_width(width),
          m_fill(fill) {
    m_start_time = chrono::steady_clock::now();
}

void bar::update(unsigned int percent) {
    if (percent > 100)
        percent = 100;

    std::string pbstr(m_width, m_fill);
    unsigned filled = percent * m_width / 100;

    auto now = std::chrono::steady_clock::now();
    auto elapsed_sec =
            std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time).count();

    double rate = (percent > 0) ? static_cast<double>(elapsed_sec) / percent : 0.0;
    int remaining_sec =
            (percent < 100) ? static_cast<int>(rate * (100 - percent)) : 0;

    fprintf(stderr, "\r\33[2K[%-*.*s] %3u%% | [%2llds<%2ds]",
            m_width, filled, pbstr.c_str(), percent, elapsed_sec, remaining_sec);
    fflush(stderr);

    if (percent == 100)
        fprintf(stderr, "\n");
}

namespace tqdm {
    tqdm::tqdm(size_t num, float flush_every, unsigned int width, char fill)
            : bar(width, fill),
              num(num),
              flush_interval(100 * flush_every),
              next_flush(flush_interval) {
    }

    void tqdm::step() {
        cur++;

        auto percent = static_cast<float>(100 * cur) / static_cast<float>(num);
        if (percent >= next_flush) {
            update(static_cast<unsigned int>(percent));
            next_flush += flush_interval;
        }
    }

    void tqdm::complete() {
        update(100);
    }

    tqdm tqdm::operator++(int) {
        return ++(*this);
    }

    tqdm &tqdm::operator++() {
        step();
        return *this;
    }
}
