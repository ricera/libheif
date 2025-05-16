/*
  libheif example application.

  MIT License

  Copyright (c) 2019 Dirk Farin <dirk.farin@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#include "encoder_y4m.h"

#include <cerrno>
#include <cstring>
#include <cassert>


Y4MEncoder::Y4MEncoder() = default;


void Y4MEncoder::UpdateDecodingOptions(const struct heif_image_handle* handle,
                                       struct heif_decoding_options* options) const
{
  options->convert_hdr_to_8bit = 0;
}


bool Y4MEncoder::Encode(const struct heif_image_handle* handle,
                        const struct heif_image* image,
                        const std::string& filename)
{
  FILE* fp = fopen(filename.c_str(), "wb");
  if (!fp) {
    fprintf(stderr, "Can't open %s: %s\n", filename.c_str(), strerror(errno));
    return false;
  }

  int y_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
  int cb_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cb);
  int cr_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cr);

  int bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);
  printf("Input luma bit depth: %d\n", bit_depth);
  printf("Encoding image with Y=%d, Cb=%d, Cr=%d bits per pixel\n", y_bpp, cb_bpp, cr_bpp);

  size_t y_stride, cb_stride, cr_stride;
  const uint8_t* yp = heif_image_get_plane_readonly2(image, heif_channel_Y, &y_stride);
  const uint8_t* cbp = heif_image_get_plane_readonly2(image, heif_channel_Cb, &cb_stride);
  const uint8_t* crp = heif_image_get_plane_readonly2(image, heif_channel_Cr, &cr_stride);

  assert(y_stride > 0);
  assert(cb_stride > 0);
  assert(cr_stride > 0);

  int yw = heif_image_get_width(image, heif_channel_Y);
  int yh = heif_image_get_height(image, heif_channel_Y);
  int cw = heif_image_get_width(image, heif_channel_Cb);
  int ch = heif_image_get_height(image, heif_channel_Cb);

  if (yw < 0 || cw < 0) {
    fclose(fp);
    return false;
  }

#if 0
  printf("y_stride: %llu\n", y_stride);
  printf("cb_stride: %llu\n", cb_stride);
  printf("cr_stride: %llu\n", cr_stride);

  printf("Y width: %d\n", yw);
  printf("Y height: %d\n", yh);
  printf("C width: %d\n", cw);
  printf("C height: %d\n", ch);
#endif

  /* If 10-bit image output, use P010 format as output */
  if (y_bpp == 10) {
    printf("Output in P010 YUV format\n");

#if 0
    printf("First 8 bytes in Y: %4x %4x\n", ((uint16_t *)yp)[0], ((uint16_t *)yp)[1]);
    printf("                    %4x %4x\n", ((uint16_t *)yp)[2], ((uint16_t *)yp)[3]);
  
    printf("First 8 bytes in Cb: %4x %4x\n", ((uint16_t *)cbp)[0], ((uint16_t *)cbp)[1]);
    printf("                     %4x %4x\n", ((uint16_t *)cbp)[2], ((uint16_t *)cbp)[3]);
  
    printf("First 8 bytes in Cr: %4x %4x\n", ((uint16_t *)crp)[0], ((uint16_t *)crp)[1]);
    printf("                     %4x %4x\n", ((uint16_t *)crp)[2], ((uint16_t *)crp)[3]);
#endif

    /* FIXME: drop header for now */
    // fprintf(fp, "YUV4MPEG2 W%d H%d F30:1 C420p10\nFRAME\n", yw, yh);

    const uint16_t* yp_16 = (const uint16_t *)yp;
    const uint16_t* cbp_16 = (const uint16_t *)cbp;
    const uint16_t* crp_16 = (const uint16_t *)crp;

    /* In P010, values are encoded in the 10 most significant bits, so the decoded plane cannot
     * be written out to the output file as-is, unlike in 8-bit YUV420 below
     */
    for (int y = 0; y < yh; y++) {
      for (int z = 0; z < yw; z++) {
        uint16_t word = *(yp_16 + z + (y * yw));
        word = (word << 6); // Little Endian
        fwrite(&word, 2, 1, fp);
      }
    }
  
    /* The U and V planes are interleaved in P010;
     * U == Cb, and V == Cr
     */
    for (int y = 0; y < ch; y++) {
      for (int z = 0; z < cw; z++) {
        uint16_t word = *(cbp_16 + z + (y * cw));
        word = (word << 6); // Little Endian
        fwrite(&word, 2, 1, fp);
      
        word = *(crp_16 + z + (y * cw));
        word = (word << 6); // Little Endian
        fwrite(&word, 2, 1, fp);
      }
    }
  } else {
    /* Encode as 8-bit image in C420 */
#if 0
    printf("First 8 bytes in Y: %2x %2x %2x %2x\n", yp[0], yp[1], yp[2], yp[3]);
    printf("                    %2x %2x %2x %2x\n", yp[4], yp[5], yp[6], yp[7]);
  
    printf("First 8 bytes in Cb: %2x %2x %2x %2x\n", cbp[0], cbp[1], cbp[2], cbp[3]);
    printf("                     %2x %2x %2x %2x\n", cbp[4], cbp[5], cbp[6], cbp[7]);
  
    printf("First 8 bytes in Cr: %2x %2x %2x %2x\n", crp[0], crp[1], crp[2], crp[3]);
    printf("                     %2x %2x %2x %2x\n", crp[4], crp[5], crp[6], crp[7]);
#endif
    fprintf(fp, "YUV4MPEG2 W%d H%d F30:1 C420\nFRAME\n", yw, yh);

    for (int y = 0; y < yh; y++) {
      fwrite(yp + y * y_stride, 1, yw, fp);
    }

    for (int y = 0; y < ch; y++) {
      fwrite(cbp + y * cb_stride, 1, cw, fp);
    }

    for (int y = 0; y < ch; y++) {
      fwrite(crp + y * cr_stride, 1, cw, fp);
    }
  }

  fclose(fp);

  return true;
}
