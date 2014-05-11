// Copyright 2013 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// TODO(skal): implement gradient smoothing.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "./quant_levels_dec.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

int DequantizeLevels(uint8_t* const data, int width, int height,
                     int row, int num_rows) {
  if (data == NULL || width <= 0 || height <= 0 || row < 0 || num_rows < 0 ||
      row + num_rows > height) {
    return 0;
  }
  return 1;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
