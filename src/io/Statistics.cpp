#include "Statistics.h"
#include "allocation/AllocationEngine.h"

#include "pandas.h"


using namespace std;

void Statistics::resize(size_t n) {
    m_order_delivered.resize(n);
    m_order_fm_dist.resize(n);
    m_order_lm_dist.resize(n);
    m_order_start_at.resize(n);
    m_order_fm_time.resize(n);
    m_order_wait_time.resize(n);
    m_order_lm_time.resize(n);
    m_order_drop_time.resize(n);
    m_order_rider_id.resize(n);
}

void Statistics::assign_order(const AllocationStatistic &result) {
    m_order_delivered[result.order_id] = true;
    m_order_fm_dist[result.order_id] = result.fm_dist;
    m_order_lm_dist[result.order_id] = result.lm_dist;
    m_order_start_at[result.order_id] = result.start_at;
    m_order_fm_time[result.order_id] = result.fm_time;
    m_order_wait_time[result.order_id] = result.wait_time;
    m_order_lm_time[result.order_id] = result.lm_time;
    m_order_drop_time[result.order_id] = result.drop_time;
    m_order_rider_id[result.order_id] = *result.rider_id;;
}

void Statistics::save(const std::string &filepath) const {
    auto table = pd::make_table(pd::col("completed", m_order_delivered),
                                pd::col("fm_dist", m_order_fm_dist),
                                pd::col("lm_dist", m_order_lm_dist),
                                pd::col("start_at", m_order_start_at),
                                pd::col("fm_time", m_order_fm_time),
                                pd::col("wait_time", m_order_wait_time),
                                pd::col("lm_time", m_order_lm_time),
                                pd::col("drop_time", m_order_drop_time),
                                pd::col("rider", m_order_rider_id)).ValueOrDie();

    if (!pd::write_table_to_parquet(table, filepath).ok())
        throw std::runtime_error("Failed to write the output file to " + filepath);
}
