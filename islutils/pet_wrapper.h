#ifndef ISLUTILS_PET_WRAPPER_H
#define ISLUTILS_PET_WRAPPER_H

#include <islutils/scop.h>
#include <islutils/type_traits.h>
#include <string>         // std::string
#include <vector>         // std::vector

class pet_scop;
class pet_stmt;
class ScopContainer;

namespace pet {

isl::ctx allocCtx();

enum TypeElement {FLOAT, DOUBLE};
class PetArray {
  public:
    PetArray() = delete;
    PetArray(isl::set e, TypeElement et, std::string name, std::string id, int l)
      : extent_(e), type_(et), array_name_(name), array_id_(id), loc_(l) {};
    int dimensionality() const;
    TypeElement type() const;
    std::string name() const;
    std::string dim(int i) const;
    void dump() const;

    isl::set extent_;
    TypeElement type_;
    std::string array_name_;
    std::string array_id_;

    int loc_ = 0;
};


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

std::string printPetStmt(pet_stmt *stmt, isl::id_to_ast_expr ref2expr);
isl::id_to_ast_expr
buildRef2Expr(pet_stmt *stmt, isl::ast_build astBuild,
              __isl_give isl_multi_pw_aff *(*indexTransform)(
                  __isl_take isl_multi_pw_aff *, __isl_keep isl_id *, void *),
              void *user);
std::string printScheduledPetStmt(isl::ast_build astBuild, isl::ast_node node,
                                  pet_stmt *stmt);
std::string printPetAndCustomComments(isl::ast_build build, isl::ast_node node,
                                      pet_stmt *stmt);
std::string printPetAndCustomCommentsWithPayload(isl::ast_build build, isl::ast_node node,
                                      pet_stmt *stmt, void *user);

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
  std::string codegen(
      std::function<std::string(isl::ast_build, isl::ast_node, pet_stmt *stmt)>
          custom = printPetAndCustomComments) const;
  std::string codegenPayload(
      std::function<std::string(isl::ast_build, isl::ast_node, pet_stmt *stmt, void *user)>
          custom = printPetAndCustomCommentsWithPayload, void *user = nullptr ) const;

  /// Find a statement by its identifier.
  pet_stmt *stmt(isl::id id) const;
  /// Return an assignable wrapper class that can be used to overwrite the
  /// Scop's schedule.
  IslCopyRefWrapper<isl::schedule> schedule();
  /// Return a copy of the Scop's schedule.
  isl::schedule schedule() const;
  /// Return a copy of the Scop's context.
  isl::set context() const;
  /// Return pet scop location (start)
  unsigned startPetLocation() const;
  /// Return pet scop location (end)
  unsigned endPetLocation() const;
  /// Return scop reads.
  isl::union_map reads() const;
  /// Return scop writes.
  isl::union_map writes() const;
  /// Return array in scop.
  std::vector<PetArray> arrays() const;

private:
  pet_scop *scop_;
};

} // namespace pet

#endif // ISLUTILS_PET_WRAPPER_H
