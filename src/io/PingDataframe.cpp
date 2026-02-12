#include "PingDataframe.h"

#include <pandas.h>
#include <numeric>


using namespace std;

PingDataFrame::PingDataFrame(const string &file) {
    cout << "reading " << file << endl;
    auto pings = pd::read_parquet(file);

    id.resize(pings->num_rows());
    std::iota(id.begin(), id.end(), 0);

    created_at = pd::column_as_vector<pd::uint32>(pings, "created_at");

    pick_lat = pd::column_as_vector<pd::float32>(pings, "pick_lat");
    pick_lon = pd::column_as_vector<pd::float32>(pings, "pick_lon");
    drop_lat = pd::column_as_vector<pd::float32>(pings, "drop_lat");
    drop_lon = pd::column_as_vector<pd::float32>(pings, "drop_lon");

    pick_time = pd::column_as_vector<pd::uint32>(pings, "pick_time");
    drop_time = pd::column_as_vector<pd::uint32>(pings, "drop_time");
    ready_time = pd::column_as_vector<pd::uint32>(pings, "ready_time");
    sla_time = pd::column_as_vector<pd::uint32>(pings, "sla_time");

    predicted_ready_time = pd::column_as_vector<pd::uint32>(pings, "predicted_ready_time");
    predicted_lm_dist = pd::column_as_vector<pd::float32>(pings, "lm_dist");

    pick_zone = pd::column_as_vector<pd::uint16>(pings, "pick_zone");
    drop_zone = pd::column_as_vector<pd::uint16>(pings, "drop_zone");
    customer = pd::column_as_vector<pd::uint16>(pings, "customer_id");
}

size_t PingDataFrame::size() const {
    return pick_lat.size();
}

PingDataFrame::iterator::iterator(const PingDataFrame *df_, size_t i)
        : df(df_),
          index(i) {
}

PingDataFrame::iterator::value_type PingDataFrame::iterator::operator*() const {
    assert(-M_PI / 2 <= df->pick_lat[index] && df->pick_lat[index] < M_PI / 2);
    assert(-M_PI <= df->pick_lon[index] && df->pick_lon[index] < M_PI);

    assert(-M_PI / 2 <= df->drop_lat[index] && df->drop_lat[index] < M_PI / 2);
    assert(-M_PI <= df->drop_lon[index] && df->drop_lon[index] < M_PI);

    return {
            .id = df->id[index],
            .created_at = df->created_at[index],

            .pick_coord = {df->pick_lat[index], df->pick_lon[index]},
            .drop_coord = {df->drop_lat[index], df->drop_lon[index]},

            .pick_time = df->pick_time[index],
            .drop_time = df->drop_time[index],
            .ready_time = df->ready_time[index],
            .sla_time = df->sla_time[index],

            .predicted_ready_time = df->predicted_ready_time[index],
            .predicted_lm_dist = df->predicted_lm_dist[index],

            .pick_zone = df->pick_zone[index],
            .drop_zone = df->drop_zone[index],
            .customer_id = df->customer[index],
    };
}

PingDataFrame::iterator &PingDataFrame::iterator::operator++() {
    ++index;
    return *this;
}

PingDataFrame::iterator PingDataFrame::iterator::operator++(int) {
    auto tmp = *this;
    ++index;
    return tmp;
}

bool PingDataFrame::iterator::operator==(const PingDataFrame::iterator &other) const {
    return index == other.index && df == other.df;
}

bool PingDataFrame::iterator::operator!=(const PingDataFrame::iterator &other) const {
    return !(*this == other);
}

PingDataFrame::iterator PingDataFrame::begin() const {
    return {this, 0};
}

PingDataFrame::iterator PingDataFrame::end() const {
    return {this, size()};
}
