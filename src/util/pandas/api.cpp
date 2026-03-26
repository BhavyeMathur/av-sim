#include "api.h"

namespace pd {
    std::shared_ptr<arrow::Schema> make_schema(const std::vector<AnyColumn> &cols) {
        std::vector<std::shared_ptr<arrow::Field>> fields;
        fields.reserve(cols.size());

        for (const auto &c: cols)
            fields.emplace_back(arrow::field(c.name, c.type));

        return arrow::schema(fields);
    }

    std::vector<std::shared_ptr<Series>> make_columns(const std::vector<AnyColumn> &cols) {
        std::vector<std::shared_ptr<Series>> arrays;
        arrays.reserve(cols.size());

        for (const auto &c: cols) {
            auto arr = c.make_array();
            if (!arr.ok())
                throw std::runtime_error("failed to convert vector to series");

            arrays.emplace_back(arr.ValueUnsafe());
        }

        return arrays;
    }

    std::shared_ptr<DataFrame> make_table(const std::vector<AnyColumn> &cols) {
        if (cols.empty())
            throw std::invalid_argument("table needs at least one column");

        auto nrows = cols.front().size;
        for (const auto &c: cols)
            if (c.size != nrows)
                throw std::invalid_argument("all columns must have the same length");

        auto schema = make_schema(cols);
        auto arrays = make_columns(cols);

        return DataFrame::Make(schema, arrays, nrows);
    }

    void write_table_to_parquet(const std::shared_ptr<DataFrame> &table, const std::string &filename,
                                parquet::Compression::type compression, int64_t chunk_size) {
        auto outfile = arrow::io::FileOutputStream::Open(filename);
        if (!outfile.ok())
            throw std::runtime_error("failed to open filename");

        auto writer_props = parquet::WriterProperties::Builder().compression(compression)->build();
        auto arrow_props = parquet::ArrowWriterProperties::Builder().build();

        if (!parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile.ValueUnsafe(),
                                        chunk_size, writer_props, arrow_props).ok())
            throw std::runtime_error("failed to write table to parquet");
    }
}
