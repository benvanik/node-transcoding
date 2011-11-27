#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_NULLWRITER
#define NODE_TRANSCODING_IO_NULLWRITER

using namespace v8;

namespace transcoding {
namespace io {

class NullWriter : public IOWriter {
public:
  NullWriter();
  virtual ~NullWriter();

  virtual int Open();
  virtual void Close();

public:
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_NULLWRITER
