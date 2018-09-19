#ifndef ISLUTILS_PET_WRAPPER_H
#define ISLUTILS_PET_WRAPPER_H

#include <islutils/scop.h>

#include <string>

class pet_scop;
class pet_stmt;

namespace pet {

isl::ctx allocCtx();

class Scop {
public:
  explicit Scop(pet_scop *scop) : scop_(scop) {}
  Scop(const Scop &) = delete;
  Scop(Scop &&) = default;
  static Scop parseFile(isl::ctx ctx, std::string filename);
  ~Scop();

  // pet_scop does not feature a copy function
  Scop &operator=(const Scop &) = delete;
  Scop &operator=(Scop &&) = default;

  /// Obtain the isl context in which the Scop lives.
  isl::ctx getCtx() const;
  /// Get a ::Scop representation of this object removing all pet-specific
  /// parts. Modifying the result will not affect this Scop.
  ::Scop getScop() const;
  /// Generate code
  std::string codegen() const;

  /// Find a statement by its identifier.
  pet_stmt *stmt(isl::id id) const;

private:
  pet_scop *scop_;
};

} // namespace pet

#endif // ISLUTILS_PET_WRAPPER_H
