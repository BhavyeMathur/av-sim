#define DEBUG false

#include "Rider.h"
#include "RiderManager.h"
#include "io/SimulationConfigs.h"


Rider::Rider(rider_id_t id, coordinate pos, uint8_t pax)
        : id_(id),
          eta_pos_(pos),
          eta_range_(sim::configs.fleet.max_range),
          capacity_(pax) {}

void Rider::assign_request() {
    n_assigned_++;
    assert(n_assigned_ <= 2 && "rider can be assigned a maximum of two requests at a time");
}

// schedule the next waypoint (if any) by pushing it to the global events queue
void Rider::schedule_next_(RiderData &data) {
    assert(!next_scheduled_ && "should not call schedule_next_() if event already scheduled");

    auto &steps_ = data.steps_;
    auto &pos_ = data.pos_;

    // if there are no more steps to take then mark ourselves as IDLE (or DEAD)
    // and return after setting next_scheduled_ = false;
    if (steps_.empty()) {
        next_scheduled_ = false;
        set_state_if_not_dead_(RiderState::Idle);
        return;
    }

    // otherwise we calculate the completion time of the next waypoint
    // by adding dwell_time + actual_eta (movement time)
    // and push this to the global event queue
    const auto &waypoint = steps_.front().waypoint;

    auto [distance, duration] = actual_eta(pos_, waypoint.pos);
    duration += waypoint.dwell_s;

    sim::events.trigger(RiderScheduleWaypoint{id_, distance});
    EventBus::push({sim::clock + duration, RiderWaypoint{id_}});
    next_scheduled_ = true;

    // perform action based on the type of the waypoint
    // at the time when the waypoint is scheduled
    switch (waypoint.kind) {
        case Waypoint::Kind::FirstMile:
            set_state_if_not_dead_(RiderState::FirstMile);
            sim::events.trigger(FirstMileStart{waypoint.request_id, distance});
            break;

        case Waypoint::Kind::WaitForPickup:
            set_state_if_not_dead_(RiderState::PickingUp);
            sim::events.trigger(ArrivedAtPickup{waypoint.request_id});
            break;

        case Waypoint::Kind::LastMile:
            set_state_if_not_dead_(RiderState::LastMile);
            sim::events.trigger(LastMileStart{waypoint.request_id, distance});
            break;

        case Waypoint::Kind::WaitForDropoff:
            set_state_if_not_dead_(RiderState::DroppingOff);
            sim::events.trigger(ArrivedAtDrop{waypoint.request_id});
            break;

        case Waypoint::Kind::ChargeStart:
        case Waypoint::Kind::ChargeDone:
            set_state_if_not_dead_(RiderState::Charging);
            break;

        default:
    }
}

// called when the next rider waypoint is reached
// the rider updates its position and schedules the next waypoint (if any)
void Rider::complete_waypoint() {
    debug("Rider::complete_waypoint(rider_id=%i)", id_);
    assert(!steps_.empty() && "no waypoints to complete");

    auto &data = sim::riders.get_data(id_);
    auto &steps_ = data.steps_;
    auto &pos_ = data.pos_;
    auto &last_commit_at_ = data.last_commit_at_;

    auto waypoint = steps_.front().waypoint;
    steps_.pop_front();

    // perform action based on the type of the waypoint
    // at the time when the waypoint is completed
    switch (waypoint.kind) {
        case Waypoint::Kind::WaitForDropoff:
            debug("Rider::complete_waypoint() rider_id=%i – Waypoint::Kind::WaitForDropoff request_id=%i\n",
                  id_, waypoint.request_id);
            assert(n_assigned_ >= 1 && "rider 'n_assigned_' in invalid state");
            n_assigned_--;

            sim::events.trigger(RequestCompleted{waypoint.request_id});
            break;

        case Waypoint::Kind::ChargeStart:
            sim::events.trigger(RiderChargeStart{id_});
            break;

        case Waypoint::Kind::ChargeDone:
            sim::events.trigger(RiderChargeComplete{id_});
            break;

        default:
    }

    // update position and last commit at
    pos_ = waypoint.pos;
    last_commit_at_ = sim::clock;

    // recalculate the new ETA of all waypoints
    // knowing that this one was completed at 'now'
    recalculate_eta_at_(data);

    // schedule the next waypoint
    next_scheduled_ = false;
    schedule_next_(data);
}

void Rider::recalculate_eta_at_(RiderData &data) {
    auto final_waypoint_at = data.last_commit_at_;
    for (auto &step: data.steps_)
        final_waypoint_at += step.duration();

    eta_at_ = final_waypoint_at;
}

void Rider::set_state_if_not_dead_(RiderState state) {
    if (state_ == RiderState::Dead)
        return;

    sim::events.trigger(RiderStateChange{id_, state_, state});
    state_ = state;
}

std::string Rider::state_to_string(RiderState state) {
    switch (state) {
        case RiderState::Dead:
            return "dead";
        case RiderState::Idle:
            return "idle";

        case RiderState::FirstMile:
            return "fm";
        case RiderState::PickingUp:
            return "wait";
        case RiderState::LastMile:
            return "lm";
        case RiderState::DroppingOff:
            return "drop";

        case RiderState::Repositioning:
            return "service";
        case RiderState::Charging:
            return "charge";

        case RiderState::SIZE:
            throw std::invalid_argument("invalid state");
    }
}
