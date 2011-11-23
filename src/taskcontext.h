#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io.h"
#include "profile.h"

#ifndef NODE_TRANSCODING_TASKCONTEXT
#define NODE_TRANSCODING_TASKCONTEXT

using namespace v8;

namespace transcoding {

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

  void Abort();

  // Occurs exclusively in the v8 thread
  int Prepare();
  AVStream* AddOutputStreamCopy(AVFormatContext* octx, AVStream* istream,
      int* pret);

  // Occurs exclusively in a worker thread
  bool Pump(int* pret, Progress* progress);
  void End();

public:
  pthread_mutex_t     lock;

  bool                running;
  bool                abort;
  int                 err;

  IOHandle*           input;
  IOHandle*           output;
  Profile*            profile;
  // options

  AVFormatContext*    ictx;
  AVFormatContext*    octx;
};

}; // transcoding

#endif // NODE_TRANSCODING_TASKCONTEXT
