#pragma once

#include <string>
#include <memory>

#include "api.h"


namespace pd {
    std::shared_ptr<DataFrame> read_parquet(const std::string &path);

    std::shared_ptr<DataFrame> read_csv(const std::string &path);
}
