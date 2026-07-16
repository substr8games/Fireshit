#include <cmath>
#include <iomanip>
#include <sstream>

#include <wayfire/bindings-repository.hpp>
#include <wayfire/core.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>

class firemod_wayfire_plugin final : public wf::plugin_interface_t
{
    wf::option_wrapper_t<wf::activatorbinding_t> menu_binding{
        "firemod-wayfire/menu"};
    wf::option_wrapper_t<bool> enabled{"firemod-wayfire/enabled"};
    wf::option_wrapper_t<bool> autostart{"firemod-wayfire/autostart"};
    wf::activator_callback menu_callback;

    static void start_core()
    {
        wf::get_core().run("firemod --background >/dev/null 2>&1");
    }

  public:
    void init() override
    {
        menu_callback = [this] (const wf::activator_data_t&)
        {
            if (!(bool)enabled)
            {
                return false;
            }

            const auto position = wf::get_core().get_cursor_position();
            if (!std::isfinite(position.x) || !std::isfinite(position.y))
            {
                return false;
            }

            std::ostringstream command;
            command << std::fixed << std::setprecision(3)
                    << "firemodctl menu " << position.x << ' ' << position.y
                    << " >/dev/null 2>&1 &";
            wf::get_core().run(command.str());
            return true;
        };

        wf::get_core().bindings->add_activator(
            menu_binding, &menu_callback);

        autostart.set_callback([this] ()
        {
            if ((bool)enabled && (bool)autostart)
            {
                start_core();
            }
        });

        enabled.set_callback([this] ()
        {
            if ((bool)enabled && (bool)autostart)
            {
                start_core();
            }
        });

        if ((bool)enabled && (bool)autostart)
        {
            start_core();
        }
    }

    void fini() override
    {
        wf::get_core().bindings->rem_binding(&menu_callback);
    }
};

DECLARE_WAYFIRE_PLUGIN(firemod_wayfire_plugin);
