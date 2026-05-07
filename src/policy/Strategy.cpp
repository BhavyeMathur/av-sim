#include "Strategy.h"
#include "extern.h"


Strategy::Strategy() {
    sim::events.on<&Strategy::on_request>(*this);
}

void Strategy::on_request(const RequestCreated &event) {
    auto &req = sim::requests[event.request_id];
    assign_request(req);
}
