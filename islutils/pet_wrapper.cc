#include <pet.h>

#include "islutils/pet_wrapper.h"

namespace pet {

isl::ctx allocCtx() { return isl::ctx(isl_ctx_alloc_with_pet_options()); }

Scop::~Scop() { pet_scop_free(scop_); }

Scop Scop::parseFile(isl::ctx ctx, std::string filename) {
  return Scop(
      pet_scop_extract_from_C_source(ctx.get(), filename.c_str(), nullptr));
}

::Scop Scop::getScop() const {
  ::Scop S;
  S.schedule = isl::manage(pet_scop_get_schedule(scop_));
  S.reads = isl::manage(pet_scop_get_tagged_may_reads(scop_));
  S.mayWrites = isl::manage(pet_scop_get_tagged_may_writes(scop_));
  S.mustWrites = isl::manage(pet_scop_get_tagged_must_writes(scop_));
  return S;
}

} // namespace pet
