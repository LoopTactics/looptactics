#include "islutils/tuner_thread.h"
#include "islutils/pet_wrapper.h"
#include <iostream> // std::cout
#include <sstream>  // std::ostringstream
#include <fstream>  // std::ofstream
#include <thread>   // std::thread
#include <cassert>
#include "pstream.h"

#define COMPILER "clang -O3 "
#define RUNS 3

using namespace pet;
using namespace timeInfo;

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
  return code;
}


static std::string insertConstantDecl(const PetArray &array) {

  std::string code;

  std::string type = "unknown";
  if (array.type() == TypeElement::FLOAT)
    type = "float";
  else 
    type = "double";

  code += type + " " + array.name() + ";" + "\n";  
  return code;
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

  std::string code{};

  code = array.name() + " = 1.5;\n";
  return code;
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
  code += "\n return 0; \n} \n";
  return code;
}

static TimingInfo execute_job(const std::string thread_id) {

  std::vector<double> results;
  for (size_t i = 0; i < RUNS; i++) {
    
    redi::ipstream exec("./" + thread_id);
    std::string execution_time{};   
    exec >> execution_time; 
    if (execution_time.empty()) {
      std::cout << kMessageFailure << "\n";
      continue;
    }
    else {
      std::cout << kMessageOK  << "\n";
      std::cout << kMessageVerbose << "[ " << execution_time << " ]" << "\n";
      results.push_back(std::stod(execution_time));  
    }
  }

  auto pos = std::min_element(results.begin(), results.end());  
  auto min = results[std::distance(results.begin(), pos)];

  pos = std::max_element(results.begin(), results.end());
  auto max = results[std::distance(results.begin(), pos)];

  auto avg = 
    std::accumulate(results.begin(), results.end(), 0.0)/results.size();

  auto median = 0.0;
  
  TimingInfo t{min, max, avg, median};
  return t; 
}

// the retunr is the file id.
static std::string compile_job(const std::string &code) {

  // get thread id
  std::ostringstream ss;
  ss << std::this_thread::get_id();
  std::string thread_id = ss.str();
  std::string thread_file_id = thread_id + ".c";

  // write to file
  std::ofstream file;
  file.open(thread_file_id);
  file << code;
  file.close();

  // compile (TODO: maybe jit compilation??)
  redi::ipstream in(COMPILER
    + thread_file_id + " -o " + thread_id);
  // need to sleep to give time to clang to compile. 
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);

  return thread_id;
}

TunerThread::TunerThread(QObject *parent, isl::ctx ctx) :
  QThread(parent), context_(ctx) {

  restart_ = false;
  abort_ = false;

}

TunerThread::~TunerThread() {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif

  mutex_.lock();
  abort_ = true;
  condition_.wakeOne();
  mutex_.unlock();

  wait();
}

void TunerThread::run() {
  
  while(1) {

    mutex_.lock();
    isl::schedule opt_schedule = opt_schedule_;
    isl::schedule baseline_schedule = baseline_schedule_;
    QString file_path = file_path_;
    mutex_.unlock();

    if (abort_)
      return;

    std::string file_path_as_std_string =
      file_path.toStdString();
    assert(!file_path_as_std_string.empty()
      && "file path cannot be empty!");
    auto scop =
      pet::Scop(pet::Scop::parseFile(context_, file_path_as_std_string)); 
    auto arrays = scop.arrays();

    scop.schedule() = baseline_schedule;
    auto baseline_code = generate_code(scop.codegen(), arrays);
    
    scop.schedule() = opt_schedule;
    auto opt_code = generate_code(scop.codegen(), arrays);
    
    auto exec_id = compile_job(baseline_code);
    auto baseline_time = execute_job(exec_id);

    exec_id = compile_job(opt_code);
    auto opt_time = execute_job(exec_id);

    redi::ipstream clean("rm " + exec_id);

    if (!restart_)
      Q_EMIT compareSchedules(baseline_time, opt_time);
  
    mutex_.lock();
    if (!restart_)
      condition_.wait(&mutex_);
    restart_= false;
    mutex_.unlock();
  }
}

void TunerThread::compare(const isl::schedule baseline_schedule,
  const isl::schedule opt_schedule, const QString &file_path) {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif 
  QMutexLocker locker(&mutex_);
  
  baseline_schedule_ = baseline_schedule;
  opt_schedule_ = opt_schedule;
  file_path_ = file_path;

  if (!isRunning()) {
    start(LowPriority);
  }
  else {
    restart_ = true;
    condition_.wakeOne();
  }
}  
