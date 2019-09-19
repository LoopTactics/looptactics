#include "islutils/haystack_runner.h"
#include "haystack/HayStack.h"
#include <cassert>
#include <iostream>

const int CACHE_SIZE1 = 32 * 1024;
const int CACHE_SIZE2 = 512 * 1024;
const int CACHE_LINE_SIZE = 64;

HaystackRunner::HaystackRunner(QObject *parent, isl::ctx ctx) : 
  QThread(parent), context_(ctx) {

  restart_ = false;
  abort_ = false;
}

HaystackRunner::~HaystackRunner() {

  #ifdef DEBUG
    std::cout << __func__ << "\n";
  #endif
  
  mutex_.lock();
  abort_ = true;
  condition_.wakeOne();
  mutex_.unlock();
  
  wait();
}

void HaystackRunner::runModel(isl::schedule currentSchedule,
  const QString &filePath) {

  QMutexLocker locker(&mutex_);

  schedule_ = currentSchedule;
  filePath_ = filePath;
  
  if (!isRunning()) {
    start(LowPriority);
  }
  else {
    restart_ = true;
    condition_.wakeOne();
  }
}

void HaystackRunner::run() {

  while(1) {
    
    mutex_.lock();
    isl::schedule schedule = schedule_;
    QString filePath = filePath_;
    mutex_.unlock();

    if (abort_)
      return;

    machine_model MachineModel = {CACHE_LINE_SIZE, {CACHE_SIZE1, CACHE_SIZE2}};
    model_options ModelOptions = {true};
    HayStack Model(context_, MachineModel, ModelOptions);
    auto petScop = pet::Scop::parseFile(context_, filePath.toStdString()); 
    petScop.schedule() = schedule;
    
    std::cout << "curr schedule: " << schedule.to_str() << "\n";

    Model.compileProgram(petScop.get());
    Model.initModel();  
    auto cacheMisses = Model.countCacheMisses();
    // collect and print result
    long totalAccesses = 0;
    long totalCompulsory = 0;
    std::vector<long> totalCapacity(MachineModel.CacheSizes.size(), 0);
    // sum the cache misses for all accesses
    for (auto &cacheMiss : cacheMisses) {
      totalAccesses += cacheMiss.second.Total;
      totalCompulsory += cacheMiss.second.CompulsoryMisses;
      std::transform(
        totalCapacity.begin(), 
        totalCapacity.end(), 
        cacheMiss.second.CapacityMisses.begin(),
        totalCapacity.begin(), std::plus<long>());
    }
    auto stats = userFeedback::CacheStats{
      totalAccesses, totalCompulsory, {totalCapacity[0], totalCapacity[1]}};

    if (!restart_)
      Q_EMIT computedStats(stats);
    mutex_.lock();
    if (!restart_)
      condition_.wait(&mutex_);
    restart_ = false;
    mutex_.unlock();
  }
}
    






   
