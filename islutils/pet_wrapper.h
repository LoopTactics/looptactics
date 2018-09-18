#ifndef ISLUTILS_PET_WRAPPER_H
#define ISLUTILS_PET_WRAPPER_H

#include <islutils/scop.h>

class pet_scop;

namespace pet {

isl::ctx allocCtx();

class Scop {
public:
  explicit Scop(pet_scop *scop) : scop_(scop) {}
  Scop(const Scop &) = delete;
  Scop(Scop &&) = default;
  static Scop parseFile(isl::ctx ctx, std::string filename);
  ~Scop();

  Scop &operator=(const Scop &) = delete;
  Scop &operator=(Scop &&) = default;

  ::Scop getScop() const;

private:
  pet_scop *scop_;
};

} // namespace pet

#endif // ISLUTILS_PET_WRAPPER_H
