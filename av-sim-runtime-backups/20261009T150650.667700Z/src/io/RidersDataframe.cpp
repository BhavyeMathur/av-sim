#include "RidersDataframe.h"

#include <pandas.h>


using namespace std;

RidersDataFrame::RidersDataFrame(const string &file) {
    cout << "reading " << file << endl;
    auto riders = pd::read_parquet(file);

    lat = pd::column_as_vector<pd::float32>(riders, "lat");
    lon = pd::column_as_vector<pd::float32>(riders, "lon");
}

size_t RidersDataFrame::size() const {
    return lat.size();
}

RidersDataFrame::iterator::iterator(const RidersDataFrame *df_, size_t i)
        : df(df_),
          index(i) {
}

RidersDataFrame::iterator::value_type RidersDataFrame::iterator::operator*() const {
    assert(-M_PI / 2 <= df->lat[index] && df->lat[index] < M_PI / 2);
    assert(-M_PI <= df->lon[index] && df->lon[index] < M_PI);

    return Rider{
            .lat=df->lat[index],
            .lon=df->lon[index],
    };
}

RidersDataFrame::iterator &RidersDataFrame::iterator::operator++() {
    ++index;
    return *this;
}

RidersDataFrame::iterator RidersDataFrame::iterator::operator++(int) {
    auto tmp = *this;
    ++index;
    return tmp;
}

bool RidersDataFrame::iterator::operator==(const RidersDataFrame::iterator &other) const {
    return index == other.index && df == other.df;
}

bool RidersDataFrame::iterator::operator!=(const RidersDataFrame::iterator &other) const {
    return !(*this == other);
}

RidersDataFrame::iterator RidersDataFrame::begin() const {
    return {this, 0};
}

RidersDataFrame::iterator RidersDataFrame::end() const {
    return {this, size()};
}
