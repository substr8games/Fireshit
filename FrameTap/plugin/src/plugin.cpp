#include <cstdio>
#include <exception>
#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include "frametap/plugin/server.hpp"

class frametap_plugin final : public wf::plugin_interface_t
{
  public:
    void init() noexcept override
    {
        try
        {
            server.start(wf::get_core().ev_loop);
        } catch (const std::exception& error)
        {
            std::fprintf(stderr, "FrameTap plugin disabled: %s\n", error.what());
            server.stop();
        } catch (...)
        {
            std::fprintf(stderr,
              "FrameTap plugin disabled: unknown initialization failure\n");
            server.stop();
        }
    }

    void fini() noexcept override
    {
        server.stop();
    }

  private:
    frametap::plugin::Server server;
};

DECLARE_WAYFIRE_PLUGIN(frametap_plugin);
