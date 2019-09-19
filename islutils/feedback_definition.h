#ifndef TIMING_INFO_H
#define TIMING_INFO_H

#include <limits>

namespace userFeedback {

struct TimingInfo {

  double min_time = std::numeric_limits<double>::min();
  double max_time = std::numeric_limits<double>::max();
  double avg_time = 0.0;
  double median_time = 0.0;
};

struct CacheStats {

  long totalAccesses = 0;
  long compulsory = 0;
  std::vector<long> capacity{};
};

} // end namespace useFeedback

Q_DECLARE_METATYPE(userFeedback::TimingInfo)
Q_DECLARE_METATYPE(userFeedback::CacheStats)

#endif
