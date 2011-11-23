#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io.h"
#include "profile.h"

#ifndef NODE_TRANSCODE_TASKCONTEXT
#define NODE_TRANSCODE_TASKCONTEXT

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

class TaskContext {
public:
  TaskContext(IOHandle* input, IOHandle* output, Profile* profile);
  ~TaskContext();

  Progress GetProgress();
  void Abort();

  // Occurs exclusively in the v8 thread
  int Prepare();

  // Occurs exclusively in a worker thread
  bool Pump(int *pret);

public:
  pthread_mutex_t     lock;

  bool                running;
  bool                abort;
  int                 err;

  IOHandle*           input;
  IOHandle*           output;
  Profile*            profile;
  // options

  Progress            progress;

  AVFormatContext*    ictx;
  AVFormatContext*    octx;
};

}; // transcode

#endif // NODE_TRANSCODE_TASKCONTEXT
