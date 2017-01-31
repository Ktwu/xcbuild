/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef __graphics_Format_PDF_h
#define __graphics_Format_PDF_h

#include <ext/optional>
#include <graphics/PixelFormat.h>
#include <string>
#include <utility>
#include <vector>

namespace graphics {
namespace Format {

/*
 * Utilities for PDF images.
 */
class PDF {
private:
    PDF();
    ~PDF();

public:
    /*
     * Read a PDF image.
     */
    static std::pair<long int, long int>
    Read(std::vector<uint8_t> const &contents);

public:
    /*
     * Write a PDF image.
     */
};

}
}

#endif // !__graphics_Format_PDF_h
