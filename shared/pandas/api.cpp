#include "api.h"

namespace pd {
    arrow::Status write_table_to_parquet(const std::shared_ptr<arrow::Table> &table,
                                         const std::string &filename,
                                         parquet::Compression::type compression,
                                         int64_t chunk_size) {
        ARROW_ASSIGN_OR_RAISE(auto outfile, arrow::io::FileOutputStream::Open(filename))

        auto writer_props = parquet::WriterProperties::Builder().compression(compression)->build();
        auto arrow_props = parquet::ArrowWriterProperties::Builder().build();

        return parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile,
                                          chunk_size, writer_props, arrow_props);
    }
}
