#ifndef TIMING_INFO_H
#define TIMING_INFO_H

#include <limits>

// FIXME: be consistent while naming namespace


namespace timeInfo {
struct TimingInfo {

  double min_time = std::numeric_limits<double>::min();
  double max_time = std::numeric_limits<double>::max();
  double avg_time = 0.0;
  double median_time = 0.0;
};
} // end namespace timeInfo
Q_DECLARE_METATYPE(timeInfo::TimingInfo)

#endif
