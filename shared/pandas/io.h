#pragma once

#include <string>
#include <memory>

#include "api.h"


namespace pd {
    std::shared_ptr<arrow::Table> read_parquet(const std::string &path);
}
