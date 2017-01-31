/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <ext/optional>
#include <graphics/Format/PDF.h>
#include <iterator>

using graphics::Format::PDF;

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

std::pair<long int, long int> PDF::
Read(std::vector<uint8_t> const &contents)
{
    CGDataProviderRef dpr = CGDataProviderCreateWithData(
        NULL, contents.data(), contents.size() * sizeof(uint8_t), NULL);
    CGPDFDocumentRef pdfdr = CGPDFDocumentCreateWithProvider(dpr);

    size_t numPages = CGPDFDocumentGetNumberOfPages(pdfdr);
    CGRect resultRect;
    for (size_t i = 1; i <= numPages; i++) {
        CGPDFPageRef page = CGPDFDocumentGetPage(pdfdr, i);
        CGRect rect = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);
        resultRect.size.width = std::max(resultRect.size.width, rect.size.width);
        resultRect.size.height = std::max(resultRect.size.height, rect.size.height);
    }
    CGPDFDocumentRelease(pdfdr);
    CGDataProviderRelease(dpr);
    return std::make_pair(resultRect.size.width, resultRect.size.height);
}

#else

long int PDF::Read(std::vector<uint8_t> const &contents)
{
    // NYI
}

#endif
