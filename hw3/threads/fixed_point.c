#include <stdio.h>

//#define FIXED_POINT_MASK 0x4000
#define FIXED_POINT_MASK (1<<14)

int convert_to_fixed_point(int n) {
  return n * FIXED_POINT_MASK;
}

int convert_to_int_to_zero(int x) {
  return x / FIXED_POINT_MASK;
}

int convert_to_int_to_nearest(int x) {
  if(x >= 0) {
    x= x + FIXED_POINT_MASK / 2;
  } 
  else {
    x= x - FIXED_POINT_MASK / 2;
  }
  return x / FIXED_POINT_MASK;
}

int add_fixed_and_fixed(int x, int y) {
  return x + y;
}

int add_fixed_and_int(int x, int n) {
  return x + n * FIXED_POINT_MASK;
}

int substract_fixed_and_fixed(int x, int y) {
  return x - y;
}

int substract_fixed_and_int(int x, int n) {
  return x - n * FIXED_POINT_MASK;
}

int multiply_fixed_and_fixed(int x, int y) {
  return ((int64_t) x) * y / FIXED_POINT_MASK;
}

int multiply_fixed_and_int(int x, int n) {
  return x * n;
}
int divide_fixed_and_fixed(int x, int y) {
  return ((int64_t) x) * FIXED_POINT_MASK / y;
}

int divide_fixed_and_int(int x, int n) {
  return x / n;
}

