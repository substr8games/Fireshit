#include "frametap/protocol/codec.hpp"
#include <cassert>

int main()
{
    using namespace frametap::protocol;
    ViewRecord view;
    view.view_id = 42;
    view.width = 1280;
    view.height = 720;
    view.focused = true;
    view.mapped = true;
    view.pid = 1234;
    view.app_id = "one-wingedangel";
    view.title = "OneWingedAngel";
    auto packet = decode_packet(encode_packet(make_view_list(7, {view})));
    auto views = decode_view_list(packet);
    assert(packet.header.request_id == 7);
    assert(views.size() == 1);
    assert(views[0].view_id == 42);
    assert(views[0].pid == 1234);
    assert(views[0].app_id == "one-wingedangel");
    assert(views[0].focused);

    OutputRecord output;
    output.name = "DP-1";
    output.x = 1920;
    output.width = 2560;
    output.height = 1440;
    output.scale_milli = 1250;
    output.cursor = true;
    auto output_packet = decode_packet(
      encode_packet(make_output_list(8, {output})));
    auto outputs = decode_output_list(output_packet);
    assert(outputs.size() == 1);
    assert(outputs[0].name == "DP-1");
    assert(outputs[0].width == 2560);
    assert(outputs[0].scale_milli == 1250);
    assert(outputs[0].cursor);

    PrepareSession prepare;
    prepare.selector_type = SelectorType::region;
    prepare.selector = "DP-1";
    prepare.region_x = 120;
    prepare.region_y = 80;
    prepare.region_width = 1280;
    prepare.region_height = 720;
    auto decoded = decode_prepare_session(decode_packet(
      encode_packet(make_prepare_session(9, prepare))));
    assert(decoded.selector_type == SelectorType::region);
    assert(decoded.selector == prepare.selector);
    assert(decoded.region_x == 120);
    assert(decoded.region_width == 1280);
    assert(decoded.pool_size == 4);
    return 0;
}
