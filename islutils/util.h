#ifndef UTIL_H
#define UTIL_H

#include <isl/isl-noexceptions.h>
#include <islutils/scop.h>
#include <islutils/pet_wrapper.h>

isl::union_map computeAllDependences(const Scop &);

#endif
