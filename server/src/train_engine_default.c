/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#include "train_engine_default.h"

void train_engine_defaultreset(t_train_engine_tick_data* d) {
  d->_GO = 1;
  d->_TERM = 0;
}

void train_engine_defaultlogic(t_train_engine_tick_data* d) {
  if (d->_GO) {
    d->target_speed = 0;
    d->target_forwards = 1;
  }
  d->_taken_transitions[0] = 0;
  d->_taken_transitions[1] = 0;
  if (d->_g3) {
    d->target_speed = d->requested_speed;
    d->_taken_transitions[0] += 1;
    d->target_forwards = d->requested_forwards;
    d->_taken_transitions[1] += 1;
  }
  d->_g3 = 1;
}

void train_engine_defaulttick(t_train_engine_tick_data* d) {
  train_engine_defaultlogic(d);

  d->_GO = 0;
}
