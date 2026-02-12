#include "io.h"

#include <stdexcept>
#include <iostream>

#include <arrow/io/api.h>
#include <arrow/pretty_print.h>
#include <parquet/arrow/reader.h>


using namespace std;

namespace pd {
    shared_ptr<arrow::Table> read_parquet(const string &path) {
        arrow::MemoryPool *pool = arrow::default_memory_pool();

        auto input_result = arrow::io::ReadableFile::Open(path);
        if (!input_result.ok())
            throw runtime_error("Failed to open file: " + path + " — " + input_result.status().ToString());

        shared_ptr<arrow::io::RandomAccessFile> input = input_result.ValueOrDie();

        auto reader_result = parquet::arrow::OpenFile(input, pool);
        if (!reader_result.ok())
            throw runtime_error("Failed to open Parquet reader: " + reader_result.status().ToString());

        unique_ptr<parquet::arrow::FileReader> arrow_reader = std::move(reader_result).ValueOrDie();

        shared_ptr<DataFrame> table;
        auto status = arrow_reader->ReadTable(&table);
        if (!status.ok())
            throw runtime_error("Failed to read table: " + status.ToString());

        return table;
    }
}
