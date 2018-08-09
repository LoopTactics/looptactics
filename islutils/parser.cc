#include "parser.h"
#include "pet.h"

Parser::Parser(std::string Filename) : Filename(Filename) {}

Scop Parser::getScop() {
  Scop S;

  isl_ctx *ctx = isl_ctx_alloc_with_pet_options();
  pet_scop *scop = pet_scop_extract_from_C_source(ctx, Filename.c_str(), NULL);
  S.schedule = isl::manage(pet_scop_get_schedule(scop));
  S.reads = isl::manage(pet_scop_get_tagged_may_reads(scop));
  S.mayWrites = isl::manage(pet_scop_get_tagged_may_writes(scop));
  S.mustWrites = isl::manage(pet_scop_get_tagged_must_writes(scop));

  return S;
}
