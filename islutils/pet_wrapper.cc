#include <pet.h>

#include "islutils/die.h"
#include "islutils/operators.h"
#include "islutils/pet_wrapper.h"
#include <iostream>
#include <vector>

namespace pet {

isl::ctx allocCtx() { return isl::ctx(isl_ctx_alloc_with_pet_options()); }

Scop::~Scop() { pet_scop_free(scop_); }

Scop Scop::parseFile(isl::ctx ctx, std::string filename) {
  return Scop(
      pet_scop_extract_from_C_source(ctx.get(), filename.c_str(), nullptr));
}

static isl_stat fn(struct pet_scop *scop, void *user) {
  auto w = static_cast<ScopContainer*>(user);
  auto s = pet::Scop(scop).getScop();
  w->c.push_back(s);
  isl_stat r;
  return r;
}

ScopContainer Scop::parseMultipleScop(isl::ctx ctx, std::string filename) { 

  ScopContainer res;
  extract_multiple_scop(ctx.get(), filename.c_str(), &fn, &res);
  return res;
}

isl::ctx Scop::getCtx() const {
  return isl::ctx(isl_schedule_get_ctx(scop_->schedule));
}

::Scop Scop::getScop() const {
  ::Scop S;
  S.context = isl::manage(pet_scop_get_context(scop_));
  S.schedule = isl::manage(pet_scop_get_schedule(scop_));
  S.reads = isl::manage(pet_scop_get_tagged_may_reads(scop_));
  S.mayWrites = isl::manage(pet_scop_get_tagged_may_writes(scop_));
  S.mustWrites = isl::manage(pet_scop_get_tagged_must_writes(scop_));
  return S;
}

namespace {
// Descriptor of the statement for the code generation purposes.  One statement
// may appear multiple times in the abstract syntax tree due to, e.g.,
// unrolling or loop splitting.
struct StmtDescr {
  // Unique identifier of the statement occurrence in the AST.
  isl::id occurrenceId;
  // A pointer to the pet statement.
  pet_stmt *stmt;
  // A copy of AST build at the point where the occurrence was encountered.
  // XXX: This is potentially expensive in terms of memory, but simplifies the
  // injection of custom statements to one callback.
  isl::ast_build astBuild;
};

// Wrapper class to be passed as a user pointer to unexported C callbacks
// during AST generation.
struct ScopAndStmtsWrapper {
  const Scop &scop;
  std::vector<StmtDescr> &stmts;
  std::function<std::string(isl::ast_build, isl::ast_node, pet_stmt *)>
      stmtCodegen;
};
} // namespace

// Transform an array subscript of a reference from Domain[...] -> Access[...]
// space to Iterators[...] -> Access[...] space given the
// Iterators[...] -> Domain[...] mapping (inverse of schedule).
// This function is intended to be used as a C callback during AST generation.
static __isl_give isl_multi_pw_aff *
transformSubscripts(__isl_take isl_multi_pw_aff *subscript,
                    __isl_keep isl_id *id, void *user) {
  (void)id;
  auto iteratorMap = isl::manage_copy(static_cast<isl_pw_multi_aff *>(user));
  auto subscr = isl::manage(subscript);
  return subscr.pullback(iteratorMap).release();
}

// Construct the mapping between access reference identifiers and ast
// expressions corresponding to the code performing the access given the
// current schedule.
// This function is intended to be used as a C callback during AST generation.
static __isl_give isl_ast_node *at_domain(__isl_take isl_ast_node *node,
                                          __isl_keep isl_ast_build *build,
                                          void *user) {
  static size_t counter = 0;

  // Find the statement identifier (first argument of the "call" expression).
  auto wrapper = static_cast<ScopAndStmtsWrapper *>(user);
  isl_ast_expr *expr = isl_ast_node_user_get_expr(node);
  isl_ast_expr *idArg = isl_ast_expr_get_op_arg(expr, 0);
  isl_ast_expr_free(expr);
  isl::id id = isl::manage(isl_ast_expr_get_id(idArg));
  isl_ast_expr_free(idArg);
  auto statement = wrapper->scop.stmt(id);

  // Store the statement descriptor with the unique occurrence id and the state
  // of the AST builder at this position, annotate the AST node with the
  // occurrence identifier so that a later call can find the descriptor using
  // this identifier.
  auto astBuild = isl::manage_copy(build);
  auto occurrenceId = isl::id::alloc(
      astBuild.get_ctx(), id.get_name() + "_occ_" + std::to_string(counter++),
      nullptr);
  wrapper->stmts.emplace_back(StmtDescr{occurrenceId, statement, astBuild});
  return isl_ast_node_set_annotation(node, occurrenceId.release());
}

// Find a statement descriptor by its occurrence identifier.
// The descriptor is expected to exist.
static const StmtDescr &findStmtDescriptor(const ScopAndStmtsWrapper &wrapper,
                                           isl::id id) {
  for (const auto &descr : wrapper.stmts) {
    if (descr.occurrenceId == id) {
      return descr;
    }
  }
  ISLUTILS_DIE("could not find statement");
  static StmtDescr dummy;
  return dummy;
}

std::string printPetStmt(pet_stmt *stmt, isl::id_to_ast_expr ref2expr) {
  isl_printer *p = isl_printer_to_str(ref2expr.get_ctx().get());
  p = isl_printer_set_output_format(p, ISL_FORMAT_C);
  p = pet_stmt_print_body(stmt, p, ref2expr.get());
  std::string result(isl_printer_get_str(p));
  isl_printer_free(p);
  return result;
}

isl::id_to_ast_expr
buildRef2Expr(pet_stmt *stmt, isl::ast_build astBuild,
              __isl_give isl_multi_pw_aff *(*indexTransform)(
                  __isl_take isl_multi_pw_aff *, __isl_keep isl_id *, void *),
              void *user) {
  return isl::manage(pet_stmt_build_ast_exprs(
      stmt, astBuild.get(), indexTransform, user, nullptr, nullptr));
}

std::string printScheduledPetStmt(isl::ast_build astBuild, isl::ast_node node,
                                  pet_stmt *stmt) {
  if (!stmt) {
    ISLUTILS_DIE("attempting to print non-pet statement");
  }

  // Extract the schedule in terms of Domain[...] -> Iterators[...],
  // invert it.
  auto schedule = isl::map::from_union_map(astBuild.get_schedule());
  auto iteratorMap = isl::pw_multi_aff::from_map(schedule.reverse());
  auto ref2expr =
      buildRef2Expr(stmt, astBuild, transformSubscripts, iteratorMap.get());
  return printPetStmt(stmt, ref2expr);
}

static inline std::string printIdAsComment(isl::ast_node node) {
  isl::id occurrenceId = node.get_annotation();
  return "// " + occurrenceId.get_name();
}

std::string printPetAndCustomComments(isl::ast_build build, isl::ast_node node,
                                      pet_stmt *stmt) {
  return stmt ? printScheduledPetStmt(build, node, stmt)
              : printIdAsComment(node);
}

static __isl_give isl_printer *
printStatement(__isl_take isl_printer *p,
               __isl_take isl_ast_print_options *options,
               __isl_keep isl_ast_node *node, void *user) {
  auto wrapper = static_cast<ScopAndStmtsWrapper *>(user);

  isl_ast_print_options_free(options);
  isl::id occurrenceId = isl::manage_copy(node).get_annotation();
  const StmtDescr &descr = findStmtDescriptor(*wrapper, occurrenceId);

  if (!wrapper->stmtCodegen) {
    ISLUTILS_DIE("no statement codegen function provided");
  }

  std::string str =
      wrapper->stmtCodegen(descr.astBuild, isl::manage_copy(node), descr.stmt);
  p = isl_printer_start_line(p);
  p = isl_printer_print_str(p, str.c_str());
  return isl_printer_end_line(p);
}

// Generate code for the scop given its current schedule.
std::string Scop::codegen(
    std::function<std::string(isl::ast_build, isl::ast_node, pet_stmt *)>
        custom) const {
  auto ctx = getCtx();
  auto build = isl::ast_build::from_context(getScop().context);

  // Construct the list of statement descriptors with partial schedules while
  // building the AST.
  std::vector<StmtDescr> statements;
  ScopAndStmtsWrapper wrapper{*this, statements, custom};
  build = isl::manage(
      isl_ast_build_set_at_each_domain(build.release(), at_domain, &wrapper));
  auto astNode = build.node_from_schedule(isl::manage_copy(scop_->schedule));

  // Print the AST as C code using statement descriptors to emit access
  // expressions.
  isl_printer *prn = isl_printer_to_str(ctx.get());
  prn = isl_printer_set_output_format(prn, ISL_FORMAT_C);
  // options are consumed by ast_node_print
  isl_ast_print_options *options = isl_ast_print_options_alloc(ctx.get());
  options =
      isl_ast_print_options_set_print_user(options, printStatement, &wrapper);
  prn = isl_ast_node_print(astNode.get(), prn, options);
  char *resultStr = isl_printer_get_str(prn);
  auto result = std::string(resultStr);
  free(resultStr);
  isl_printer_free(prn);
  return result;
}

static isl::id getStmtId(const pet_stmt *stmt) {
  isl::set domain = isl::manage_copy(stmt->domain);
  return domain.get_tuple_id();
}

pet_stmt *Scop::stmt(isl::id id) const {
  for (int i = 0; i < scop_->n_stmt; ++i) {
    if (getStmtId(scop_->stmts[i]) == id) {
      return scop_->stmts[i];
    }
  }
  return nullptr;
}

IslCopyRefWrapper<isl::schedule> Scop::schedule() {
  return IslCopyRefWrapper<isl::schedule>(scop_->schedule);
}

isl::schedule Scop::schedule() const {
  return isl::manage_copy(scop_->schedule);
}

} // namespace pet
