/**
 * Generated by BahnDSL 1.0.0
 */

#include "tick_wrapper.h"
#include "check_route_sectional.h"

check_route_sectional_TickData intern_request = {};

void check_route_sectional_sync_input(check_route_sectional_tick_data *tick_data) {
    intern_request.iface._src_signal_id = tick_data->src_signal_id;
    intern_request.iface._dst_signal_id = tick_data->dst_signal_id;
    intern_request.iface._train_id = tick_data->train_id;
}

void check_route_sectional_sync_output(check_route_sectional_tick_data *tick_data) {
    tick_data->out = intern_request.iface.__out;
    tick_data->terminated = intern_request.Request_route_regionR0.threadStatus == TERMINATED ? 1 : 0;
}

void check_route_sectional_reset(check_route_sectional_tick_data *tick_data) {

    // clear the variables
    intern_request.iface = (Request_route_Iface) {};

    // sync
    check_route_sectional_sync_input(tick_data);
    intern_check_route_sectional_reset(&intern_request);
    check_route_sectional_sync_output(tick_data);
}

void check_route_sectional_tick(check_route_sectional_tick_data *tick_data) {
    check_route_sectional_sync_input(tick_data);
    intern_check_route_sectional_tick(&intern_request);
    check_route_sectional_sync_output(tick_data);
}