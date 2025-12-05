#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Log
{
    inline void init()
    {
        // Create file logger (you can add console logger too)
        auto file_logger = spdlog::basic_logger_mt("file_logger", "log.log");

        // Make file logger the default
        spdlog::set_default_logger(file_logger);

        // Set global log pattern ONCE
        spdlog::set_pattern("[%d:%m:%Y:%H:%M:%S.%e] [%l] %v");

        // Level and flushing setup
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::info);
    }
}
