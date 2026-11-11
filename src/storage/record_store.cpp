#include "minidb/storage/record_store.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>

namespace minidb::storage {
namespace {

constexpr std::string_view records_signature = "MINIDB_RECORDS";
constexpr std::string_view records_version = "2";
constexpr std::string_view legacy_records_version = "1";
constexpr std::string_view records_file_name = "records.dat";

bool parse_size(std::string_view text, std::size_t& value) {
    if (text.empty()) {
        return false;
    }
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_int64(std::string_view text, std::int64_t& value) {
    if (text.empty()) {
        return false;
    }
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_double(std::string_view text, double& value) {
    if (text.empty()) {
        return false;
    }
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), value,
                        std::chars_format::general);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
           std::isfinite(value);
}

bool read_line(std::istream& input, std::string& line) {
    return static_cast<bool>(std::getline(input, line));
}

bool parse_row_header(const std::string& line, std::size_t& count) {
    constexpr std::string_view prefix = "ROW ";
    return line.compare(0, prefix.size(), prefix) == 0 &&
           parse_size(std::string_view(line).substr(prefix.size()), count);
}

bool parse_value_header(const std::string& line, char& tag,
                        std::size_t& length) {
    if (line.size() < 3 || line[1] != ' ') {
        return false;
    }
    tag = line[0];
    return parse_size(std::string_view(line).substr(2), length);
}

bool read_record_value(std::istream& input, RecordValue& value,
                       std::string_view version, std::uintmax_t file_size) {
    std::string header;
    if (!read_line(input, header)) {
        return false;
    }
    char tag = '\0';
    std::size_t length = 0;
    if (!parse_value_header(header, tag, length) ||
        length > static_cast<std::size_t>(
                     (std::numeric_limits<std::streamsize>::max)())) {
        return false;
    }

    const auto payload_position = input.tellg();
    if (payload_position < 0 ||
        static_cast<std::uintmax_t>(payload_position) > file_size) {
        return false;
    }
    const auto remaining =
        file_size - static_cast<std::uintmax_t>(payload_position);
    std::string payload;
    if (length > payload.max_size() || length >= remaining) {
        return false;
    }
    payload.resize(length);
    if (length != 0 &&
        !input.read(payload.data(), static_cast<std::streamsize>(length))) {
        return false;
    }
    if (input.get() != '\n') {
        return false;
    }

    switch (tag) {
        case 'N':
            if (length != 0) {
                return false;
            }
            value = std::monostate{};
            return true;
        case 'I': {
            std::int64_t parsed = 0;
            if (!parse_int64(payload, parsed)) {
                return false;
            }
            value = parsed;
            return true;
        }
        case 'F': {
            if (version == legacy_records_version) {
                double parsed = 0;
                if (!parse_double(payload, parsed)) {
                    return false;
                }
                value = parsed;
            } else {
                float parsed = 0;
                if (payload.empty()) {
                    return false;
                }
                const auto converted = std::from_chars(
                    payload.data(), payload.data() + payload.size(), parsed,
                    std::chars_format::general);
                if (converted.ec != std::errc{} ||
                    converted.ptr != payload.data() + payload.size() ||
                    !std::isfinite(parsed)) {
                    return false;
                }
                value = parsed;
            }
            return true;
        }
        case 'D': {
            double parsed = 0;
            if (!parse_double(payload, parsed)) {
                return false;
            }
            value = parsed;
            return true;
        }
        case 'S':
            value = std::move(payload);
            return true;
        case 'B':
            if (payload == "0") {
                value = false;
                return true;
            }
            if (payload == "1") {
                value = true;
                return true;
            }
            return false;
        default:
            return false;
    }
}

bool write_record_value(std::ostream& output, const RecordValue& value) {
    char tag = '\0';
    std::string payload;
    if (std::holds_alternative<std::monostate>(value)) {
        tag = 'N';
    } else if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        tag = 'I';
        payload = std::to_string(*integer);
    } else if (const auto* float_value = std::get_if<float>(&value)) {
        tag = 'F';
        char buffer[64];
        const auto converted =
            std::to_chars(buffer, buffer + sizeof(buffer), *float_value,
                          std::chars_format::general,
                          std::numeric_limits<float>::max_digits10);
        if (converted.ec != std::errc{}) {
            return false;
        }
        payload.assign(buffer, converted.ptr);
    } else if (const auto* real = std::get_if<double>(&value)) {
        tag = 'D';
        char buffer[128];
        const auto converted =
            std::to_chars(buffer, buffer + sizeof(buffer), *real,
                          std::chars_format::general,
                          std::numeric_limits<double>::max_digits10);
        if (converted.ec != std::errc{}) {
            return false;
        }
        payload.assign(buffer, converted.ptr);
    } else if (const auto* text = std::get_if<std::string>(&value)) {
        tag = 'S';
        payload = *text;
    } else if (const auto* boolean = std::get_if<bool>(&value)) {
        tag = 'B';
        payload = *boolean ? "1" : "0";
    } else {
        return false;
    }

    output << tag << ' ' << payload.size() << '\n';
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.put('\n');
    return static_cast<bool>(output);
}

bool value_matches_column(const RecordValue& value, ColumnType type) {
    if (std::holds_alternative<std::monostate>(value)) {
        return true;
    }
    switch (type) {
        case ColumnType::Int:
            return std::holds_alternative<std::int64_t>(value);
        case ColumnType::Float:
            return std::holds_alternative<float>(value);
        case ColumnType::Double:
            return std::holds_alternative<double>(value);
        case ColumnType::Varchar:
        case ColumnType::Text:
            return std::holds_alternative<std::string>(value);
        case ColumnType::Boolean:
            return std::holds_alternative<bool>(value);
    }
    return false;
}

bool ensure_regular_file_or_missing(const std::filesystem::path& path,
                                    bool& exists, std::string& message) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        exists = false;
        return true;
    }
    if (error) {
        message = "Cannot inspect record file: " + error.message();
        return false;
    }
    if (status.type() == std::filesystem::file_type::not_found) {
        exists = false;
        return true;
    }
    if (std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status)) {
        message = "Record data must be a regular, non-symlink file.";
        return false;
    }
    exists = true;
    return true;
}

bool recover_record_files(const std::filesystem::path& path,
                          std::string& message) {
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".bak";

    bool data_exists = false;
    bool temporary_exists = false;
    bool backup_exists = false;
    if (!ensure_regular_file_or_missing(path, data_exists, message) ||
        !ensure_regular_file_or_missing(temporary, temporary_exists, message) ||
        !ensure_regular_file_or_missing(backup, backup_exists, message)) {
        return false;
    }

    std::error_code error;
    if (backup_exists) {
        if (!data_exists) {
            std::filesystem::rename(backup, path, error);
            if (error) {
                message = "Cannot recover record data: " + error.message();
                return false;
            }
            data_exists = true;
        } else {
            std::filesystem::remove(backup, error);
            if (error) {
                message = "Cannot remove stale record backup: " +
                          error.message();
                return false;
            }
        }
    }
    if (temporary_exists) {
        error.clear();
        std::filesystem::remove(temporary, error);
        if (error) {
            message = "Cannot remove incomplete record file: " +
                      error.message();
            return false;
        }
    }
    return true;
}

bool write_records_file(const std::filesystem::path& path,
                        const std::vector<Record>& rows,
                        std::string& message) {
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".bak";

    bool data_exists = false;
    bool temporary_exists = false;
    bool backup_exists = false;
    if (!ensure_regular_file_or_missing(path, data_exists, message) ||
        !ensure_regular_file_or_missing(temporary, temporary_exists, message) ||
        !ensure_regular_file_or_missing(backup, backup_exists, message)) {
        return false;
    }
    if (backup_exists || temporary_exists) {
        message = "Record data has an unfinished previous update; reload the "
                  "table before writing.";
        return false;
    }

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            message = "Cannot create record data file.";
            return false;
        }
        output << records_signature << '\n'
               << records_version << '\n'
               << rows.size() << '\n';
        for (const auto& row : rows) {
            output << "ROW " << row.size() << '\n';
            for (const auto& value : row) {
                if (!write_record_value(output, value)) {
                    output.close();
                    std::error_code cleanup_error;
                    std::filesystem::remove(temporary, cleanup_error);
                    message = "Cannot write record data.";
                    if (cleanup_error) {
                        message += " Cleanup failed: " +
                                   cleanup_error.message();
                    }
                    return false;
                }
            }
        }
        output.flush();
        if (!output) {
            output.close();
            std::error_code cleanup_error;
            std::filesystem::remove(temporary, cleanup_error);
            message = "Cannot write record data.";
            if (cleanup_error) {
                message += " Cleanup failed: " + cleanup_error.message();
            }
            return false;
        }
        output.close();
        if (!output) {
            message = "Cannot finalize record data.";
            return false;
        }
    }

    std::error_code error;
    if (data_exists) {
        std::filesystem::rename(path, backup, error);
        if (error) {
            message = "Cannot prepare record data update: " + error.message();
            return false;
        }
    }

    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) {
        const std::string rename_message = error.message();
        if (data_exists) {
            std::error_code restore_error;
            std::filesystem::rename(backup, path, restore_error);
            message = "Cannot finalize record data update: " + rename_message;
            if (restore_error) {
                message += ". Restore failed: " + restore_error.message();
            }
        } else {
            message = "Cannot finalize record data update: " + rename_message;
        }
        return false;
    }

    if (data_exists) {
        error.clear();
        std::filesystem::remove(backup, error);
        if (error) {
            message = "Records were saved, but the old record backup could "
                      "not be removed: " + error.message();
            return false;
        }
    }
    return true;
}

}  // namespace

RecordStore::RecordStore(const DatabaseManager& database_manager)
    : database_manager_(database_manager), table_manager_(database_manager) {}

RecordsResult RecordStore::read(std::string_view table) const try {
    const auto schema_result = table_manager_.describe_table(table);
    if (!schema_result.success) {
        return {false, {}, schema_result.message};
    }
    const auto database_path = database_manager_.selected_database_path();
    if (!database_path) {
        return {false, {}, "No database is selected."};
    }

    const auto path = *database_path / "tables" / schema_result.schema.name /
                      records_file_name;
    std::string message;
    if (!recover_record_files(path, message)) {
        return {false, {}, std::move(message)};
    }

    bool exists = false;
    if (!ensure_regular_file_or_missing(path, exists, message)) {
        return {false, {}, std::move(message)};
    }
    if (!exists) {
        return {true, {}, {}};
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {false, {}, "Cannot open record data file."};
    }
    std::error_code file_size_error;
    const auto file_size = std::filesystem::file_size(path, file_size_error);
    if (file_size_error) {
        return {false, {},
                "Cannot inspect record data size: " +
                    file_size_error.message()};
    }

    std::string signature;
    std::string version;
    std::string count_line;
    std::size_t row_count = 0;
    if (!read_line(input, signature) || !read_line(input, version) ||
        !read_line(input, count_line) || signature != records_signature ||
        (version != records_version && version != legacy_records_version) ||
        !parse_size(count_line, row_count)) {
        return {false, {}, "Record data has an unsupported or invalid format."};
    }

    const auto rows_position = input.tellg();
    if (rows_position < 0 ||
        static_cast<std::uintmax_t>(rows_position) > file_size) {
        return {false, {}, "Record data has an invalid row count."};
    }
    const std::uintmax_t remaining =
        file_size - static_cast<std::uintmax_t>(rows_position);
    constexpr std::uintmax_t row_header_minimum = 6;
    constexpr std::uintmax_t value_minimum = 5;
    const auto column_count = schema_result.schema.columns.size();
    if (column_count >
        ((std::numeric_limits<std::uintmax_t>::max)() -
         row_header_minimum) /
            value_minimum) {
        return {false, {}, "Table schema is too large to read record data."};
    }
    const std::uintmax_t minimum_row_size =
        row_header_minimum + column_count * value_minimum;
    if (minimum_row_size == 0 || row_count > remaining / minimum_row_size) {
        return {false, {},
                "Record data has an invalid or impossible row count."};
    }

    std::vector<Record> rows;
    for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
        std::string row_header;
        std::size_t value_count = 0;
        if (!read_line(input, row_header) ||
            !parse_row_header(row_header, value_count) ||
            value_count != schema_result.schema.columns.size()) {
            return {false, {},
                    "Record data contains an invalid row header or column "
                    "count."};
        }
        Record row;
        row.reserve(value_count);
        for (std::size_t value_index = 0; value_index < value_count;
             ++value_index) {
            RecordValue value;
            if (!read_record_value(input, value, version, file_size)) {
                return {false, {},
                        "Record data contains an invalid or incomplete value."};
            }
            if (!value_matches_column(
                    value, schema_result.schema.columns[value_index].type)) {
                if (version == legacy_records_version &&
                    schema_result.schema.columns[value_index].type ==
                        ColumnType::Float) {
                    if (const auto* legacy_real =
                            std::get_if<double>(&value)) {
                        if (std::abs(*legacy_real) <=
                            static_cast<double>(
                                (std::numeric_limits<float>::max)())) {
                            value = static_cast<float>(*legacy_real);
                        }
                    }
                }
            }
            if (!value_matches_column(
                    value, schema_result.schema.columns[value_index].type)) {
                return {false, {},
                        "Record data contains a value incompatible with the "
                        "table schema."};
            }
            row.push_back(std::move(value));
        }
        rows.push_back(std::move(row));
    }

    char trailing = '\0';
    if (input.get(trailing)) {
        return {false, {}, "Record data contains unexpected trailing bytes."};
    }
    if (!input.eof()) {
        return {false, {}, "Cannot finish reading record data."};
    }
    return {true, std::move(rows), {}};
} catch (const std::bad_alloc&) {
    return {false, {}, "Record data exceeds available memory."};
} catch (const std::length_error&) {
    return {false, {}, "Record data exceeds supported size limits."};
}

RecordOperationResult RecordStore::write(
    std::string_view table, const std::vector<Record>& rows) const {
    const auto schema_result = table_manager_.describe_table(table);
    if (!schema_result.success) {
        return {false, schema_result.message};
    }
    const auto database_path = database_manager_.selected_database_path();
    if (!database_path) {
        return {false, "No database is selected."};
    }
    for (const auto& row : rows) {
        if (row.size() != schema_result.schema.columns.size()) {
            return {false, "Record column count does not match the table "
                           "schema."};
        }
        for (std::size_t index = 0; index < row.size(); ++index) {
            if (!value_matches_column(
                    row[index], schema_result.schema.columns[index].type)) {
                return {false,
                        "Record value type does not match the table schema."};
            }
        }
    }

    const auto path = *database_path / "tables" / schema_result.schema.name /
                      records_file_name;
    std::string message;
    if (!recover_record_files(path, message)) {
        return {false, std::move(message)};
    }
    if (!write_records_file(path, rows, message)) {
        return {false, std::move(message)};
    }
    return {true, {}};
}

}  // namespace minidb::storage
