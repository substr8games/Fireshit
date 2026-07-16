#include "frametap/worker/cli.hpp"
#include "frametap/worker/config.hpp"
#include "frametap/worker/plugin_client.hpp"
#include "frametap/worker/session.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

#ifndef FRAMETAP_VERSION
#define FRAMETAP_VERSION "unknown"
#endif

namespace
{
bool single_command(int argc, char** argv, std::string_view command)
{
    return argc == 2 && std::string_view(argv[1]) == command;
}
}

int main(int argc, char** argv)
{
    try
    {
        if (single_command(argc, argv, "--config-path"))
        {
            std::cout << frametap::worker::user_config_path() << '\n';
            return 0;
        }
        if (single_command(argc, argv, "--write-default-config"))
        {
            const auto path = frametap::worker::user_config_path();
            const bool existed = std::filesystem::exists(path);
            frametap::worker::write_default_config();
            std::cout << "FrameTap config: " << path;
            if (existed) std::cout << " (already exists; unchanged)";
            std::cout << '\n';
            return 0;
        }
        if (single_command(argc, argv, "--show-config"))
        {
            const auto config = frametap::worker::load_config();
            std::cout << "# path: " << config.file_path << '\n'
                      << "# source: " << (config.loaded_from_file ?
                        "user file" : "built-in defaults; file not written")
                      << "\n\n" << frametap::worker::format_config(config);
            return 0;
        }

        auto options = frametap::worker::parse_cli(argc, argv);
        if (options.help)
        {
            std::cout << frametap::worker::help_text();
            return 0;
        }
        if (options.version)
        {
            std::cout << "FrameTap " FRAMETAP_VERSION
                      << " protocol 5 Wayfire 0.10.x\n";
            return 0;
        }
        if (options.list)
        {
            frametap::worker::PluginClient client;
            for (const auto& view : client.list_views())
            {
                std::cout << view.view_id << '\t' << view.pid << '\t'
                          << view.width << 'x' << view.height << '\t'
                          << (view.focused ? "focused\t" : "\t")
                          << view.app_id << '\t' << view.title << '\n';
            }
            return 0;
        }
        if (options.list_outputs)
        {
            frametap::worker::PluginClient client;
            for (const auto& output : client.list_outputs())
            {
                std::cout << output.name << '\t' << output.x << ',' << output.y
                          << '\t' << output.width << 'x' << output.height
                          << '\t' << (static_cast<double>(output.scale_milli) / 1000.0)
                          << '\t' << (output.cursor ? "cursor" : "") << '\n';
            }
            return 0;
        }

        const auto config = frametap::worker::load_config();
        frametap::worker::apply_config(options, config);
        frametap::worker::ensure_default_config_exists();
        return frametap::worker::run_session(options);
    } catch (const std::exception& error)
    {
        std::cerr << "FrameTap: " << error.what() << '\n';
        return 3;
    }
}
