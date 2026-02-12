#include "ZoneClusters.h"
#include "io/SimulationConfigs.h"

#include <fstream>
#include <sstream>
#include <vector>


namespace sim {
    extern thread_local SimulationConfigs configs;
}

ZoneClusters::ZoneClusters()
        : m_zone_to_cluster(
        [&]() {
            auto filename = "data_sim/zones/" + sim::configs.get<std::string>("zones") + ".txt";
            std::ifstream file(filename);
            std::vector<std::pair<zone_id_t, Zone>> vec;

            if (!file.is_open())
                throw std::invalid_argument("could not open zone file '" + filename + "'");
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // ignore header

            zone_id_t zone_id;
            Zone zone_data{};

            while (file >> zone_id >> zone_data)
                vec.emplace_back(zone_id, zone_data);
            file.close();

            return std::unordered_map{vec.begin(), vec.end()};
        }()),

          m_cluster_to_zones([&]() {
              auto filename = "data_sim/clusters/" + sim::configs.get<std::string>("clusters") + ".txt";
              std::ifstream file(filename);
              std::unordered_map<cluster_id_t, std::unordered_set<zone_id_t>> map;

              if (!file.is_open())
                  throw std::invalid_argument("could not open clusters file '" + filename + "'");
              file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // ignore header

              cluster_id_t cluster_id;
              std::string line;

              while (std::getline(file, line)) {
                  if (line.empty())
                      continue;

                  std::istringstream iss(line);
                  if (!(iss >> cluster_id))
                      continue;

                  std::unordered_set<zone_id_t> zones;
                  zone_id_t z;
                  while (iss >> z)
                      zones.insert(z);

                  map.emplace(cluster_id, std::move(zones));
              }

              file.close();
              return map;
          }()) {
}

ZoneClusters::ZoneClusters(bool) {}

bool ZoneClusters::is_deliverable(zone_id_t rider_zone, zone_id_t drop_zone) const {
    auto rider_cluster = m_zone_to_cluster.find(rider_zone);
    if (rider_cluster == m_zone_to_cluster.end())
        return rider_zone == drop_zone;

    return m_cluster_to_zones.at(rider_cluster->second.cluster).contains(drop_zone);
}

Zone ZoneClusters::operator[](zone_id_t zone_id) const {
    return m_zone_to_cluster.at(zone_id);
}

std::istream &operator>>(std::istream &in, Zone &zone) {
    return in >> zone.cluster >> zone.centroid;
}
