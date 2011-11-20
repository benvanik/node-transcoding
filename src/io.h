#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODE_IO
#define NODE_TRANSCODE_IO

using namespace v8;

namespace transcode {

class IODescriptor {
public:
  IODescriptor(Handle<Object> source);
  virtual ~IODescriptor();

public:
  char* filename;
};

class InputDescriptor : public IODescriptor {
public:
  InputDescriptor(Handle<Object> source);
  virtual ~InputDescriptor();
};

class OutputDescriptor : public IODescriptor {
public:
  OutputDescriptor(Handle<Object> target);
  virtual ~OutputDescriptor();
};

AVFormatContext* createInputContext(InputDescriptor* descr, int* pret);
AVFormatContext* createOutputContext(OutputDescriptor* descr, int* pret);
void cleanupContext(AVFormatContext* ctx);

}; // transcode

#endif // NODE_TRANSCODE_IO
