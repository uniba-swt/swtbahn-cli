/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#include "train_engine_linear.h"

void train_engine_linearreset(t_train_engine_tick_data* d) {
  d->_GO = 1;
  d->_TERM = 0;
  d->_pg5 = 0;
  d->_pg37 = 0;
  d->_pg38 = 0;
  d->_pg29 = 0;
  d->_pg18 = 0;
  d->_pg11 = 0;
}

void train_engine_linearlogic(t_train_engine_tick_data* d) {
  if (d->_GO) {
    d->target_speed = 0;
    d->target_forwards = 1;
    d->_reg_requested_forwards = 0;
    d->_reg_requested_speed = 0;
  }
  d->_taken_transitions[0] = 0;
  d->_taken_transitions[1] = 0;
  d->_taken_transitions[2] = 0;
  d->_taken_transitions[3] = 0;
  d->_taken_transitions[4] = 0;
  d->_taken_transitions[5] = 0;
  d->_taken_transitions[6] = 0;
  d->_taken_transitions[7] = 0;
  d->_taken_transitions[8] = 0;
  d->_taken_transitions[9] = 0;
  d->_taken_transitions[10] = 0;
  d->_taken_transitions[11] = 0;
  d->new_request = 0;
  d->_pre_requested_forwards = d->_reg_requested_forwards;
  d->_reg_requested_forwards = d->requested_forwards;
  d->_pre_requested_speed = d->_reg_requested_speed;
  d->_reg_requested_speed = d->requested_speed;
  d->_g4 = d->_pg5;
  d->_cg4 = d->_pre_requested_speed != d->requested_speed || d->_pre_requested_forwards != d->requested_forwards;
  d->_g5 = d->_g4 && d->_cg4;
  if (d->_g5) {
    d->new_request |= 1;
    d->_taken_transitions[0] += 1;
  }
  d->_g5 = d->_GO || d->_g5 || (d->_g4 && !d->_cg4);
  d->_g4 = d->_pg37;
  d->_cg13 = d->new_request;
  d->_g21 = d->_pg38;
  d->_cg21 = d->new_request;
  d->_g23 = d->_pg29;
  d->_cg23 = d->new_request;
  d->_g31 = d->_pg18;
  d->_cg31 = d->new_request;
  d->_g35 = d->_pg11;
  d->_cg35 = d->new_request;
  d->_g14 = (d->_g4 && d->_cg13) || (d->_g21 && d->_cg21) || (d->_g23 && d->_cg23) || (d->_g31 && d->_cg31) || (d->_g35 && d->_cg35);
  if (d->_g14) {
    d->_taken_transitions[1] += 1;
  }
  d->_g14 = d->_GO || d->_g14;
  d->_cg8 = !d->target_forwards && d->requested_forwards;
  d->_g35 = d->_g35 && !d->_cg35;
  d->_cg36 = !d->target_forwards && d->requested_forwards;
  d->_g9 = (d->_g14 && d->_cg8) || (d->_g35 && d->_cg36);
  if (d->_g9) {
    d->_taken_transitions[2] += 1;
  }
  d->_g23 = d->_g23 && !d->_cg23;
  d->_cg24 = d->target_speed > 0;
  d->_g25 = d->_g23 && d->_cg24;
  if (d->_g25) {
    d->target_speed -= 1;
    d->_taken_transitions[5] += 1;
  }
  d->_g9 = d->_g9 || d->_g25 || (d->_g23 && !d->_cg24);
  d->_cg10 = d->target_speed == 0;
  d->_g24 = d->_g9 && d->_cg10;
  if (d->_g24) {
    d->target_forwards = 1;
    d->_taken_transitions[6] += 1;
  }
  d->_g25 = d->_g4 && !d->_cg13;
  d->_cg15 = d->target_speed < d->requested_speed;
  d->_g13 = d->_g25 && d->_cg15;
  if (d->_g13) {
    d->target_speed += 1;
    d->_taken_transitions[9] += 1;
  }
  d->_g15 = d->_g25 && !d->_cg15;
  d->_cg17 = d->target_speed > d->requested_speed;
  d->_g18 = d->_g15 && d->_cg17;
  if (d->_g18) {
    d->target_speed -= 1;
    d->_taken_transitions[10] += 1;
  }
  d->_g8 = d->_g14 && !d->_cg8;
  d->_cg26 = d->target_forwards && !d->requested_forwards;
  d->_g36 = d->_g35 && !d->_cg36;
  d->_cg37 = d->target_forwards && !d->requested_forwards;
  d->_g27 = (d->_g8 && d->_cg26) || (d->_g36 && d->_cg37);
  if (d->_g27) {
    d->_taken_transitions[3] += 1;
  }
  d->_g31 = d->_g31 && !d->_cg31;
  d->_cg32 = d->target_speed > 0;
  d->_g33 = d->_g31 && d->_cg32;
  if (d->_g33) {
    d->target_speed -= 1;
    d->_taken_transitions[7] += 1;
  }
  d->_g33 = d->_g27 || d->_g33 || (d->_g31 && !d->_cg32);
  d->_cg28 = d->target_speed == 0;
  d->_g27 = d->_g33 && d->_cg28;
  if (d->_g27) {
    d->target_forwards = 0;
    d->_taken_transitions[8] += 1;
  }
  d->_g32 = d->_g36 && !d->_cg37;
  if (d->_g32) {
    d->_taken_transitions[4] += 1;
  }
  d->_g37 = d->_g24 || d->_g13 || d->_g18 || d->_g27 || d->_g32;
  d->_g16 = d->_g15 && !d->_cg17;
  if (d->_g16) {
    d->_taken_transitions[11] += 1;
  }
  d->_g38 = d->_g16 || (d->_g21 && !d->_cg21);
  d->_g29 = d->_g9 && !d->_cg10;
  d->_g18 = d->_g33 && !d->_cg28;
  d->_g11 = d->_g8 && !d->_cg26;
}

void train_engine_lineartick(t_train_engine_tick_data* d) {
  train_engine_linearlogic(d);

  d->_pg5 = d->_g5;
  d->_pg37 = d->_g37;
  d->_pg38 = d->_g38;
  d->_pg29 = d->_g29;
  d->_pg18 = d->_g18;
  d->_pg11 = d->_g11;
  d->_GO = 0;
}
