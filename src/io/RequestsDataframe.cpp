#include "RequestsDataframe.h"

#include <pandas.h>


using namespace std;

RequestsDataFrame::RequestsDataFrame(const string &file) {
    cout << "reading " << file << endl;
    auto reqs = pd::read_parquet(file);

    id.resize(reqs->num_rows());
    std::iota(id.begin(), id.end(), 0);

    created_at = pd::column_as_vector<pd::uint32>(reqs, "created_at");

    pick_lat = pd::column_as_vector<pd::float32>(reqs, "pick_lat");
    pick_lon = pd::column_as_vector<pd::float32>(reqs, "pick_lon");
    drop_lat = pd::column_as_vector<pd::float32>(reqs, "drop_lat");
    drop_lon = pd::column_as_vector<pd::float32>(reqs, "drop_lon");

    predicted_lm_dist = pd::column_as_vector<pd::float32>(reqs, "predicted_lm_dist");
}

size_t RequestsDataFrame::size() const {
    return pick_lat.size();
}

RequestsDataFrame::iterator::iterator(const RequestsDataFrame *df_, size_t i)
        : df(df_),
          index(i) {
}

RequestsDataFrame::iterator::value_type RequestsDataFrame::iterator::operator*() const {
    assert(-M_PI / 2 <= df->pick_lat[index] && df->pick_lat[index] < M_PI / 2);
    assert(-M_PI <= df->pick_lon[index] && df->pick_lon[index] < M_PI);

    assert(-M_PI / 2 <= df->drop_lat[index] && df->drop_lat[index] < M_PI / 2);
    assert(-M_PI <= df->drop_lon[index] && df->drop_lon[index] < M_PI);

    return {df->id[index], df->created_at[index],
            {df->pick_lat[index], df->pick_lon[index]},
            {df->drop_lat[index], df->drop_lon[index]},
            df->predicted_lm_dist[index],
    };
}

RequestsDataFrame::iterator &RequestsDataFrame::iterator::operator++() {
    ++index;
    return *this;
}

RequestsDataFrame::iterator RequestsDataFrame::iterator::operator++(int) {
    auto tmp = *this;
    ++index;
    return tmp;
}

bool RequestsDataFrame::iterator::operator==(const RequestsDataFrame::iterator &other) const {
    return index == other.index && df == other.df;
}

bool RequestsDataFrame::iterator::operator!=(const RequestsDataFrame::iterator &other) const {
    return !(*this == other);
}

RequestsDataFrame::iterator RequestsDataFrame::begin() const {
    return {this, 0};
}

RequestsDataFrame::iterator RequestsDataFrame::end() const {
    return {this, size()};
}
