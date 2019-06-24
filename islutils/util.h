#ifndef UTIL_H
#define UTIL_H

#include <isl/cpp.h>
#include <islutils/scop.h>
#include <islutils/pet_wrapper.h>

isl::union_map computeAllDependences(const Scop &);

#endif
