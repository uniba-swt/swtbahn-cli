/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#ifndef INTERLOCKING_ALGORITHM_H
#define INTERLOCKING_ALGORITHM_H

//#include "handler_controller.h"

typedef struct {
  const char* train_id;
  const char* source_id;
  const char* destination_id;
  int route_id;
  char not_grantable;
  char is_clear;
  char block_status;
  char set_status;
  char request_available;
  char _g2;
  char _g3;
  char _g4;
  char _g5;
  char _g6;
  char _g7;
  char _g8;
  char _g10;
  char _g11;
  char _g12;
  char _g14;
  char _g16;
  char _g18;
  char _g20;
  char _g22;
  char _g24;
  char _g25;
  char _GO;
  char _cg25;
  char _cg1;
  char _cg24;
  char _cg3;
  char _cg5;
  char _cg7;
  char _cg11;
  char _cg15;
  char _cg19;
  char _TERM;
  char _pg8;
  char _pg11;
  char _pg12;
  char _pg20;
  char _pg16;
} t_interlocking_algorithm_tick_data;

void interlocking_algorithm_reset(t_interlocking_algorithm_tick_data* d);
void interlocking_algorithm_logic(t_interlocking_algorithm_tick_data* d);
void interlocking_algorithm_tick(t_interlocking_algorithm_tick_data* d);

#endif // INTERLOCKING_ALGORITHM_H
