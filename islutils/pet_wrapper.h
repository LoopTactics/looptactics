#ifndef ISLUTILS_PET_WRAPPER_H
#define ISLUTILS_PET_WRAPPER_H

#include <islutils/scop.h>
#include <islutils/type_traits.h>

#include <string>

class pet_scop;
class pet_stmt;

namespace pet {

isl::ctx allocCtx();

template <typename T>
/// A wrapper class around an isl C object, convertible (with copy) to the
/// respective isl C++ type and assignable from such type.
/// This class is inteded to provide access to isl C objects hidden inside
/// other objects without exposing isl C API.
class IslCopyRefWrapper {
public:
  IslCopyRefWrapper(isl_unwrap_t<T> &r) : ref(r) {}

  const T &operator=(const T &rhs) {
    // This will create a C++ wrapper, destroy it immediatly and thus call the
    // appropriate cleaning function for ref.
    isl::manage(ref);

    ref = rhs.copy();
    return rhs;
  }

  operator T() { return isl::manage_copy(ref); }

private:
  isl_unwrap_t<T> &ref;
};

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
