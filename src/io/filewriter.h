#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_FILEWRITER
#define NODE_TRANSCODING_IO_FILEWRITER

using namespace v8;

namespace transcoding {
namespace io {

class FileWriter : public IOWriter {
public:
  FileWriter(Handle<Object> source);
  virtual ~FileWriter();

  virtual int Open();
  virtual void Close();

public:
  std::string     path;
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_FILEWRITER
