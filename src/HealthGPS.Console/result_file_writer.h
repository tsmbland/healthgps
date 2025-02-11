#pragma once
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "model_info.h"
#include "result_writer.h"

namespace host {
/// @brief Defines the results message file stream writer class
///
/// The message content are written to a JSON (JavaScript Object Notation) file,
/// experiment information and global averages values, while `time-series` data
/// is written to a associated CSV (Comma-separated Values) file with same name
/// but different extension.
class ResultFileWriter final : public ResultWriter {
  public:
    ResultFileWriter() = delete;
    /// @brief Initialises an instance of the host::ResultFileWriter class.
    /// @param file_name The JSON output file full name
    /// @param info The associated experiment information
    ResultFileWriter(const std::filesystem::path &file_name, ExperimentInfo info);

    ResultFileWriter(const ResultFileWriter &) = delete;
    ResultFileWriter &operator=(const ResultFileWriter &) = delete;
    ResultFileWriter(ResultFileWriter &&other) noexcept;
    ResultFileWriter &operator=(ResultFileWriter &&other) noexcept;

    /// @brief Destroys a host::ResultFileWriter instance
    ~ResultFileWriter();
    void write(const hgps::ResultEventMessage &message) override;

  private:
    std::ofstream stream_;
    std::ofstream csvstream_;
    std::mutex lock_mutex_;
    std::atomic<bool> first_row_{true};
    ExperimentInfo info_;

    void write_json_begin(const std::filesystem::path &output);
    void write_json_end();

    static std::string to_json_string(const hgps::ResultEventMessage &message);
    void write_csv_channels(const hgps::ResultEventMessage &message);
    void write_csv_header(const hgps::ResultEventMessage &message);
};
} // namespace host
