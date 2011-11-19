#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODE_PROCESSOR
#define NODE_TRANSCODE_PROCESSOR

using namespace v8;

namespace transcode {

typedef struct Progress_t {
  double    timestamp;
  double    duration;
  double    timeElapsed;
  double    timeEstimated;
  double    timeRemaining;
  double    timeMultiplier;
} Progress;

int beginProcessing();

}; // transcode

#endif // NODE_TRANSCODE_PROCESSOR
