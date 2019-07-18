
#include <pet.h>
#include "islutils/die.h"
#include "islutils/operators.h"
#include "islutils/pet_wrapper.h"
#include <iostream>
#include <vector>

namespace pet {

void PetArray::dump() const {

  std::cout << "=========\n";
  std::cout << "extent : " << extent_.to_str() << "\n";
  std::cout << "array name : " << array_name_ << "\n";
  std::cout << "array id : " << array_id_ << "\n";
  std::cout << "loc : " << loc_ << "\n";
  if (type_ == TypeElement::FLOAT)
    std::cout << "type : float\n";
  else 
    std::cout << "type : double\n";
  std::cout << "=========\n";
}

std::string PetArray::dim(int i) const {

  size_t tmp = i;
  assert(tmp < extent_.get_space().dim(isl::dim::out) && "should be less!");
  isl::pw_aff pwaff = extent_.dim_max(i);
  assert(pwaff.n_piece() == 1 && "expect single piece");
  isl::val val;
  pwaff.foreach_piece([&](isl::set s, isl::aff a) -> isl_stat {
    val = a.get_constant_val();
    return isl_stat_ok;
  });

  val = val.add(isl::val::one(val.get_ctx()));
  return val.to_str();
}


TypeElement PetArray::type() const {

  return type_;
}

int PetArray::dimensionality() const {

  auto space = extent_.get_space();
  return space.dim(isl::dim::out);
}

std::string PetArray::name() const {

  return array_name_;
}

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
  S.context = isl::manage(pet_scop_get_context(scop_));
  S.schedule = isl::manage(pet_scop_get_schedule(scop_));
  S.reads = isl::manage(pet_scop_get_tagged_may_reads(scop_));
  S.mayWrites = isl::manage(pet_scop_get_tagged_may_writes(scop_));
  S.mustWrites = isl::manage(pet_scop_get_tagged_must_writes(scop_));
  S.n_array = scop_->n_array;
  for(int i = 0; i < S.n_array; ++i) {
    ScopArray a;
    a.context = isl::manage_copy(scop_->arrays[i]->context);
    a.extent = isl::manage_copy(scop_->arrays[i]->extent);
    a.element_type = scop_->arrays[i]->element_type;
    a.element_is_record = scop_->arrays[i]->element_is_record;
    a.element_size = scop_->arrays[i]->element_size;
    a.live_out = scop_->arrays[i]->live_out;
    a.uniquely_defined = scop_->arrays[i]->uniquely_defined;
    a.declared = scop_->arrays[i]->declared;
    a.exposed = scop_->arrays[i]->exposed;
    a.outer = scop_->arrays[i]->outer;
    S.arrays.push_back(a);
  }
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

struct ScopAndStmtsWrapperWithPayload {
  const Scop &scop;
  std::vector<StmtDescr> &stmts;
  std::function<std::string(isl::ast_build, isl::ast_node, pet_stmt *, void *user)>
      stmtCodegen;
  void *user;
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

std::string printPetAndCustomCommentsWithPayload(isl::ast_build build, isl::ast_node node,
                                                 pet_stmt *stmt, void *user) {
  return printPetAndCustomComments(build, node, stmt);
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
  auto build = isl::ast_build::from_context(context());

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

// Find a statement descriptor by its occurrence identifier.
// The descriptor is expected to exist.
static const StmtDescr &findStmtDescriptorPayload(const ScopAndStmtsWrapperWithPayload &wrapper,
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

static __isl_give isl_printer *
printStatementPayload(__isl_take isl_printer *p,
               __isl_take isl_ast_print_options *options,
               __isl_keep isl_ast_node *node, void *user) {
  auto wrapper = static_cast<ScopAndStmtsWrapperWithPayload *>(user);

  isl_ast_print_options_free(options);
  isl::id occurrenceId = isl::manage_copy(node).get_annotation();
  const StmtDescr &descr = findStmtDescriptorPayload(*wrapper, occurrenceId);

  if (!wrapper->stmtCodegen) {
    ISLUTILS_DIE("no statement codegen function provided");
  }

  std::string str =
      wrapper->stmtCodegen(descr.astBuild, isl::manage_copy(node), descr.stmt, wrapper->user);
  p = isl_printer_start_line(p);
  p = isl_printer_print_str(p, str.c_str());
  return isl_printer_end_line(p);
}

// Generate code for the scop given its current schedule and a given payload.
std::string Scop::codegenPayload(
    std::function<std::string(isl::ast_build, isl::ast_node, pet_stmt *, void *user)>
        custom, void *user) const {
  auto ctx = getCtx();
  auto build = isl::ast_build::from_context(context());

  // Construct the list of statement descriptors with partial schedules while
  // building the AST.
  std::vector<StmtDescr> statements;
  ScopAndStmtsWrapperWithPayload wrapper{*this, statements, custom, user};
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
      isl_ast_print_options_set_print_user(options, printStatementPayload, &wrapper);
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

unsigned Scop::startPetLocation() const {
  return pet_loc_get_start(scop_->loc);
}

unsigned Scop::endPetLocation() const {
  return pet_loc_get_end(scop_->loc);
}
/*
// partially taken from:
// https://github.com/spcl/haystack/blob/0753e076f0ea28da68533577edb60fbc5092ccbe/src/isl-helpers.cpp#L53
static std::string print_isl_expression(isl::aff aff) {

  std::string result{};

  for (int i = 0; i < aff.dim(isl::dim::in); i++) {
    isl::val coefficient = aff.get_coefficient_val(isl::dim::in, i);
    if (!coefficient.is_zero()) {
      result += aff.get_dim_name(isl::dim::in, i);
    }
  }
  return result;     
}
*/

isl::union_map Scop::reads() const {

  return isl::manage(pet_scop_get_tagged_may_reads(scop_)).curry();
}

isl::union_map Scop::writes() const {

  return isl::manage(pet_scop_get_tagged_may_writes(scop_)).curry();
}

isl::set Scop::context() const {

  return isl::manage(pet_scop_get_context(scop_));
}

std::vector<PetArray> Scop::arrays() const {

  std::vector<PetArray> res;

  struct Payload {
    std::string array_name;
    std::string array_id; 
    int line = 0;
  };
  std::vector<Payload> payloads;

  for (int idx = 0; idx < scop_->n_stmt; idx++) {
    pet_expr *expression = pet_tree_expr_get_expr(scop_->stmts[idx]->body);
    isl::space space = isl::manage(pet_stmt_get_space(scop_->stmts[idx]));
    std::string statement = space.get_tuple_name(isl::dim::set);

    auto get_array_info = [](__isl_keep pet_expr *expr, void *user) {
  
      auto ups = (std::vector<Payload> *)user;
      if (pet_expr_access_is_read(expr) || pet_expr_access_is_write(expr)) {
        std::string id = isl::manage(pet_expr_access_get_ref_id(expr)).to_str();
        std::string name = isl::manage(pet_expr_access_get_id(expr)).to_str();   
        ups->push_back({name, id});
      }

      return 0;
    };
    pet_expr_foreach_access_expr(expression, get_array_info, &payloads);
    pet_expr_free(expression);

    pet_loc *loc = pet_tree_get_loc(scop_->stmts[idx]->body);
    int line = pet_loc_get_line(loc);
    pet_loc_free(loc);
    for (auto &p : payloads) {
      p.line = line;
    }
  }

  for (int idx = 0; idx < scop_->n_array; idx++) {
    isl::set ext = isl::manage_copy(scop_->arrays[idx]->extent);
    std::string name = ext.get_tuple_name();
    TypeElement t = 
      (std::string(scop_->arrays[idx]->element_type).compare("float") == 0) ?
        TypeElement::FLOAT : TypeElement::DOUBLE;
    for (const auto &p : payloads) {
      if (name.compare(p.array_name) == 0) {
        res.push_back({ext, t, p.array_name, p.array_id, p.line});
        break;
      }
    }
  }

  #ifdef DEBUG
  std::cout << "# arrays : " << res.size();
  for (const auto &a : res)
    a.dump();
  #endif

  return res;
}  

} // namespace pet
