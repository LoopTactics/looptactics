#include <islutils/tuner.h>

using namespace TunerLoopTactics;
using namespace LoopTactics;
using namespace pet;

// Messages printed to stdout (in colours)
const std::string kMessageFull    = "\x1b[32m[==========]\x1b[0m";
const std::string kMessageHead    = "\x1b[32m[----------]\x1b[0m";
const std::string kMessageRun     = "\x1b[32m[ RUN      ]\x1b[0m";
const std::string kMessageInfo    = "\x1b[32m[   INFO   ]\x1b[0m";
const std::string kMessageVerbose = "\x1b[39m[ VERBOSE  ]\x1b[0m";
const std::string kMessageOK      = "\x1b[32m[       OK ]\x1b[0m";
const std::string kMessageWarning = "\x1b[33m[  WARNING ]\x1b[0m";
const std::string kMessageFailure = "\x1b[31m[   FAILED ]\x1b[0m";
const std::string kMessageResult  = "\x1b[32m[ RESULT   ]\x1b[0m";
const std::string kMessageBest    = "\x1b[35m[     BEST ]\x1b[0m";

bool Tuner::check_configurations(const TileConfigurations cs) {
  bool res = (cs.size() == 0) ? false : true;
  return res;
}

bool Tuner::check_arrays(const std::vector<PetArray> pa) {
  bool res = (pa.size() == 0) ? false : true;
  return res;
}

Tuner::Tuner(const TileConfigurations cs, 
  const std::vector<PetArray> pa,
  const std::string path_to_file, isl::schedule current_schedule) :
  cs_(check_configurations(cs) ? cs : throw Error::Error("invalid configurations")),
  arrays_(check_arrays(pa) ? pa : throw Error::Error("invalid arrays")),
  opt_(path_to_file),
  current_schedule_(current_schedule) {}

inline std::string insertTab(int tab) {
  std::string result;
  for(int i = 0; i < tab; ++i) {
    result += " ";
  }
  return result;
}

static std::string dumpHeaders() {

  std::string code = "";
  code += "\n#include <sys/time.h>\n";
  code += "#include <assert.h>\n";
  code += "#include <stdio.h>\n"; 
  code += "#include <stdlib.h>\n\n";
  return code;
}

static std::string dumpDefines() {

  std::string code = "";
  code += "#define min(a,b) (((a)<(b))?(a):(b))\n";
  code += "#define max(a,b) (((a)>(b))?(a):(b))\n\n";
  return code;
}

static std::string dumpTimingUtilities() {

  std::string code = "";
  code += "\nstatic double start_walltime;\n";
  code += "unsigned long long start_cycle;\n";
  code += "// Timing function \n\n";
  code += "static inline double rtclock() {\n";
  code += insertTab(2) + "struct timezone Tzp; \n";
  code += insertTab(2) + "struct timeval Tp; \n";
  code += insertTab(2) + "int stat;\n";
  code += insertTab(2) + "stat = gettimeofday (&Tp, &Tzp);\n";
  code += insertTab(2) + "if (stat != 0) assert(0); \n";
  code += insertTab(2) + "return (Tp.tv_sec + Tp.tv_usec*1.0e-6);\n";
  code += "}\n\n";
  code += "void init_timer() {\n";
  code += insertTab(2) + "start_walltime = -1.0;\n";
  code += "}\n\n";
  code += "static inline void start_timer() {\n";
  code += insertTab(2) + "start_walltime = rtclock();\n";
  code += "}\n\n";
  code += "static double inline stop_timer() {\n";
  code += insertTab(2) + "return rtclock() - start_walltime;\n";
  code += "}\n\n";
  code += "void reset_timer() {\n";
  code += insertTab(2) + "start_walltime = -1.0;\n";
  code += "}\n";
  code += "double get_walltime() {\n";
  code += insertTab(2) + "return rtclock();\n";
  code += "}\n\n";
  code += "double get_start_walltime() {\n";
  code += insertTab(2) + "return start_walltime;\n";
  code += "}\n\n";
  return code;
}


static std::string insertConstantDecl(const PetArray &array) {
  assert(0 && "not implemented!");
  return "";
}

static std::string insert1Decl(const PetArray &array) {
  assert(0 && "not implemented!");
  return "";
}

static std::string insert2Decl(const PetArray &array) {

  std::string code;
  
  std::string type = "unknown";
  if (array.type() == TypeElement::FLOAT)
    type = "float";
  else 
    type = "double";

  code += type + " **" + array.name() + " = " +
       + "(" + type + " **)malloc(" + array.dim(0)
       + " * sizeof(" + type + "*));\n";
  code += "for (int i = 0; i < " + array.dim(0) + "; i++)\n";
  code += insertTab(2) + array.name() + "[i] = (" 
       +  type + " *)malloc(" + array.dim(1) + " * sizeof(" + type + "));\n\n";

  return code;
}

static std::string insert3Decl(const PetArray &array) {

  std::string code;
  
  std::string type = "unknown";
  if (array.type() == TypeElement::FLOAT)
    type = "float";
  else 
    type = "double";

  code += type + " ***" + array.name() + " = " +
       + "(" + type + " ***)malloc(" + array.dim(0)
       + " * sizeof(" + type + "**));\n";
  code += "for (int i = 0; i < " + array.dim(0) + "; i++) {\n";
  code += insertTab(2) + array.name() + "[i] = ("
       + type + " **)malloc(" + array.dim(1) + " * sizeof(" + type + "*));\n";
  code += insertTab(2) + "for (int j = 0; j < " + array.dim(1) + "; j++) \n";
  code += insertTab(4) + array.name() + "[i][j] = (" 
       + type + "*)malloc(" + array.dim(2) + " * sizeof(" + type + "))\n";
  code += insertTab(2) + "}\n";
  
  return code;
} 

static std::string insertDecl(const PetArray &array) {

  std::string code = "";
  int dims = array.dimensionality();
  switch (dims) {
    case 0: 
      code += insertConstantDecl(array);
      break;
    case 1: 
      code += insert1Decl(array);
      break;
    case 2: 
      code += insert2Decl(array);
      break;
    case 3:
      code += insert3Decl(array);
      break;
    default: 
      std::cout << array.extent_.to_str() << "\n";
      assert(0 && "unknown array type");
  }
  return code;
}

static std::string dumpArrayDecl(const std::vector<PetArray> &arrays) {

  std::string code = "";
  for(const auto &a : arrays)
    code += insertDecl(a);
  code += "\n";
  return code;
}

static std::string insertConstantInit(const PetArray &array) {

  assert(0 && "not implemented!");
  return "nullptr";
}

static std::string insert1DInit(const PetArray &array) {

  assert(0 && "not implemented!");
  return "nullptr";
}

static std::string insert2DInit(const PetArray &array) {

  std::string type = "unknown";
  if (array.type() == TypeElement::FLOAT)
    type = "float";
  else
    type = "double";

  std::string code = 
    "for (int i = 0; i < " + array.dim(0) + "; i++)\n";
  code += insertTab(2) 
    + "for (int j = 0; j < " + array.dim(1) + "; j++)\n";
  code += insertTab(4)
    + array.name() + "[i][j] = ((" + type + ") i*j)/" + array.dim(0) + ";\n\n";
  return code;
}

static std::string insert3DInit(const PetArray &a) {
  
  assert(0 && "not implemented!");
  return "nullptr";
}

static std::string insertInit(const PetArray &array) {

  std::string code = "";
  int dims = array.dimensionality();
  switch(dims) {
    case 0:
      code += insertConstantInit(array);
      break;
    case 1:
      code += insert1DInit(array);
      break;
    case 2:
      code += insert2DInit(array);
      break;
    case 3:
      code += insert3DInit(array);
      break;
    default:
      std::cout << array.extent_.to_str() << "\n";
      assert(0 && "unknown array type");
  }
  return code;
}
     

static std::string dumpArrayInit(const std::vector<PetArray> &arrays) {

  std::string code = "";
  for (const auto &a : arrays)
    code += insertInit(a);
  code += "\n";
  return code;
}

static std::string dumpTimingStart() {
  
  std::string code = "";

  code += "double refElapsed;\n";
  code += "init_timer();\n";
  code += "start_timer();\n";
  return code;
}

static std::string dumpTimingStop() {

  std::string code = "";
  
  code += "refElapsed = stop_timer();\n"; 
  code += "printf(\"%f\\n\", refElapsed);\n";
  return code;
}

static std::string generate_code(std::string kernel, 
const std::vector<PetArray> &arrays) {

  std::string code = dumpHeaders();
  code += dumpDefines();
  code += dumpTimingUtilities();
  code += "\n int main(void) {\n\n";
  code += dumpArrayDecl(arrays);
  code += dumpArrayInit(arrays);
  code += dumpTimingStart();
  code += kernel;
  code += dumpTimingStop();
  code += "\n retunr 0; \n} \n";
  return code;
}

template <typename Iterator>
TileConfiguration run_jobs(
Iterator begin, Iterator end, const TileConfigurations cs, 
isl::schedule schedule, LoopTactics::LoopOptimizer &opt,
const std::vector<PetArray> &arrays) {

  for (auto it = begin; it != end; it++) {
    TileConfiguration c = *it;
    for (auto &s : c) {
      schedule = opt.tile(s.name_, s.value_, schedule); 
    }
    std::string code = generate_code(opt.code_gen(schedule), arrays);
    std::cout << code << std::endl;
    assert(0); 
  }

  return cs[0];
}


TileConfiguration Tuner::tune() {

  auto result = run_jobs(cs_.begin(), cs_.end(), 
                         cs_, current_schedule_, opt_,
                         arrays_);
  return result;
}
