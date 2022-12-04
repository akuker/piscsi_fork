
//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "test/linux_os_stubs.h"
#include "test/test_shared.h"

#include <filesystem>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

using namespace std;
using namespace filesystem;

extern "C" {

#ifdef __USE_LARGEFILE64
FILE *__wrap_fopen64(const char *__restrict __filename, const char *__restrict __modes)
#else
FILE *__wrap_fopen(const char *__restrict __filename, const char *__restrict __modes)
#endif
{
    path new_filename;
    bool create_directory = false;

    // If we're trying to open up the device tree soc ranges,
    // re-direct it to a temporary local file.
    if (string(__filename) == "/proc/device-tree/soc/ranges") {
        create_directory = true;
        new_filename     = test_data_temp_path;
        new_filename += path(__filename);
    } else {
        new_filename = path(__filename);
    }

    if (create_directory) {
        create_directories(new_filename.parent_path());
    }
#ifdef __USE_LARGEFILE64
    return __real_fopen64(new_filename.c_str(), __modes);
#else
    return __real_fopen(new_filename.c_str(), __modes);
#endif
}

} // end extern "C"