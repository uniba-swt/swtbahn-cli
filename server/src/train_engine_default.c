/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#include "train_engine_default.h"

void train_engine_default_reset(t_tick_data* d) {
  d->_GO = 1;
  d->_TERM = 0;
  d->_pg3 = 0;
  d->_pg7 = 0;
}

void train_engine_default_logic(t_tick_data* d) {
  if (d->_GO) {
    d->requested_speed = 0;
    d->requested_forwards = 1;
    d->target_speed = 0;
    d->target_forwards = 1;
  }
  d->_g3 = d->_pg3;
  if (d->_g3) {
    d->target_speed = d->requested_speed;
    d->target_forwards = d->target_forwards;
  }
  d->_g3 = d->_GO || d->_g3;
  d->_g7 = d->_pg7;
  d->_g7 = d->_GO || d->_g7;
  if (d->_g7) {
    d->new_request = 0;
  }
}

void train_engine_default_tick(t_tick_data* d) {
  train_engine_default_logic(d);

  d->_pg3 = d->_g3;
  d->_pg7 = d->_g7;
  d->_GO = 0;
}
