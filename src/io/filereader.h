#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_FILEREADER
#define NODE_TRANSCODING_IO_FILEREADER

using namespace v8;

namespace transcoding {
namespace io {

class FileReader : public IOReader {
public:
  FileReader(Handle<Object> source);
  virtual ~FileReader();

  virtual int Open();

public:
  std::string     path;
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_FILEREADER
