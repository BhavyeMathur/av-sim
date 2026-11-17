#!/usr/bin/env python3
"""Repair av-sim initialization and hidden worker failures.

Run from the av-sim repository root:
    .venv/bin/python fix_av_sim_runtime.py

Designed for kag924/av-sim master 0be0fcb, including the missing-header edits
from this conversation. Validates all changes before writing, saves source
backups, and can be run again safely. Modifies C++ source files only.

Validated on Linux: Clang18 release build and GCC13 Debug with AddressSanitizer
and UndefinedBehaviorSanitizer. Three deterministic input cases, including
15,000 vehicles, produced native outputs and passed trip/fleet checks.
Validated policy: bounded-h3 with charging:disable. Mac build and the user's
SEMCOG dataset still require their own run. Charging and batching were not
validated by these tests.
"""

from pathlib import Path
from datetime import datetime, timezone
import argparse
import json
import re
import shutil
import sys

PATCHES = json.loads('{"src/World.cpp":{"headers":[],"changes":[{"before":"    sim::events.on([](const RiderWaypoint &event) {\\n        debug(\\"RiderWaypoint(rider_id=%i)\\\\n\\", event.rider_id);\\n        sim::riders.get_rider(event.rider_id).complete_waypoint();\\n    });\\n\\n    sim::events.on([](const RequestAssigned &event) {\\n","after":"    sim::events.on([](const RiderWaypoint &event) {\\n        debug(\\"RiderWaypoint(rider_id=%i)\\\\n\\", event.rider_id);\\n        sim::riders.get_rider(event.rider_id).complete_waypoint();\\n        // Completion can free a vehicle\'s second queued-request slot. Reindex\\n        // after its method returns, because moving pools invalidates references.\\n        sim::riders.update_grid(event.rider_id);\\n    });\\n\\n    sim::events.on([](const RequestAssigned &event) {\\n"},{"before":"    auto alloc_engine = get_allocation_engine();\\n    auto charging_policy = get_charging_policy();\\n\\n    FleetStats();\\n    RiderStats();\\n\\n    // ------------------\\n    sim::events.trigger(SimStart{});\\n","after":"    auto alloc_engine = get_allocation_engine();\\n    auto charging_policy = get_charging_policy();\\n\\n    FleetStats fleet_stats;\\n    RiderStats rider_stats;\\n\\n    // ------------------\\n    sim::events.trigger(SimStart{});\\n"}]},"src/dsa/queues.h":{"headers":["queue"],"changes":[]},"src/dsa/rolling_counter.cpp":{"headers":["cmath"],"changes":[{"before":"rolling_counter::rolling_counter(uint32_t window, uint32_t resolution)\\n        : m_window(window),\\n          m_resolution(resolution),\\n          m_events(static_cast<uint32_t>(ceil(window / resolution))) {\\n}\\n\\nvoid rolling_counter::up(uint32_t clock) {\\n","after":"rolling_counter::rolling_counter(uint32_t window, uint32_t resolution)\\n        : m_window(window),\\n          m_resolution(resolution),\\n          m_events(static_cast<uint32_t>(std::ceil(window / resolution))) {\\n}\\n\\nvoid rolling_counter::up(uint32_t clock) {\\n"}]},"src/io/Database.cpp":{"headers":["stdexcept"],"changes":[]},"src/io/RequestsDataframe.cpp":{"headers":["cmath"],"changes":[]},"src/io/RidersDataframe.cpp":{"headers":["cmath"],"changes":[]},"src/io/SimulationConfigs.h":{"headers":["cstdint"],"changes":[]},"src/main.cpp":{"headers":["vector","string","stdexcept","cstdlib"],"changes":[{"before":"\\n    constexpr size_t MAX_PROCS = 8;\\n\\n    std::vector<pid_t> pids;\\n    size_t next = 0;\\n    size_t alive = 0;\\n\\n","after":"\\n    constexpr size_t MAX_PROCS = 8;\\n\\n    bool failed = false;\\n    size_t next = 0;\\n    size_t alive = 0;\\n\\n"},{"before":"        if (pid < 0) {\\n            perror(\\"wait\\");\\n            exit(1);\\n        }\\n\\n        alive--;\\n","after":"        if (pid < 0) {\\n            perror(\\"wait\\");\\n            exit(1);\\n        }\\n\\n        if (WIFSIGNALED(status)) {\\n            std::cerr << \\"Simulation process \\" << pid << \\" terminated by signal \\"\\n                      << WTERMSIG(status) << \'\\\\n\';\\n            failed = true;\\n        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {\\n            std::cerr << \\"Simulation process \\" << pid << \\" failed\\";\\n            if (WIFEXITED(status))\\n                std::cerr << \\" with exit code \\" << WEXITSTATUS(status);\\n            std::cerr << \'\\\\n\';\\n            failed = true;\\n        }\\n\\n        alive--;\\n"},{"before":"        }\\n\\n        if (pid == 0) {\\n            run(experiments[next]);\\n            exit(0);\\n        }\\n\\n        alive++;\\n","after":"        }\\n\\n        if (pid == 0) {\\n            try {\\n                run(experiments[next]);\\n                exit(0);\\n            } catch (const std::exception &error) {\\n                std::cerr << \\"Simulation failed for \\" << experiments[next]\\n                          << \\": \\" << error.what() << \'\\\\n\';\\n                exit(1);\\n            }\\n        }\\n\\n        alive++;\\n"},{"before":"    while (alive > 0)\\n        wait_one();\\n\\n    return 0;\\n}\\n","after":"    while (alive > 0)\\n        wait_one();\\n\\n    return failed ? 1 : 0;\\n}\\n"}]},"src/riders/Rider.cpp":{"headers":[],"changes":[{"before":"    // otherwise we calculate the completion time of the next waypoint\\n    // by adding dwell_time + actual_eta (movement time)\\n    // and push this to the global event queue\\n    const auto &waypoint = steps_.front().waypoint;\\n\\n    auto [distance, duration] = actual_eta(pos_, waypoint.pos);\\n    duration += waypoint.dwell_s;\\n","after":"    // otherwise we calculate the completion time of the next waypoint\\n    // by adding dwell_time + actual_eta (movement time)\\n    // and push this to the global event queue\\n    const auto waypoint = steps_.front().waypoint;\\n\\n    auto [distance, duration] = actual_eta(pos_, waypoint.pos);\\n    duration += waypoint.dwell_s;\\n"},{"before":"            break;\\n\\n        default:\\n    }\\n}\\n\\n","after":"            break;\\n\\n        default:\\n            break;\\n    }\\n}\\n\\n"},{"before":"// the rider updates its position and schedules the next waypoint (if any)\\nvoid Rider::complete_waypoint() {\\n    debug(\\"Rider::complete_waypoint(rider_id=%i)\\", id_);\\n    assert(!steps_.empty() && \\"no waypoints to complete\\");\\n\\n    auto &data = sim::riders.get_data(id_);\\n    auto &steps_ = data.steps_;\\n    auto &pos_ = data.pos_;\\n    auto &last_commit_at_ = data.last_commit_at_;\\n\\n","after":"// the rider updates its position and schedules the next waypoint (if any)\\nvoid Rider::complete_waypoint() {\\n    debug(\\"Rider::complete_waypoint(rider_id=%i)\\", id_);\\n\\n    auto &data = sim::riders.get_data(id_);\\n    auto &steps_ = data.steps_;\\n    assert(!steps_.empty() && \\"no waypoints to complete\\");\\n    auto &pos_ = data.pos_;\\n    auto &last_commit_at_ = data.last_commit_at_;\\n\\n"},{"before":"            break;\\n\\n        default:\\n    }\\n\\n    // update position and last commit at\\n","after":"            break;\\n\\n        default:\\n            break;\\n    }\\n\\n    // update position and last commit at\\n"},{"before":"        case RiderState::SIZE:\\n            throw std::invalid_argument(\\"invalid state\\");\\n    }\\n}\\n","after":"        case RiderState::SIZE:\\n            throw std::invalid_argument(\\"invalid state\\");\\n    }\\n\\n    throw std::invalid_argument(\\"invalid state\\");\\n}\\n"}]},"src/riders/RiderGrid.cpp":{"headers":[],"changes":[{"before":"#include \\"routing/grid/Grid.h\\"\\n\\n\\nRiderGrid::RiderGrid(size_t n_riders) {\\n    cell_to_riders_.emplace(INVALID_CELL_ID, INVALID_CELL_ID);\\n\\n    rider_id_to_cell_.reserve(n_riders);\\n    rider_id_to_idx_.reserve(n_riders);\\n\\n    sim::events.on<&RiderGrid::on_request_completed>(*this);\\n}\\n\\nRiderPool &RiderGrid::riders_in_cell(cell_id_t cell) {\\n","after":"#include \\"routing/grid/Grid.h\\"\\n\\n\\nRiderGrid::RiderGrid(size_t n_riders)\\n        : rider_id_to_cell_(n_riders, INVALID_CELL_ID),\\n          rider_id_to_idx_(n_riders, 0) {\\n    cell_to_riders_.try_emplace(INVALID_CELL_ID, INVALID_CELL_ID);\\n}\\n\\nRiderPool &RiderGrid::riders_in_cell(cell_id_t cell) {\\n"},{"before":"\\nvoid RiderGrid::emplace(rider_id_t id, coordinate pos, uint8_t pax) {\\n    auto cell = grid::latlon_to_cell(pos);\\n    auto &vec = cell_to_riders_[cell].riders;\\n\\n    rider_id_to_cell_[id] = cell;\\n    rider_id_to_idx_[id] = vec.size();\\n","after":"\\nvoid RiderGrid::emplace(rider_id_t id, coordinate pos, uint8_t pax) {\\n    auto cell = grid::latlon_to_cell(pos);\\n    auto [it, _] = cell_to_riders_.try_emplace(cell, cell);\\n    auto &vec = it->second.riders;\\n\\n    rider_id_to_cell_[id] = cell;\\n    rider_id_to_idx_[id] = vec.size();\\n"},{"before":"\\n    auto old_cell = rider_id_to_cell_[rider_id];\\n    auto old_idx = rider_id_to_idx_[rider_id];\\n    auto &old_pool = cell_to_riders_.at(old_cell);\\n\\n    if (rider.n_requests_assigned() >= 2)\\n        new_cell = INVALID_CELL_ID;\\n","after":"\\n    auto old_cell = rider_id_to_cell_[rider_id];\\n    auto old_idx = rider_id_to_idx_[rider_id];\\n\\n    if (rider.n_requests_assigned() >= 2)\\n        new_cell = INVALID_CELL_ID;\\n"},{"before":"    if (old_cell == new_cell)\\n        return rider;\\n\\n    auto [it, _] = cell_to_riders_.try_emplace(new_cell, new_cell);\\n    auto &new_pool = it->second;\\n    rider_id_to_idx_[rider_id] = new_pool.riders.size();\\n    new_pool.riders.push_back(rider);\\n\\n    if (old_idx != old_pool.riders.size() - 1) {\\n        auto &moved = old_pool.riders.back();\\n","after":"    if (old_cell == new_cell)\\n        return rider;\\n\\n    // Inserting a cell can rehash the map; preserve the vehicle first and\\n    // acquire both pool references only after the insertion.\\n    auto saved_rider = rider;\\n    auto [it, _] = cell_to_riders_.try_emplace(new_cell, new_cell);\\n    auto &new_pool = it->second;\\n    auto &old_pool = cell_to_riders_.at(old_cell);\\n    rider_id_to_idx_[rider_id] = new_pool.riders.size();\\n    new_pool.riders.push_back(saved_rider);\\n\\n    if (old_idx != old_pool.riders.size() - 1) {\\n        auto &moved = old_pool.riders.back();\\n"}]},"src/riders/RiderManager.cpp":{"headers":[],"changes":[{"before":"\\n#include <random>\\n\\nRiderManager::RiderManager(const RidersDataFrame &df) {\\n    std::mt19937 rng(std::random_device{}());\\n    std::discrete_distribution<int> dist{\\n            sim::configs.fleet.frac_2_seater,\\n","after":"\\n#include <random>\\n\\nRiderManager::RiderManager(const RidersDataFrame &df)\\n        : RiderGrid(df.size()),\\n          rider_id_to_data_(df.size()),\\n          n_riders_(df.size()) {\\n    std::mt19937 rng(std::random_device{}());\\n    std::discrete_distribution<int> dist{\\n            sim::configs.fleet.frac_2_seater,\\n"},{"before":"    return rider_id_to_data_[rider_id];\\n}\\n\\nvoid RiderManager::charge(rider_id_t rider_id, coordinate at) {\\n    auto &rider = get_rider(rider_id);\\n    return charge(rider, at);\\n","after":"    return rider_id_to_data_[rider_id];\\n}\\n\\nvoid RiderManager::update_grid(rider_id_t rider_id) {\\n    update(get_rider(rider_id));\\n}\\n\\nvoid RiderManager::charge(rider_id_t rider_id, coordinate at) {\\n    auto &rider = get_rider(rider_id);\\n    return charge(rider, at);\\n"},{"before":"\\nvoid RiderManager::charge(Rider &rider, coordinate at) {\\n    EventLock <RiderUpdatedETAPos> lock;\\n\\n    auto [distance, _] = approx_eta(rider.eta_pos_, at);\\n    rider.eta_range_ -= distance;\\n","after":"\\nvoid RiderManager::charge(Rider &rider, coordinate at) {\\n    EventLock <RiderUpdatedETAPos> lock;\\n    auto rider_id = rider.id();\\n\\n    auto [distance, _] = approx_eta(rider.eta_pos_, at);\\n    rider.eta_range_ -= distance;\\n"},{"before":"    push_waypoints(rider, Waypoint{at, 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart},\\n                   Waypoint{at, approx_charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});\\n\\n    rider.eta_range_ = sim::configs.fleet.max_range;  // we will be back to full capacity once charging is complete\\n}\\n","after":"    push_waypoints(rider, Waypoint{at, 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart},\\n                   Waypoint{at, approx_charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});\\n\\n    // Waypoint insertion can move the vehicle into another pool.\\n    get_rider(rider_id).eta_range_ = sim::configs.fleet.max_range;\\n}\\n"}]},"src/riders/RiderManager.h":{"headers":[],"changes":[{"before":"\\n    [[nodiscard]] RiderData &get_data(rider_id_t rider_id);\\n\\n    template<typename... W> requires (std::same_as<std::decay_t<W>, Waypoint> &&...)\\n\\n    auto push_waypoints(rider_id_t rider_id, W &&... wp) {\\n","after":"\\n    [[nodiscard]] RiderData &get_data(rider_id_t rider_id);\\n\\n    void update_grid(rider_id_t rider_id);\\n\\n    template<typename... W> requires (std::same_as<std::decay_t<W>, Waypoint> &&...)\\n\\n    auto push_waypoints(rider_id_t rider_id, W &&... wp) {\\n"},{"before":"        auto rider_id = rider.id();\\n        auto &rider_data = get_data(rider_id);\\n        distance_t total_distance = 0;\\n        coordinate last_pos{};\\n\\n        auto process = [&](const Waypoint &w) {\\n            auto [distance, duration] = approx_eta(last_pos, w.pos);\\n","after":"        auto rider_id = rider.id();\\n        auto &rider_data = get_data(rider_id);\\n        distance_t total_distance = 0;\\n        coordinate last_pos = rider.eta_pos_;\\n\\n        auto process = [&](const Waypoint &w) {\\n            auto [distance, duration] = approx_eta(last_pos, w.pos);\\n"},{"before":"\\n        rider.eta_range_ -= total_distance;\\n        rider.eta_pos_ = last_pos;\\n        rider = update(rider);\\n        sim::events.trigger(RiderUpdatedETAPos{rider_id, total_distance});\\n\\n        if (!rider.next_scheduled_)\\n            rider.schedule_next_(rider_data);\\n    }\\n\\n    void charge(rider_id_t rider_id, coordinate at);\\n","after":"\\n        rider.eta_range_ -= total_distance;\\n        rider.eta_pos_ = last_pos;\\n        update(rider);\\n        sim::events.trigger(RiderUpdatedETAPos{rider_id, total_distance});\\n\\n        // Both indexing and charging callbacks can move a vehicle. Look it up\\n        // by ID instead of retaining a reference into its former pool.\\n        auto &current_rider = get_rider(rider_id);\\n        if (!current_rider.next_scheduled_)\\n            current_rider.schedule_next_(rider_data);\\n    }\\n\\n    void charge(rider_id_t rider_id, coordinate at);\\n"}]},"src/routing/Distance.cpp":{"headers":["cmath"],"changes":[]}}')

def normalized(text, names):
    text = text.replace('\r\n', '\n').rstrip('\n') + '\n'
    for name in names:
        text = re.sub(r'^[ \t]*#include[ \t]*<' + re.escape(name) + r'>[^\n]*\n', '', text, flags=re.M)
    return text

def repaired_text(text, specification, rel):
    text = normalized(text, specification['headers'])
    for number, change in enumerate(specification['changes'], 1):
        before, after = change['before'], change['after']
        if before == after:
            continue
        if text.count(before) == 1:
            text = text.replace(before, after, 1)
        elif text.count(after) == 1:
            continue
        else:
            raise ValueError(f'{rel}: cannot safely match change {number}. No files have been changed.')
    includes = ''.join(f'#include <{name}>\n' for name in specification['headers'])
    if includes:
        if text.startswith('#pragma once\n'):
            text = text.replace('#pragma once\n', '#pragma once\n' + includes, 1)
        else:
            index = text.find('#include ')
            if index < 0:
                raise ValueError(f'{rel}: include section not found. No files have been changed.')
            text = text[:index] + includes + text[index:]
    return text

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd(), help='av-sim repository root')
    parser.add_argument('--check', action='store_true', help='validate and show changes without writing')
    args = parser.parse_args()
    root = args.repo.resolve()
    if not (root / 'CMakeLists.txt').is_file():
        raise ValueError('Run this command from the av-sim folder containing CMakeLists.txt.')
    prepared = []
    for rel, specification in PATCHES.items():
        path = root / rel
        if not path.is_file() or path.is_symlink():
            raise ValueError(f'Missing or unexpected source file: {path}. No files have been changed.')
        data = path.read_bytes()
        text = data.decode('utf-8')
        updated = repaired_text(text, specification, rel).encode('utf-8')
        if data != updated:
            prepared.append((rel, path, data, updated))
    if not prepared:
        print('Runtime repair is already applied.')
        return 0
    for rel, *_ in prepared:
        print(('Would update: ' if args.check else 'Update: ') + rel)
    if args.check:
        print('Check passed; no files changed.')
        return 0
    suffix = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    backup = root / 'av-sim-runtime-backups' / suffix
    for rel, path, data, updated in prepared:
        destination = backup / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    written = []
    try:
        for rel, path, data, updated in prepared:
            path.write_bytes(updated)
            written.append((path, data))
    except Exception:
        for path, data in written:
            path.write_bytes(data)
        raise
    print(f'Backup: {backup}')
    print('Repair applied. Rebuild and rerun the simulator:')
    print('  cmake --build cmake-build-first-run --target sim -j 4')
    print('  ./sim runs/ann-arbor-first-run/manifest.txt')
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, UnicodeError) as error:
        print(f'Repair stopped: {error}', file=sys.stderr)
        sys.exit(1)
