/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#include "train_engine_linear.h"

void train_engine_linear_reset(t_train_engine_tick_data* d) {
  d->_GO = 1;
  d->_TERM = 0;
  d->_pg46 = 0;
  d->_pg38 = 0;
  d->_pg42 = 0;
  d->_pg5 = 0;
  d->_pg31 = 0;
  d->_pg12 = 0;
  d->_pg15 = 0;
  d->_pg21 = 0;
  d->_pg22 = 0;
}

void train_engine_linear_logic(t_train_engine_tick_data* d) {
  if (d->_GO) {
    d->requested_speed = 0;
    d->requested_forwards = 1;
    d->target_speed = 0;
    d->target_forwards = 1;
    d->_reg_requested_forwards = 0;
    d->_reg_requested_speed = 0;
  }
  d->_g46 = d->_pg46;
  d->_g46 = d->_GO || d->_g46;
  if (d->_g46) {
    d->new_request = 0;
  }
  d->_g38 = d->_pg38;
  d->_g38 = d->_GO || d->_g38;
  if (d->_g38) {
    d->_pre_requested_forwards = d->_reg_requested_forwards;
    d->_reg_requested_forwards = d->requested_forwards;
  }
  d->_g42 = d->_pg42;
  d->_g42 = d->_GO || d->_g42;
  if (d->_g42) {
    d->_pre_requested_speed = d->_reg_requested_speed;
    d->_reg_requested_speed = d->requested_speed;
  }
  d->_g4 = d->_pg5;
  d->_cg4 = d->_pre_requested_speed != d->requested_speed || d->_pre_requested_forwards != d->requested_forwards;
  d->_g5 = d->_g4 && d->_cg4;
  if (d->_g5) {
    d->new_request |= 1;
  }
  d->_g5 = d->_GO || d->_g5 || d->_g4 && !d->_cg4;
  d->_g4 = d->_pg31;
  d->_cg10 = d->new_request;
  d->_g17 = d->_pg12;
  d->_cg17 = d->new_request;
  d->_g20 = d->_pg15;
  d->_cg20 = d->new_request;
  d->_g27 = d->_pg21;
  d->_cg27 = d->new_request;
  d->_g33 = d->_pg22;
  d->_cg33 = d->new_request;
  d->_g33 = d->_GO || d->_g4 && d->_cg10 || d->_g17 && d->_cg17 || d->_g20 && d->_cg20 || d->_g27 && d->_cg27 || d->_g33 && d->_cg33 || d->_g33 && d->_cg33;
  d->_cg8 = d->target_forwards == d->requested_forwards;
  d->_g10 = d->_g4 && !d->_cg10;
  d->_cg11 = d->target_speed < d->requested_speed;
  d->_g12 = d->_g10 && d->_cg11;
  if (d->_g12) {
    d->target_speed++;
  }
  d->_g11 = d->_g10 && !d->_cg11;
  d->_cg13 = d->target_speed > d->requested_speed;
  d->_g14 = d->_g11 && d->_cg13;
  if (d->_g14) {
    d->target_speed--;
  }
  d->_g13 = d->_g11 && !d->_cg13;
  d->_cg15 = d->target_speed == d->requested_speed;
  d->_g20 = d->_g20 && !d->_cg20;
  d->_cg21 = d->target_speed > 0;
  d->_g23 = d->_g20 && !d->_cg21;
  d->_cg23 = d->target_speed == 0;
  d->_g24 = d->_g23 && d->_cg23;
  if (d->_g24) {
    d->target_forwards = 1;
  }
  d->_g27 = d->_g27 && !d->_cg27;
  d->_cg28 = d->target_speed > 0;
  d->_g30 = d->_g27 && !d->_cg28;
  d->_cg30 = d->target_speed == 0;
  d->_g31 = d->_g30 && d->_cg30;
  if (d->_g31) {
    d->target_forwards = 0;
  }
  d->_g31 = d->_g33 && d->_cg8 || d->_g12 || d->_g14 || d->_g13 && !d->_cg15 || d->_g24 || d->_g31;
  d->_g12 = d->_g13 && d->_cg15 || d->_g17 && !d->_cg17;
  d->_g24 = d->_g33 && !d->_cg8;
  d->_cg18 = !d->target_forwards && d->requested_forwards;
  d->_g14 = d->_g20 && d->_cg21;
  if (d->_g14) {
    d->target_speed--;
  }
  d->_g15 = d->_g24 && d->_cg18 || d->_g14 || d->_g23 && !d->_cg23;
  d->_g17 = d->_g24 && !d->_cg18;
  d->_cg25 = d->target_forwards && !d->requested_forwards;
  d->_g8 = d->_g27 && d->_cg28;
  if (d->_g8) {
    d->target_speed--;
  }
  d->_g21 = d->_g17 && d->_cg25 || d->_g8 || d->_g30 && !d->_cg30;
  d->_g22 = d->_g17 && !d->_cg25;
}

void train_engine_linear_tick(t_train_engine_tick_data* d) {
  train_engine_linear_logic(d);

  d->_pg46 = d->_g46;
  d->_pg38 = d->_g38;
  d->_pg42 = d->_g42;
  d->_pg5 = d->_g5;
  d->_pg31 = d->_g31;
  d->_pg12 = d->_g12;
  d->_pg15 = d->_g15;
  d->_pg21 = d->_g21;
  d->_pg22 = d->_g22;
  d->_GO = 0;
}

