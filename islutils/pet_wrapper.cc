#include <pet.h>

#include "islutils/die.h"
#include "islutils/operators.h"
#include "islutils/pet_wrapper.h"

#include <vector>

namespace pet {

isl::ctx allocCtx() { return isl::ctx(isl_ctx_alloc_with_pet_options()); }

Scop::~Scop() { pet_scop_free(scop_); }

Scop Scop::parseFile(isl::ctx ctx, std::string filename) {
  return Scop(
      pet_scop_extract_from_C_source(ctx.get(), filename.c_str(), nullptr));
}

isl::ctx Scop::getCtx() const {
  return isl::ctx(isl_schedule_get_ctx(scop_->schedule));
}

::Scop Scop::getScop() const {
  ::Scop S;
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
  // A map between access reference identifiers and ast expressions
  // corresponding to the code that performs those accesses.
  isl::id_to_ast_expr ref2expr;
  // A pointer to the pet statement.
  pet_stmt *stmt;
};

// Wrapper class to be passed as a user pointer to unexported C callbacks
// during AST generation.
struct ScopAndStmtsWrapper {
  const Scop &scop;
  std::vector<StmtDescr> &stmts;
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

  // Extract the schedule in terms of Domain[...] -> Iterators[...], invert it.
  auto astBuild = isl::manage_copy(build);
  auto schedule = isl::map::from_union_map(astBuild.get_schedule());
  auto iteratorMap = isl::pw_multi_aff::from_map(schedule.reverse());

  // Store the statement descriptor with the unique occurrence id and inverted
  // schedule, annotate the AST node with the occurrence identifier so that a
  // later call can find the descriptor using this identifier.
  auto occurrenceId = isl::id::alloc(
      astBuild.get_ctx(), id.get_name() + "_occ_" + std::to_string(counter++),
      nullptr);
  auto ref2expr = isl::manage(
      pet_stmt_build_ast_exprs(statement, astBuild.get(), transformSubscripts,
                               iteratorMap.get(), nullptr, nullptr));
  wrapper->stmts.emplace_back(StmtDescr{occurrenceId, ref2expr, statement});
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

// Generate code for the scop given its current schedule.
std::string Scop::codegen() const {
  auto ctx = getCtx();
  auto build = isl::ast_build(ctx);

  // Construct the list of statement descriptors with partial schedules while
  // building the AST.
  std::vector<StmtDescr> statements;
  ScopAndStmtsWrapper wrapper{*this, statements};
  build = isl::manage(
      isl_ast_build_set_at_each_domain(build.release(), at_domain, &wrapper));
  auto astNode = build.node_from_schedule(isl::manage_copy(scop_->schedule));

  // Print the AST as C code using statement descriptors to emit access
  // expressions.
  isl_printer *prn = isl_printer_to_str(ctx.get());
  prn = isl_printer_set_output_format(prn, ISL_FORMAT_C);
  // options are consumed by ast_node_print
  isl_ast_print_options *options = isl_ast_print_options_alloc(ctx.get());
  options = isl_ast_print_options_set_print_user(
      options,
      [](isl_printer *p, isl_ast_print_options *options, isl_ast_node *node,
         void *user) {
        auto wrapper = static_cast<ScopAndStmtsWrapper *>(user);
        isl_ast_print_options_free(options);
        isl::id occurrenceId = isl::manage_copy(node).get_annotation();
        const StmtDescr &descr = findStmtDescriptor(*wrapper, occurrenceId);
        return pet_stmt_print_body(descr.stmt, p, descr.ref2expr.get());
      },
      &wrapper);
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

} // namespace pet
