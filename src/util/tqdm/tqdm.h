#ifndef RIDER_SIM_PROGRESSBAR_H
#define RIDER_SIM_PROGRESSBAR_H

#include <chrono>


class bar {
public:
    void update(unsigned percent);

protected:
    explicit bar(unsigned width = 50, char fill = '#');

private:
    unsigned m_width;
    char m_fill;
    std::chrono::steady_clock::time_point m_start_time;
};

namespace tqdm {
    class tqdm : public bar { // NOLINT(*-pro-type-member-init)
    public:
        explicit tqdm(size_t num, float flush_interval = 0.1, unsigned width = 50, char fill = '#');

        void step();

        void complete();

        tqdm &operator++();

        tqdm operator++(int);

    private:
        size_t num;
        size_t cur = 0;
        float flush_interval;
        float next_flush;
    };
}

#endif //RIDER_SIM_PROGRESSBAR_H
