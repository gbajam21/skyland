#pragma once


#include "number/endian.hpp"
#include "number/int.h"



namespace filesystem {



struct Root {
    char magic_[4];
    host_u32 file_count_;
};



struct FileHeader {
    char path_[64]; // Must be null-terminated.
    host_u32 size_;
};



using NullTerminatedString = const char*;
using FileContents = NullTerminatedString;
using FilePath = NullTerminatedString;



bool is_mounted();



FileContents load(FilePath path);



} // namespace filesystem
