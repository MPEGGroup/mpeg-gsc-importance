/* The copyright in this software is being made available under the BSD
 * Licence, included below.  This software may be subject to other third
 * party and contributor rights, including patent rights, and no such
 * rights are granted under this licence.
 *
 * Copyright (c) 2025, ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the ISO/IEC nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "metric.hpp"
#include "pointcloud.hpp"
#include "image.hpp"

#include "xSSIM.h"
#include "xColorSpace.h"

Metric::Metric() {}

Metric::~Metric() { results_.clear(); }

inline glm::dvec3 rgbToYuvBt709( const glm::dvec3& rgb ) {
  return glm::vec3( 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2],
                    -0.1146f * rgb[0] - 0.3854f * rgb[1] + 0.5000f * rgb[2] + 128.0f,
                    0.5000f * rgb[0] - 0.4542f * rgb[1] - 0.0458f * rgb[2] + 128.0f );
}
inline glm::dvec3 log10( const glm::dvec3& v ) {
  return glm::dvec3( std::log10( v.x ), std::log10( v.y ), std::log10( v.z ) );
};

template <typename T, int32_t N>
void Metric::compute( const Image<T, N>& imageA, const Image<T, N>& imageB ) {
  Results res;
  if ( imageA.size() != imageB.size() ) {
    printf( "Error: Images are not the same size\n" );
    return;
  } else {
    double peak = 1.0;
    if constexpr ( std::is_integral_v<T> ) {
      peak = static_cast<double>( std::numeric_limits<T>::max() );
    } else if constexpr ( std::is_floating_point_v<T> ) {
      peak = 1.0;
    }
    const auto size = imageA.size();
    for ( int32_t i = 0; i < size; i++ ) {
      const glm::dvec3 rgb = imageA.get( i ) - imageB.get( i );
      const glm::dvec3 yuv = rgbToYuvBt709( imageA.get( i ) ) - rgbToYuvBt709( imageB.get( i ) );
      res.rgbMSE_ += rgb * rgb;
      res.yuvMSE_ += yuv * yuv;
    }
    res.rgbPSNR_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak * peak ) / ( res.rgbMSE_ / (double)size ) ) );
    res.yuvPSNR_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak * peak ) / ( res.yuvMSE_ / (double)size ) ) );

    computeIVSSIM_(res, imageA, imageB);
  }
  results_.push_back(res);
}

template <typename T, int32_t N> void convertBuffer(PMBB::xPicP& Dst, const Image<T, N>& Src)
{
  using namespace PMBB;

  const int32 Width     = Dst.getWidth   ();
  const int32 Height    = Dst.getHeight  ();
  const int32 BitDepth  = Dst.getBitDepth();
  const int32 DstStride = Dst.getStride  ();

  for(int32 c = 0; c < 3; c++)
  {
    const Plane<T>& SrcPlane = Src.plane(c);
    uint16*         DstPtr   = Dst.getAddr(eCmp(c));
    
    for(int32 y = 0; y < Height; y++)
    {
      for(int32 x = 0; x < Width; x++)
      {
        T SrcA = SrcPlane.get(x, y);
        if constexpr(std::is_same_v<T, uint8_t > || std::is_same_v<T, uint16_t>)
        {
          DstPtr[x] = SrcA;
        }
        else if constexpr(std::is_same_v<T, float>)
        {
          DstPtr[x] = round(SrcA * xBitDepth2MaxValue(BitDepth));
        }
      }
      DstPtr += DstStride;
    }
  }
}

template <typename T, int32_t N> void Metric::computeIVSSIM_(Results& Result, const Image<T, N>& imageA, const Image<T, N>& imageB)
{
  using namespace PMBB;

  const int32 Width  = imageA.width ();
  const int32 Height = imageA.height();

  int32 BitDepth = NOT_VALID;
  if      constexpr (std::is_same_v<T, uint8_t >) { BitDepth = 8 ; }
  else if constexpr (std::is_same_v<T, uint16_t>) { BitDepth = 16; }
  else if constexpr (std::is_same_v<T, float   >) { BitDepth = 16; }
  else { static_assert(false, "Unsupported Image<T, N> type"); }

  static_assert(N==3, "Unsupported number of components");

  //create PMBB image buffers
  xPicP PicA({ Width, Height }, BitDepth, 4);
  xPicP PicB({ Width, Height }, BitDepth, 4);

  //create IVSSIM calculator
  xIVSSIM ProcIVSSIM;
  ProcIVSSIM.create({ Width, Height }, BitDepth, 4, false);
  ProcIVSSIM.createThrdPoolIntf(nullptr, 0); //create empty thread pool interface - don't use parallel processing here
  ProcIVSSIM.setStructSimParams(xSSIM::eMode::BlockAveraged, false, 8, 4);
  ProcIVSSIM.initRowBuffers(Height);

  //copy/convert data from Image<T, N> to xPicP
  convertBuffer<T, N>(PicA, imageA);
  assert(PicA.check("imageA"));
  PicA.extend();

  convertBuffer<T, N>(PicB, imageB);
  assert(PicB.check("imageB"));
  PicB.extend();

  //calculate RGB IV-SSIM
  double IVSSIM_RGB = ProcIVSSIM.calcPicIVSSIM(&PicA, &PicB);

  //convert colorspace (in-place)
  xColorSpace::ConvertRGB2YCbCr(PicA.getAddr(eCmp::LM), PicA.getAddr(eCmp::CB), PicA.getAddr(eCmp::CR), PicA.getAddr(eCmp::R ), PicA.getAddr(eCmp::G ), PicA.getAddr(eCmp::B ),
                                PicA.getStride(), PicA.getStride(), PicA.getWidth(), PicA.getHeight(), PicA.getBitDepth(), eClrSpcLC::BT709);
  PicA.extend();

  xColorSpace::ConvertRGB2YCbCr(PicB.getAddr(eCmp::LM), PicB.getAddr(eCmp::CB), PicB.getAddr(eCmp::CR), PicB.getAddr(eCmp::R ), PicB.getAddr(eCmp::G ), PicB.getAddr(eCmp::B ),
                                PicB.getStride(), PicB.getStride(), PicB.getWidth(), PicB.getHeight(), PicB.getBitDepth(), eClrSpcLC::BT709);
  PicB.extend();

  //calculate YCbCr IV-SSIM
  double IVSSIM_YCbCr = ProcIVSSIM.calcPicIVSSIM(&PicA, &PicB);

  //store results
  Result.rgbIVSSIM_ = IVSSIM_RGB  ;
  Result.yuvIVSSIM_ = IVSSIM_YCbCr;

  //cleanup
  ProcIVSSIM.destroy();
}

Results Metric::getAverage() {
  Results res;
  for ( auto& el : results_ ) res += el;
  res /= results_.size();
  return res;
}

template void Metric::compute<uint8_t>( const Image<uint8_t>&, const Image<uint8_t>& );
template void Metric::compute<uint16_t>( const Image<uint16_t>&, const Image<uint16_t>& );
template void Metric::compute<float>( const Image<float>&, const Image<float>& );