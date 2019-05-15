#include "gtest/gtest.h"
#include <vector>
#include <regex>
#include <string>
#include <iostream>
#include <cassert>
#include <fstream>
#include <string>
#include <algorithm>
#include <islutils/util.h>
#include <islutils/ctx.h>

#include <islutils/matchers.h>
#include <islutils/builders.h>
#include <islutils/access.h>
#include <islutils/access_patterns.h>

/*

* GRAMMAR *

~~~~~
PATTERN
loop(i,j,k) pattern C[i][j] += A[i][k] + B[k][j]
~~~~~
~~~~~
TILING 
loop(i,j,k) tile_size(32, 32, 32) \
  point_loop(i1, j1, k1) tile_loop(i2, j2, k2)

loop(i,j,k) tile_size(range(0,32,+32), range(0,32,+32), range(0,32,+32)) \
  point_loop(i1, j1, k1) tile_loop(i2, j2, k2)
~~~~~

The general idea is:
1. Allow the user to specify the matchers
2. Automatically instantiate those matchers.

#pragma matcher 
  C[i][j] += A[i][k] + B[k][j]
#pragma endmatcher

will instantiate:

  arrayPlaceholder C_
  arrayPlaceholder B_
  arrayPlaceholder A_
  
  Placeholder i_
  Placeholder j_
  Placeholder k_

  match(reads, allOf(access(C_, i_, j_) ...
*/

enum class AccessType {
  READ,
  WRITE
};

class AccessInfo {
  public:
    // 2 * i (2 is the coefficient)
    std::vector<int> coefficients;
    // is the increment positive or negative?
    // i + 1 (positive: 1)
    // i - 1 (negative: 0)
    // i (no increment: -1)
    std::vector<int> signs;
    // actual value for the increment
    std::vector<int> increments;
    // induction variable(s)
    std::vector<std::string> inductions;
    // array name
    std::string arrayId;
    // access type
    AccessType accessType;
};

/// overloading operator for printing AccessInfo
inline std::ostream& operator<< (std::ostream &OS, AccessInfo access) {
  OS << "Array id: " << access.arrayId << "\n";
  for (size_t i = 0; i < access.inductions.size(); i++) 
    OS << "Induction variable id: " << access.inductions[i] << "\n";
  if (access.accessType == AccessType::READ)
    OS << "Read\n";
  else 
    OS << "Write\n";
  return OS;
}

/// overloading operator form printing std::vector<AccessInfo>
inline std::ostream& operator<< (std::ostream &OS, std::vector<AccessInfo> & av) {
  for (size_t i = 0; i < av.size(); i++) {
    OS << "**** Access ****\n";
    OS << av[i];
    OS << "\n\n";
  }
  return OS;
}

/// Extract access info from string a.
/// FIXME
AccessInfo getAccessInfo(std::string a, AccessType type) {

  AccessInfo ai;
  ai.accessType = type;

  // step 1. get array name.
  std::regex regexArrayId(R"((\w+)\[+.*)");
  std::smatch match;
  std::regex_match(a, match, regexArrayId);

  ai.arrayId = match.str(1);

  // step 2.
  // detect expression: 2*x+3
  std::regex exprPlusOp(R"([a-z,A-Z]\[(\d+)\*([a-z,A-Z]+)+\+(\d+)\])");
  while (std::regex_search(a, match, exprPlusOp)) {
    ai.coefficients.push_back(std::stoi(match[1].str()));
    ai.inductions.push_back(match[2].str());
    ai.increments.push_back(std::stoi(match[3].str()));
    ai.signs.push_back(1);
    a = match.suffix().str();
  }
  // detect expression: 2*x-3
  std::regex exprMinusOp(R"([a-z,A-Z]\[(\d+)\*([a-z,A-Z]+)+\-(\d+)\])");
  while (std::regex_search(a, match, exprMinusOp)) {
    ai.coefficients.push_back(std::stoi(match[1].str()));
    ai.inductions.push_back(match[2].str());
    ai.increments.push_back(std::stoi(match[3].str()));
    ai.signs.push_back(0);
    a = match.suffix().str();
  }
  // detect expression 2*x
  std::regex subscriptTimeConst(R"([a-z,A-Z]\[(\d+)\*([a-z,A-Z]+)\])");
  while (std::regex_search(a, match, subscriptTimeConst)) {
    ai.coefficients.push_back(std::stoi(match[1].str()));
    ai.inductions.push_back(match[2].str());
    ai.increments.push_back(0);
    ai.signs.push_back(-1);
    a = match.suffix().str();
  }
  
  // match : induction
  // induction group 1
  std::regex regexSubscript(R"(\[+([a-z])+\])");
  while(std::regex_search(a, match, regexSubscript)) {
    ai.coefficients.push_back(1);
    ai.inductions.push_back(match[1].str());
    ai.increments.push_back(0);
    ai.signs.push_back(-1);
    a = match.suffix().str();
  }

  return ai;
}
  
/// overloading operator for string
inline std::string operator-(std::string &lhs, const std::string &rhs) {
  assert(lhs.length() > rhs.length());
  std::size_t i = lhs.find(rhs);
  if (i != std::string::npos)
    lhs.erase(i, rhs.length());
  return lhs;
}

/// Remove pattern p from string s in a recursive way
//static std::string removeFromStringRecursive(std::string s, std::string p) {
//  while(s.find(p) != std::string::npos)
//    s.erase(s.find(p), p.length());
//  return s;
//}
    
/// split string s based in pattern regexAsString.
/// The function returns substrings
std::vector<std::string>
split(const std::string &s, std::string regexAsString) {

  std::vector<std::string> res;
  std::regex regex(regexAsString);

  std::sregex_token_iterator iter(s.begin(), s.end(), regex, -1);
  std::sregex_token_iterator end;
  while(iter != end) {
    res.push_back(*iter);
    ++iter;
  }
  return res;
}

std::vector<AccessInfo> getAccessesInfo(std::string p) {

  std::string lhs, rhs;
  std::vector<AccessInfo> accessesInfo = {};
  // remove white spaces.
  p.erase(std::remove_if(p.begin(), p.end(), isspace), p.end());

  // check the expression contains = operator.
  std::smatch match;
  std::regex equalOperator(R"(=)");
  std::regex_search(p, match, equalOperator);
  assert(match.size() == 1 && "multiple equal operators");
  
  // get lhs and rhs
  std::string pCopy = p;
  std::regex RHSExpr(R"([a-z,A-Z]+(\[[a-z,A-Z,1-9,+,*,-]+\])+(?=[^=]*$))");
  while(std::regex_search(pCopy, match, RHSExpr)) {
    rhs += match[0].str();
    pCopy = match.suffix().str();
  }
  std::regex LHSExpr(R"([a-z,A-Z]+(\[[a-z,A-Z,1-9,+,*,-]+\])+(?![^=]*$))");
  std::regex_search(p, match, LHSExpr);
  lhs = match[0].str();

  // check if we are dealing with += or = operator.
  // in case we are dealing with += we append lhs to rhs.
  if (p.find("+=") != std::string::npos)
    rhs += lhs; 

  std::regex Expr(R"(([a-z,A-Z,1-9]+(\[[a-z,A-Z,1-9,+,*,-]+\])+)+)");
  assert(std::regex_match(lhs, match, Expr) && "wrong format lhs");
  assert(std::regex_match(rhs, match, Expr) && "wrong format rhs");
  std::regex SubExpr(R"([a-z,A-Z]+(\[[a-z,A-Z,1-9,+,*,-]+\])+)");
 
  std::string lhsCopy = lhs; 
  while(std::regex_match(lhsCopy, match, SubExpr)) {
    accessesInfo.push_back(getAccessInfo(match[0], AccessType::WRITE));
    lhsCopy = match.suffix().str();
  }
  std::string rhsCopy = rhs;
  while(std::regex_search(rhsCopy, match, SubExpr)) {
    accessesInfo.push_back(getAccessInfo(match[0], AccessType::READ));
    rhsCopy = match.suffix().str();
  }

  return accessesInfo;
}

/// This function takes as input a string that looks like
/// loop(i, j, k) pattern C[i][j] += A[i][k] * B[k][j]
void handlePattern(std::string pattern) {
  ASSERT_TRUE(pattern.find("pattern") != std::string::npos);

  // remove white spaces. 
  pattern.erase(std::remove_if(pattern.begin(), pattern.end(), isspace), pattern.end());
  // remove 'pattern'
  pattern = pattern - "pattern";
  // identify keyword loop(*/ loop dims */)
  std::smatch match;
  std::regex keywordLoop(R"(loop\(([a-z]+,){1,}[a-z]+\))");
  std::regex_search(pattern, match, keywordLoop);
  std::string loopKeyword = match[0];
  // remove from pattern the keyword loopKeyword.
  pattern = pattern - loopKeyword;
  // remove 'loop', '(' and ')' from loop(/* loop dims */)
  loopKeyword = loopKeyword - "loop";
  loopKeyword = loopKeyword - ")";
  loopKeyword = loopKeyword - "(";
  // split the string based on ','. The size 
  // of the resulting vector will be the number of
  // dimensions in the loop.
  //unsigned int dims = split(loopKeyword, ",").size();
  auto accessInfo = getAccessesInfo(pattern);  
}

TEST(Parser, regexp) {

  std::string target = "loop(i,j,k)CXX";
  std::string targetOne = "loop(i,j,km)";
  std::string targetTwo = "loop(i2, j, k)";
  std::string targetThree = "loop(,,j)";
  std::string targetFour = "loop((,,,)";
  std::smatch match;
  std::regex keywordLoop(R"(loop\(([a-z]+,){1,}[a-z]+\))");
  
  std::regex_search(target, match, keywordLoop);
  EXPECT_EQ(match.str(0), "loop(i,j,k)");
  std::regex_search(targetOne, match, keywordLoop);
  EXPECT_EQ(match.str(0), "loop(i,j,km)");
  std::regex_search(targetTwo, match, keywordLoop);
  EXPECT_EQ(match.str(0), "");
  std::regex_search(targetThree, match, keywordLoop);
  EXPECT_EQ(match.str(0), "");
  std::regex_search(targetFour, match, keywordLoop);
  EXPECT_EQ(match.str(0), "");

}

TEST(Parser, Access) {
  // matcher spec.
  std::string s1 = "C[i][j] += A[i][j] - B[k][j]";
  auto accesses = getAccessesInfo(s1);
  EXPECT_TRUE(accesses.size() == 4); 
}

class Tiling {
  public:
    std::vector<int> tileSizes;
    std::vector<std::string> idTileLoop;
    std::vector<std::string> idPointLoop;
};

/// overloading for printing Tiling
inline std::ostream& operator<< (std::ostream &OS, Tiling t) {
  OS << "tile transformation: \n";
  OS << "tile size : ";
  for (size_t i = 0; i < t.tileSizes.size(); i++) 
    OS << t.tileSizes[i] << " ";
  OS << "\n";
  OS << "tile id : ";
  for (size_t i = 0; i < t.idTileLoop.size(); i++)
    OS << t.idTileLoop[i] << " ";
  OS << "\n";
  OS << "point id : ";
  for (size_t i = 0; i < t.idPointLoop.size(); i++)
    OS << t.idPointLoop[i] << " ";
  OS << "\n";
  return OS;
}
  
bool handleTiling(std::string pattern) {

  Tiling t;

  // remove white spaces. 
  pattern.erase(std::remove_if(pattern.begin(), pattern.end(), isspace), pattern.end());
  // identify keyword loop(/* loop dims */)
  std::smatch match;
  std::regex keywordLoop(R"(loop\(([a-z]+,){1,}[a-z]+\))");
  if (!std::regex_search(pattern, match, keywordLoop)) {
    std::cout << "keyword detection failed\n";
    return false;
  }
  std::string loopKeyword = match[0];
  // remove from pattern the keyword loopKeyword.
  pattern = pattern - loopKeyword;
  // remove 'loop', '(' and ')' from loop(/* loop dims */)
  loopKeyword = loopKeyword - "loop";
  loopKeyword = loopKeyword - ")";
  loopKeyword = loopKeyword - "(";
  // split the string based on ','. The size 
  // of the resulting vector will be the number of
  // dimensions in the loop.
  unsigned int dims = split(loopKeyword, ",").size();
  if (dims == 0)
    return false;
  
  // identify keyword tile_size(/* tile sizes */)
  std::regex keywordTileSize(R"(tile_size\(([1-9]+,){1,}[1-9]+\))");
  if (!std::regex_search(pattern, match, keywordTileSize)) {
    std::cout << "keyword detection failed (tile_size)\n";
    return false;
  }
  std::string tileSizeKeyword = match[0];
  // remove from pattern the keyword tileSizeKeyword.
  pattern = pattern - tileSizeKeyword;
  tileSizeKeyword = tileSizeKeyword - "tile_size";
  tileSizeKeyword = tileSizeKeyword - ")";
  tileSizeKeyword = tileSizeKeyword - "(";
  // split the string based on ','
  std::vector<std::string> tileSizes = 
    split(tileSizeKeyword, ",");
  for (size_t i = 0; i < tileSizes.size(); i++) {
    t.tileSizes.push_back(std::stoi(tileSizes[i]));
  }
  if (t.tileSizes.size() != dims) 
    return false;

  // identify keyword tile_loop (/* id tiles loop */)
  std::regex keywordTileLoopId(R"(tile_loop\(([a-z,A-Z,1-9]+,){1,}[a-z,A-Z,1-9]+\))");
  if (!std::regex_search(pattern, match, keywordTileLoopId)) {
    std::cout << "keyword detection failed (tile_loop)\n";
    return false;
  }
  std::string tileLoopIdKeyword = match[0];
  // remove from pattern the keyword tileLoopIdKeyword
  pattern = pattern - tileLoopIdKeyword;
  tileLoopIdKeyword = tileLoopIdKeyword - "tile_loop";
  tileLoopIdKeyword = tileLoopIdKeyword - ")";
  tileLoopIdKeyword = tileLoopIdKeyword - "(";
  // split the string based on ','
  std::vector<std::string> idTileLoop =
    split(tileLoopIdKeyword, ","); 
  if (idTileLoop.size() != dims)
    return false;
  t.idTileLoop = std::move(idTileLoop);

  // identify the keyword point_loop (/* id point loop */)
  std::regex keywordPointLoopId(R"(point_loop\(([a-z,A-Z,1-9]+,){1,}[a-z,A-Z,1-9]+\))");
  if (!std::regex_search(pattern, match, keywordPointLoopId)) {
    std::cout << "keyword detection failed (point_loop)\n";
    return false;
  }
  std::string pointLoopKeyword = match[0];
  // remove from pattern the keyword pointLoopKeyword
  pattern = pattern - pointLoopKeyword;
  pointLoopKeyword = pointLoopKeyword - "point_loop";
  pointLoopKeyword = pointLoopKeyword - "(";
  pointLoopKeyword = pointLoopKeyword - ")";
  // split the string based on ','
  std::vector<std::string> idPointLoop =
    split(pointLoopKeyword, ",");
  if (idPointLoop.size() != dims)
    return false;
  t.idPointLoop = std::move(idPointLoop);
  
  std::cout << t << std::endl;
  return true;
}
  
TEST(Parser, Tiling) {
  // loop(i,j,k) tile_size(32,32,32) /
  //  point_loop(i1, j1, k1) tile_loop(i2, j2, k2)
  //
  // loop(i,j,k) tile_size(range(0, 32, +32), range(0, 32, +32), range(0, 32, +32)) /
  //  point_loop(i1, j1, k1) tile_loop(i2, j2, k2)

  std::string s1 =
    "loop(i,j,k) tile_size(32,32,32) / point_loop(i1, j1, k1) tile_loop(i2, j2, k2)";
  EXPECT_TRUE(handleTiling(s1));
}

std::vector<std::string> getUniqueInductionIds(std::vector<AccessInfo> &AI) {

  std::vector<std::string> I = {};
  for (size_t i = 0; i < AI.size(); i++) {
    for (size_t j = 0; j < AI[i].inductions.size(); j++) {
      I.push_back(AI[i].inductions[j]);
    }
  }
  std::sort(I.begin(), I.end());
  I.erase(std::unique(I.begin(), I.end()), I.end());
  return I;
}

std::vector<std::string> getArrayIds(std::vector<AccessInfo> &AI) {

  std::vector<std::string> A = {};
  for (size_t i = 0; i < AI.size(); i++) {
    A.push_back(AI[i].arrayId);
  }
  std::sort(A.begin(), A.end());
  A.erase(std::unique(A.begin(), A.end()), A.end());
  return A;
}

// forward decl.
isl::schedule_node 
replaceDFSPreorderRepeatedly(isl::schedule_node,
                             const matchers::ScheduleNodeMatcher &,
                             const builders::ScheduleNodeBuilder &);
isl::schedule_node
rebuild(isl::schedule_node,
        const builders::ScheduleNodeBuilder &);
isl::schedule_node
replaceRepeatedly(isl::schedule_node,
                  const matchers::ScheduleNodeMatcher &,
                  const builders::ScheduleNodeBuilder &);

isl::schedule collapseBands(isl::schedule schedule) {

  isl::schedule_node node = schedule.get_root().child(0);

  isl::schedule_node parent, child, grandchild;
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
      band(child,
        anyTree(grandchild)));
    //clang-format on
  }();
    
  auto merger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    // clang-format off
    auto computeSched = [&]() {
      isl::multi_union_pw_aff sched =
        parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
      return sched;
    };
    // clang-format on
    auto st = [&]() { return subtreeBuilder(grandchild); };
    merger = band(computeSched, subtree(st));
  }

  node = replaceDFSPreorderRepeatedly(node, matcher, merger);
  return node.root().get_schedule();
}

isl::schedule_node
replaceDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement) {
  node = replaceRepeatedly(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderRepeatedly(node.child(i), pattern, replacement)
               .parent();
  }
  return node;
}

isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

isl::schedule_node
applyRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  std::function<isl::schedule_node(isl::schedule_node)> func) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = func(node);
  }
  return node;
}

isl::schedule_node
applyDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             std::function<isl::schedule_node(isl::schedule_node)> func) {
  node = applyRepeatedly(node, pattern, func);
  for (int i = 0; i < node.n_children(); ++i) {
    node = applyDFSPreorderRepeatedly(node.child(i), pattern, func)
               .parent();
  }
  return node;
}

isl::schedule_node
applyOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            std::function<isl::schedule_node(isl::schedule_node)> func) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = func(node);
  }
  return node;
}

isl::schedule_node
applyDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       std::function<isl::schedule_node(isl::schedule_node)> func) {
  node = applyOnce(node, pattern, func);
  for (int i = 0; i < node.n_children(); ++i) {
    node = applyDFSPreorderOnce(node.child(i), pattern, func).parent();
  }
  return node;
}

std::vector<AccessInfo> getReadAccesses(std::vector<AccessInfo> &AI) {
  std::vector<AccessInfo> res = {};
  for (size_t i = 0; i < AI.size(); i++) {
    if (AI[i].accessType == AccessType::READ) {
      res.push_back(AI[i]);
    }
  }
  return res;
}

std::vector<AccessInfo> getWriteAccesses(std::vector<AccessInfo> &AI) {
  std::vector<AccessInfo> res = {}; 
  for (size_t i = 0; i < AI.size(); i++) {
    if (AI[i].accessType == AccessType::WRITE) {
      res.push_back(AI[i]);
    }
  }
  return res;
}

//using namespace matchers;

//template <typename CandidatePayload, typename PatternPayload>
//class PlaceholderSet {
//  public:
//    Placeholder<CandidatePayload,PatternPayload> p;
//    std::string id;
//};

//template <typename CandidatePayload, typename PatternPayload>
//using PlaceholderSet =
//  std::vector<std::map<std::string, Placeholder<CandidatePayload,PatternPayload>>>;

//class ArrayPlaceholderSet { 
//  public:
//    ArrayPlaceholder ap;
//    std::string id;
//};

//template<class T>
//int getIndex (std::string t, std::vector<T> &p) {
//  for (size_t i = 0; i < p.size(); i++) 
//    if (p[i].id == t)
//      return i;
//  assert(0 && "index not found");
//  return -1;
//}


template <typename T>
class Pset {
  public:
    T t;
    std::string id;
};
     
TEST(Parser, DetectPatternFromSpec) {

  using namespace matchers;
  using namespace util;
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/1mmWithoutInitStmt.c");
  auto scop = petScop.getScop();

  using P = Placeholder<SingleInputDim,UnfixedOutDimPattern<SimpleAff>>;
  using A = ArrayPlaceholder;
  using ACC = ArrayPlaceholderList<SingleInputDim, FixedOutDimPattern<SimpleAff>>;

  // unique placeholder and arrayPlaceholder.
  std::vector<Pset<P>> vectorPlaceholderSet = {};
  std::vector<Pset<A>> vectorArrayPlaceholderSet = {};
  std::vector<ACC> vectorAccesses = {};

  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();
  isl::schedule schedule = scop.schedule;
  schedule = collapseBands(schedule);
  isl::schedule_node root = schedule.get_root();
  
  std::string s1 = "C[i][j] += A[i][k] - B[k][j]";
  auto accesses = getAccessesInfo(s1);

  // extract unique array and induction variable ids.
  // i.e., for C[i][j] +] A[i][k] * B[k][j]
  // uniqueArrayIds is {C, A, B};
  // uniqueInductionIds is {i, j, k}
  auto arrayIds = getArrayIds(accesses);
  auto inductionIds = getUniqueInductionIds(accesses);
  for (size_t i = 0; i < arrayIds.size(); i++) {
    std::cout << "array :" << arrayIds[i] << std::endl;  
  }
  for (size_t i = 0; i < inductionIds.size(); i++) {
    std::cout << "induction : " << inductionIds[i] << std::endl;
  }
  EXPECT_TRUE(arrayIds.size() == 3);
  EXPECT_TRUE(inductionIds.size() == 3);

  // instantiate placeholders and arrayPlaceholder.
  for (size_t i = 0; i < inductionIds.size(); i++) {
    Pset<P> tmp = {placeholder(ctx), inductionIds[i]};
    std::cout << "assigend id to placeholder is :" << inductionIds[i] << "\n";
    vectorPlaceholderSet.push_back(tmp);
  }
  for (size_t i = 0; i < arrayIds.size(); i++) {
    Pset<A> tmp = {arrayPlaceholder(), arrayIds[i]};
    std::cout << "assigned id to arrayPlaceholder is :" << arrayIds[i] << "\n";
    vectorArrayPlaceholderSet.push_back(tmp);
  }

  // get reads accesses.
  auto accessesRead = getReadAccesses(accesses);
  EXPECT_TRUE(accessesRead.size() == 3);
  std::cout << accessesRead << std::endl;

  // prepare placeholder and arrayPlaceholder.
  vectorAccesses.push_back(access(vectorArrayPlaceholderSet[0].t,
                                  vectorPlaceholderSet[0].t,
                                  vectorPlaceholderSet[1].t));  
  vectorAccesses.push_back(access(vectorArrayPlaceholderSet[1].t,
                                  vectorPlaceholderSet[0].t,
                                  vectorPlaceholderSet[2].t));
  vectorAccesses.push_back(access(vectorArrayPlaceholderSet[2].t,
                                  vectorPlaceholderSet[2].t,
                                  vectorPlaceholderSet[1].t));
  auto psRead = allOf(vectorAccesses);
  auto readMatches = match(scop.reads.curry(), psRead);

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _A = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _C = arrayPlaceholder();
  
  auto psReadOrig = 
    allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto readMatchesOrig = match(scop.reads.curry(), psReadOrig);             
  
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[0].t].payload().inputDimPos_,
            readMatchesOrig[0][_i].payload().inputDimPos_); 
}

















