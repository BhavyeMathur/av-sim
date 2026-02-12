#include <iostream>
#include "tqdm.h"

using namespace std;


ProgressBar::ProgressBar(unsigned width, char fill)
        : m_width(width),
          m_fill(fill) {
    m_start_time = chrono::steady_clock::now();
}

void ProgressBar::update(unsigned int percent) {
    if (percent > 100)
        percent = 100;

    char *pbstr = new char[m_width + 1];
    memset(pbstr, m_fill, m_width);
    pbstr[m_width] = '\0';
    unsigned filled = percent * m_width / 100;

    auto now = chrono::steady_clock::now();
    auto elapsed_sec = chrono::duration_cast<chrono::seconds>(now - m_start_time).count();
    double rate = (percent > 0) ? static_cast<double>(elapsed_sec) / percent : 0;
    int remaining_sec = (percent < 100) ? static_cast<int>(rate * (100 - percent)) : 0;

    fprintf(stderr, "\r[%-*.*s] %3u%% | [%2llds<%2ds]\n",
            m_width, filled, pbstr, percent, elapsed_sec, remaining_sec);

    if (percent == 100)
        fprintf(stderr, "\n");

    delete[] pbstr;
}

namespace tqdm {
    tqdm::tqdm(size_t num, float flush_every, unsigned int width, char fill)
            : ProgressBar(width, fill),
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
