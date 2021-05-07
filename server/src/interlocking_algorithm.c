/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#include <stdbool.h>

#include "interlocking_algorithm.h"
#include "interlocking.h"

bool route_is_unavailable_or_conflicted(const int route_id);
bool route_is_clear(const int route_id, const char *train_id);
bool set_route_points_signals(const int route_id);
bool block_route(const int route_id, const char *train_id);

void interlocking_algorithm_reset(t_interlocking_algorithm_tick_data* d) {
  d->_GO = 1;
  d->_TERM = 0;
  d->_pg8 = 0;
  d->_pg11 = 0;
  d->_pg12 = 0;
  d->_pg20 = 0;
  d->_pg16 = 0;
}

void interlocking_algorithm_logic(t_interlocking_algorithm_tick_data* d) {
  if (d->_GO) {
    d->route_id = -1;
    d->not_grantable = 1;
    d->is_clear = 0;
    d->block_status = 0;
    d->set_status = 0;
  }
  d->_g10 = d->_pg8;
  if (d->_g10) {
    d->route_id = -1;
  }
  d->_g24 = d->_pg11;
  d->_cg24 = d->request_available;
  d->_g25 = d->_g24 && !d->_cg24;
  d->_cg25 = !d->request_available;
  d->_g10 = d->_GO || d->_g10 || (d->_g25 && d->_cg25);
  if (d->_g10) {
    d->route_id = -1;
  }
  d->_cg1 = d->request_available;
  d->_g24 = (d->_g10 && d->_cg1) || (d->_g24 && d->_cg24);
  if (d->_g24) {
    d->route_id = interlocking_table_get_route_id(d->source_id, d->destination_id);
  }
  d->_g22 = d->_pg12;
  d->_g22 = d->_g24 || d->_g22;
  d->_cg3 = d->route_id != -1;
  d->_g2 = d->_g22 && d->_cg3;
  if (d->_g2) {
    d->not_grantable = route_is_unavailable_or_conflicted(d->route_id);
  }
  d->_g18 = d->_pg20;
  d->_g4 = d->_g2 || d->_g18;
  d->_cg5 = !d->not_grantable;
  d->_g18 = d->_g4 && d->_cg5;
  if (d->_g18) {
    d->is_clear = route_is_clear(d->route_id, d->train_id);
  }
  d->_g14 = d->_pg16;
  d->_g6 = d->_g18 || d->_g14;
  d->_cg7 = d->is_clear;
  d->_g14 = d->_g6 && d->_cg7;
  if (d->_g14) {
    d->block_status = block_route(d->route_id, d->train_id);
    d->set_status = set_route_points_signals(d->route_id);
  }
  d->_g7 = d->_g6 && !d->_cg7;
  d->_cg11 = !d->is_clear;
  d->_g12 = d->_g7 && d->_cg11;
  if (d->_g12) {
    d->route_id = -4;
  }
  d->_g5 = d->_g4 && !d->_cg5;
  d->_cg15 = d->not_grantable;
  d->_g16 = d->_g5 && d->_cg15;
  if (d->_g16) {
    d->route_id = -3;
  }
  d->_g3 = d->_g22 && !d->_cg3;
  d->_cg19 = d->route_id == -1;
  d->_g20 = d->_g3 && d->_cg19;
  if (d->_g20) {
    d->route_id = -2;
  }
  d->_g8 = d->_g14 || d->_g12 || d->_g16 || d->_g20;
  d->_g16 = d->_g7 && !d->_cg11;
  d->_g20 = d->_g5 && !d->_cg15;
  d->_g12 = d->_g3 && !d->_cg19;
  d->_g11 = (d->_g10 && !d->_cg1) || (d->_g25 && !d->_cg25);
}

void interlocking_algorithm_tick(t_interlocking_algorithm_tick_data* d) {
  interlocking_algorithm_logic(d);

  d->_pg8 = d->_g8;
  d->_pg11 = d->_g11;
  d->_pg12 = d->_g12;
  d->_pg20 = d->_g20;
  d->_pg16 = d->_g16;
  d->_GO = 0;
}
