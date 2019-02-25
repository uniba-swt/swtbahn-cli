/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#ifndef TRAIN_ENGINE_DEFAULT_H
#define TRAIN_ENGINE_DEFAULT_H

typedef struct {
  int requested_speed;
  char requested_forwards;
  int target_speed;
  char target_forwards;
  char new_request;
  char _reg_requested_forwards;
  char _pre_requested_forwards;
  int _reg_requested_speed;
  int _pre_requested_speed;
  char _g4;
  char _g5;
  char _g8;
  char _g10;
  char _g11;
  char _g12;
  char _g13;
  char _g14;
  char _g15;
  char _g17;
  char _g20;
  char _g21;
  char _g22;
  char _g23;
  char _g24;
  char _g27;
  char _g30;
  char _g31;
  char _g33;
  char _g38;
  char _g42;
  char _g46;
  char _GO;
  char _cg4;
  char _cg10;
  char _cg17;
  char _cg20;
  char _cg27;
  char _cg33;
  char _cg8;
  char _cg15;
  char _cg11;
  char _cg13;
  char _cg18;
  char _cg23;
  char _cg21;
  char _cg25;
  char _cg30;
  char _cg28;
  char _TERM;
  char _pg46;
  char _pg38;
  char _pg42;
  char _pg5;
  char _pg31;
  char _pg12;
  char _pg15;
  char _pg21;
  char _pg22;
  
  char _g3;
  char _g7;
  char _pg3;
  char _pg7;
} t_tick_data;

void train_engine_default_reset(t_tick_data* d);
void train_engine_default_logic(t_tick_data* d);
void train_engine_default_tick(t_tick_data* d);

#endif // TRAIN_ENGINE_DEFAULT_H
