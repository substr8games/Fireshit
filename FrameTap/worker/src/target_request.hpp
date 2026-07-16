#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "frametap/worker/cli.hpp"
#include "frametap/protocol/packet.hpp"

namespace frametap::worker
{
class LaunchedApplication;
class PluginClient;

AudioMode effective_audio_mode(const CliOptions& options);
protocol::PrepareSession resolve_request(const CliOptions& options,
  PluginClient& client, std::unique_ptr<LaunchedApplication>& launched,
  const std::vector<std::pair<std::string, std::string>>& environment);
}
