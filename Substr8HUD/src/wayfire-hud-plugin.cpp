#include <string>

#include <wayfire/bindings-repository.hpp>
#include <wayfire/core.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>

class wayfire_hud_plugin : public wf::plugin_interface_t
{
    wf::option_wrapper_t<wf::activatorbinding_t> toggle{
        "wayfire-hud/toggle"};
    wf::option_wrapper_t<bool> enabled{
        "wayfire-hud/enabled"};
    wf::option_wrapper_t<bool> autostart{
        "wayfire-hud/autostart"};
    wf::option_wrapper_t<int> width_percent{
        "wayfire-hud/width_percent"};
    wf::option_wrapper_t<double> opacity{
        "wayfire-hud/opacity"};
    wf::option_wrapper_t<std::string> module_directory{
        "wayfire-hud/module_directory"};
    wf::option_wrapper_t<std::string> initial_state{
        "wayfire-hud/initial_state"};

    wf::activator_callback toggle_callback;

    static void start_hud()
    {
        wf::get_core().run(
            "wayfire-hudctl reload >/dev/null 2>&1 || "
            "wayfire-hud >/dev/null 2>&1 &");
    }

    static void stop_hud()
    {
        wf::get_core().run(
            "wayfire-hudctl quit >/dev/null 2>&1 || true");
    }

    static void reload_hud()
    {
        wf::get_core().run(
            "wayfire-hudctl reload >/dev/null 2>&1 || true");
    }

    static void restart_hud()
    {
        wf::get_core().run(
            "(wayfire-hudctl quit >/dev/null 2>&1 || true); "
            "sleep 0.15; "
            "wayfire-hud >/dev/null 2>&1 &");
    }

    void sync_enabled_state()
    {
        if ((bool)enabled)
        {
            start_hud();
        } else
        {
            stop_hud();
        }
    }

  public:
    void init() override
    {
        toggle_callback = [this] (const wf::activator_data_t&)
        {
            if (!(bool)enabled)
            {
                return false;
            }

            return wf::get_core().run(
                "wayfire-hudctl cycle >/dev/null 2>&1 || "
                "wayfire-hud >/dev/null 2>&1 &") >= 0;
        };

        wf::get_core().bindings->add_activator(
            toggle,
            &toggle_callback);

        enabled.set_callback([this] ()
        {
            sync_enabled_state();
        });

        autostart.set_callback([this] ()
        {
            if ((bool)enabled && (bool)autostart)
            {
                start_hud();
            }
        });

        width_percent.set_callback([] ()
        {
            reload_hud();
        });

        opacity.set_callback([] ()
        {
            reload_hud();
        });

        module_directory.set_callback([this] ()
        {
            if ((bool)enabled)
            {
                restart_hud();
            }
        });

        initial_state.set_callback([this] ()
        {
            if ((bool)enabled)
            {
                restart_hud();
            }
        });

        if ((bool)enabled && (bool)autostart)
        {
            start_hud();
        }
    }

    void fini() override
    {
        wf::get_core().bindings->rem_binding(
            &toggle_callback);
        stop_hud();
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_hud_plugin);
