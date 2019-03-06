/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#include "interlocking_algorithm.h"

void interlocking_algorithm_reset(t_interlocking_algorithm_tick_data* d) {
  d->_GO = 1;
  d->_TERM = 0;
  d->_pg6 = 0;
  d->_pg11 = 0;
  d->_pg1 = 0;
  d->_pg18 = 0;
  d->_pg15 = 0;
  d->_pg10 = 0;
}

void interlocking_algorithm_logic(t_interlocking_algorithm_tick_data* d) {
  if (d->_GO) {
    d->route_id = -1;
    d->not_grantable = 1;
    d->is_clear = 0;
    d->block_status = 0;
    d->set_status = 0;
  }
  d->_g9 = d->_pg6;
  d->_g12 = d->_pg11;
  d->_g22 = d->_pg1;
  d->_cg22 = d->request_available;
  d->_g23 = d->_g22 && !d->_cg22;
  d->_cg23 = !d->request_available;
  d->_g9 = d->_GO || d->_g9 || d->_g12 || d->_g23 && d->_cg23;
  if (d->_g9) {
    d->route_id = -1;
  }
  d->_cg1 = d->request_available;
  d->_g12 = d->_g9 && d->_cg1 || d->_g22 && d->_cg22;
  if (d->_g12) {
    d->route_id = interlocking_table_get_route_id(d->source_id, d->destination_id);
  }
  d->_g22 = d->_pg18;
  d->_g20 = d->_g12 || d->_g22;
  d->_cg3 = d->route_id != -1;
  d->_g2 = d->_g20 && d->_cg3;
  if (d->_g2) {
    d->not_grantable = route_is_unavailable_or_conflicted(d->route_id);
  }
  d->_g17 = d->_pg15;
  d->_g4 = d->_g2 || d->_g17;
  d->_cg5 = !d->not_grantable;
  d->_g17 = d->_g4 && d->_cg5;
  if (d->_g17) {
    d->is_clear = route_is_clear(d->route_id, d->train_id);
  }
  d->_g14 = d->_pg10;
  d->_g14 = d->_g17 || d->_g14;
  d->_cg7 = d->is_clear;
  d->_g6 = d->_g14 && d->_cg7;
  if (d->_g6) {
    d->block_status = block_route(d->route_id, d->train_id);
    d->set_status = set_route_points_signals(d->route_id);
  }
  d->_g7 = d->_g14 && !d->_cg7;
  d->_cg10 = !d->is_clear;
  d->_g5 = d->_g4 && !d->_cg5;
  d->_cg15 = d->not_grantable;
  d->_g3 = d->_g20 && !d->_cg3;
  d->_cg18 = d->route_id == -1;
  d->_g11 = d->_g7 && d->_cg10 || d->_g5 && d->_cg15 || d->_g3 && d->_cg18;
  if (d->_g11) {
    d->route_id = -1;
  }
  d->_g10 = d->_g7 && !d->_cg10;
  d->_g15 = d->_g5 && !d->_cg15;
  d->_g18 = d->_g3 && !d->_cg18;
  d->_g1 = d->_g9 && !d->_cg1 || d->_g23 && !d->_cg23;
}

void interlocking_algorithm_tick(t_interlocking_algorithm_tick_data* d) {
  interlocking_algorithm_logic(d);

  d->_pg6 = d->_g6;
  d->_pg11 = d->_g11;
  d->_pg1 = d->_g1;
  d->_pg18 = d->_g18;
  d->_pg15 = d->_g15;
  d->_pg10 = d->_g10;
  d->_GO = 0;
}
