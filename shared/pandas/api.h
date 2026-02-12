#pragma once

#include <stdexcept>
#include <memory>
#include <vector>
#include <type_traits>
#include <string>
#include <cstdint>

#include <arrow/api.h>
#include <arrow/table.h>
#include <arrow/io/api.h>
#include <arrow/compute/api.h>
#include <parquet/arrow/writer.h>

namespace {
    template <typename T>
        std::shared_ptr<arrow::DataType> arrow_type() {
            using U = std::decay_t<T>;

            if constexpr (std::is_same_v<U, bool>)               return arrow::boolean();
            else if constexpr (std::is_same_v<U, int8_t>)        return arrow::int8();
            else if constexpr (std::is_same_v<U, uint8_t>)       return arrow::uint8();
            else if constexpr (std::is_same_v<U, int16_t>)       return arrow::int16();
            else if constexpr (std::is_same_v<U, uint16_t>)      return arrow::uint16();
            else if constexpr (std::is_same_v<U, int32_t>)       return arrow::int32();
            else if constexpr (std::is_same_v<U, uint32_t>)      return arrow::uint32();
            else if constexpr (std::is_same_v<U, int64_t>)       return arrow::int64();
            else if constexpr (std::is_same_v<U, uint64_t>)      return arrow::uint64();
            else if constexpr (std::is_same_v<U, float>)         return arrow::float32();
            else if constexpr (std::is_same_v<U, double>)        return arrow::float64();
            else if constexpr (std::is_same_v<U, std::string>)   return arrow::utf8();
        }
}


namespace pd {
    typedef arrow::Table DataFrame;
    typedef arrow::Array Series;

    typedef arrow::FloatScalar float32;
    typedef arrow::DoubleScalar float64;

    typedef arrow::Int8Scalar int8;
    typedef arrow::Int16Scalar int16;
    typedef arrow::Int32Scalar int32;
    typedef arrow::Int64Scalar int64;

    typedef arrow::UInt8Scalar uint8;
    typedef arrow::UInt16Scalar uint16;
    typedef arrow::UInt32Scalar uint32;
    typedef arrow::UInt64Scalar uint64;

    template<typename ArrowArrayType, typename CType = ArrowArrayType::ValueType>
        std::vector<CType> column_as_vector(std::shared_ptr<arrow::Table> &df,
                                            const std::string &col_name) {
            auto column = df->GetColumnByName(col_name);
            if (!column)
                throw std::runtime_error("Column not found: " + col_name);

            if (column->type()->id() != ArrowArrayType().type->id())
                throw std::runtime_error("'" + col_name + "' expected " + ArrowArrayType().type->name() + ", found "
                                         + column->type()->name());

            std::vector<CType> result;
            result.reserve(column->length());

            for (int64_t i = 0; i < column->length(); i++) {
                auto maybe_scalar = column->GetScalar(i);
                if (!maybe_scalar.ok())
                    throw std::runtime_error("Failed to get scalar: " + maybe_scalar.status().ToString());

                auto scalar = maybe_scalar.ValueOrDie();
                if (!scalar->is_valid)
                    throw std::runtime_error("Null value encountered at index " + std::to_string(i));

                auto typed_scalar = std::static_pointer_cast<ArrowArrayType>(scalar);
                result.push_back(typed_scalar->value);
            }

            return result;
        }

    template<typename T>
        arrow::Result<std::shared_ptr<arrow::Array>> vector_as_column(const std::vector<T> &data) {
            using U = std::decay_t<T>;

            // -------- string --------
            if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, const char *>) {
                arrow::StringBuilder builder;
                ARROW_RETURN_NOT_OK(builder.Reserve(static_cast<int64_t>(data.size())));
                for (const auto &s: data)
                    ARROW_RETURN_NOT_OK(builder.Append(s));
                std::shared_ptr<arrow::Array> arr;
                ARROW_RETURN_NOT_OK(builder.Finish(&arr));
                return arr;
            }

            // -------- boolean --------
            else if constexpr (std::is_same_v<U, bool>) {
                arrow::BooleanBuilder builder;
                ARROW_RETURN_NOT_OK(builder.AppendValues(data));
                std::shared_ptr<arrow::Array> arr;
                ARROW_RETURN_NOT_OK(builder.Finish(&arr));
                return arr;
            }

            // -------- numeric --------
            else if constexpr (std::is_integral_v<U> || std::is_floating_point_v<U>) {
                using BuilderT =
                        std::conditional_t<std::is_same_v<U, int8_t>, arrow::Int8Builder,
                        std::conditional_t<std::is_same_v<U, uint8_t>, arrow::UInt8Builder,
                        std::conditional_t<std::is_same_v<U, int16_t>, arrow::Int16Builder,
                        std::conditional_t<std::is_same_v<U, uint16_t>, arrow::UInt16Builder,
                        std::conditional_t<std::is_same_v<U, int32_t>, arrow::Int32Builder,
                        std::conditional_t<std::is_same_v<U, uint32_t>, arrow::UInt32Builder,
                        std::conditional_t<std::is_same_v<U, int64_t>, arrow::Int64Builder,
                        std::conditional_t<std::is_same_v<U, uint64_t>, arrow::UInt64Builder,
                        std::conditional_t<std::is_same_v<U, float>, arrow::FloatBuilder,
                        std::conditional_t<std::is_same_v<U, double>, arrow::DoubleBuilder, void>>>>>>>>>>;

                BuilderT builder;
                ARROW_RETURN_NOT_OK(builder.AppendValues(data.data(),
                                                         static_cast<int64_t>(data.size())));
                std::shared_ptr<arrow::Array> arr;
                ARROW_RETURN_NOT_OK(builder.Finish(&arr));
                return arr;
            }

            else
                return arrow::Status::Invalid("Unsupported type for vector_as_column()");
        }

    template <typename Name, typename T>
        std::pair<std::string, const std::vector<T>&> col(Name &&name, const std::vector<T> &v) {
            return { std::string(std::forward<Name>(name)), v };
        }

    template <typename Name, typename... T>
        std::shared_ptr<arrow::Schema> make_schema(const std::pair<Name, const std::vector<T>&> &...cols) {
            std::vector<std::shared_ptr<arrow::Field>> fields;
            fields.reserve(sizeof...(T));
            (fields.emplace_back(
                    arrow::field(std::string(cols.first), arrow_type<T>())
            ), ...);
            return arrow::schema(std::move(fields));
        }

    template <typename Name, typename... T>
        arrow::Result<std::vector<std::shared_ptr<arrow::Array>>>
                make_columns(const std::pair<Name, const std::vector<T>&> &...cols) {
            std::vector<std::shared_ptr<arrow::Array>> arrays;
            arrays.reserve(sizeof...(T));

            auto push_array = [&](const auto &named_vec) {
                ARROW_ASSIGN_OR_RAISE(auto arr, vector_as_column(named_vec.second));
                arrays.emplace_back(std::move(arr));
                return arrow::Status::OK();
            };

            arrow::Status status = arrow::Status::OK();
            ((status = status.ok() ? push_array(cols) : status), ...);
            if (!status.ok())
                return status;

            return arrays;
        }

    template <typename Name, typename... T>
        arrow::Result<std::shared_ptr<arrow::Table>> make_table(const std::pair<Name, const std::vector<T>&> &...cols) {
            const auto nrows = static_cast<int64_t>(std::get<1>(std::tie(cols...)).second.size());
            if (!((static_cast<int64_t>(cols.second.size()) == nrows) && ...))
                return arrow::Status::Invalid("All columns must have the same length");

            auto schema = make_schema(cols...);
            ARROW_ASSIGN_OR_RAISE(auto arrays, make_columns(cols...))

            return arrow::Table::Make(std::move(schema), std::move(arrays), nrows);
        }

    arrow::Status write_table_to_parquet(const std::shared_ptr<arrow::Table>& table,
                                         const std::string& filename,
                                         parquet::Compression::type compression = parquet::Compression::SNAPPY,
                                         int64_t chunk_size = 64 * 1024);
}
