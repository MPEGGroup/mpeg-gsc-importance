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
#include <vector>
#include <cmath>
#include <algorithm>

#include "xSSIM.h"
#include "xColorSpace.h"

Metric::Metric() : computeOmMetrics_( true ), computeSsimMetrics_( true ) {}

Metric::~Metric() { results_.clear(); }

inline glm::dvec3 rgbToYuvBt709( const glm::dvec3& rgb ) {
  return glm::vec3( 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2],
                    -0.1146f * rgb[0] - 0.3854f * rgb[1] + 0.5000f * rgb[2] + 128.0f,
                    0.5000f * rgb[0] - 0.4542f * rgb[1] - 0.0458f * rgb[2] + 128.0f );
}
inline glm::dvec3 log10( const glm::dvec3& v ) {
  return glm::dvec3( std::log10( v.x ), std::log10( v.y ), std::log10( v.z ) );
};

// Generate Gaussian window
std::vector<std::vector<double>> generate_gaussian_window( int window_size, double sigma ) {
  std::vector<std::vector<double>> window( window_size, std::vector<double>( window_size ) );
  double                           sum       = 0.0;
  int                              half_size = window_size / 2;

  for ( int i = -half_size; i <= half_size; i++ ) {
    for ( int j = -half_size; j <= half_size; j++ ) {
      double value                         = std::exp( -( i * i + j * j ) / ( 2 * sigma * sigma ) );
      window[i + half_size][j + half_size] = value;
      sum += value;
    }
  }

  // Normalize
  for ( auto& row : window ) {
    for ( auto& val : row ) { val /= sum; }
  }

  return window;
}

// Implementation of compute function with mask
template <typename T, int32_t N>
void Metric::compute( const Image<T, N>& imageA, const Image<T, N>& imageB, const Image<uint8_t, 1>& mask ) {
  Results res;
  if ( imageA.size() != imageB.size() || imageA.size() != mask.size() ) {
    printf( "Error: Images or mask are not the same size\n" );
    return;
  } else {
    double peak = 1.0;
    if constexpr ( std::is_integral_v<T> ) {
      peak = static_cast<double>( std::numeric_limits<T>::max() );
    } else if constexpr ( std::is_floating_point_v<T> ) {
      peak = 1.0;
    }
    const auto size   = imageA.size();
    const int  width  = imageA.width();
    const int  height = imageA.height();

    // Initialize statistical variables
    int32_t    validPixelCount = 0;
    glm::dvec3 mseRgbOm        = { 0.0, 0.0, 0.0 };
    glm::dvec3 mseYuvOm        = { 0.0, 0.0, 0.0 };

    // Create YUV images
    Plane<double> yuvA[3], yuvB[3];
    for ( int i = 0; i < 3; i++ ) {
      yuvA[i].resize( width, height );
      yuvB[i].resize( width, height );
    }

    // Convert to YUV and count valid pixels
    for ( int32_t y = 0; y < height; y++ ) {
      for ( int32_t x = 0; x < width; x++ ) {
        int32_t          index    = y * width + x;
        const glm::dvec3 rgbA     = imageA.get( index );
        const glm::dvec3 rgbB     = imageB.get( index );
        const glm::dvec3 yuvA_val = rgbToYuvBt709( rgbA );
        const glm::dvec3 yuvB_val = rgbToYuvBt709( rgbB );

        for ( int c = 0; c < 3; c++ ) {
          yuvA[c].set( x, y, yuvA_val[c] );
          yuvB[c].set( x, y, yuvB_val[c] );
        }

        // Use mask or automatically detect valid pixels
        bool isValid = ( mask.plane( 0 ).get( x, y ) != 0 );

        if ( isValid ) {
          validPixelCount++;

          // Compute RGB difference
          const glm::dvec3 rgb = rgbA - rgbB;
          mseRgbOm += rgb * rgb;

          // Compute YUV difference
          const glm::dvec3 yuv = yuvA_val - yuvB_val;
          mseYuvOm += yuv * yuv;
        }
      }
    }
    // Compute valid pixel ratio
    res.occupancyRate_ = ( validPixelCount > 0 ) ? ( 100.0 * validPixelCount / size ) : 0.0;

    // Compute OM-PSNR
    if ( computeOmMetrics_ ) {
      if ( validPixelCount > 0 ) {
        res.mseRgbOm_  = mseRgbOm / static_cast<double>( validPixelCount );
        res.psnrRgbOm_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak * peak ) / ( res.mseRgbOm_ ) ) );

        res.mseYuvOm_  = mseYuvOm / static_cast<double>( validPixelCount );
        res.psnrYuvOm_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak * peak ) / ( res.mseYuvOm_ ) ) );

        // Compute RGB combined channel OM-PSNR (1:1:1)
        double mseRgbOmAvg = ( res.mseRgbOm_.x + res.mseRgbOm_.y + res.mseRgbOm_.z ) / 3.0;
        res.psnrRgbOmAvg_ =
            ( mseRgbOmAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak * peak ) / mseRgbOmAvg ) ) : 999.99;

        // Compute YUV combined channel OM-PSNR (6:1:1)
        double mseYuvOmAvg = ( 6.0 * res.mseYuvOm_.x + res.mseYuvOm_.y + res.mseYuvOm_.z ) / 8.0;
        res.psnrYuvOmAvg_ =
            ( mseYuvOmAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak * peak ) / mseYuvOmAvg ) ) : 999.99;
      } else {
        // Handle case with no valid pixels
        res.mseRgbOm_     = glm::dvec3( 0.0 );
        res.psnrRgbOm_    = glm::dvec3( 999.99 );
        res.mseYuvOm_     = glm::dvec3( 0.0 );
        res.psnrYuvOm_    = glm::dvec3( 999.99 );
        res.psnrRgbOmAvg_ = 999.99;
        res.psnrYuvOmAvg_ = 999.99;
      }
    }

    // Original PSNR computation (full image)
    for ( int32_t y = 0; y < height; y++ ) {
      for ( int32_t x = 0; x < width; x++ ) {
        int32_t          index = y * width + x;
        const glm::dvec3 rgb   = imageA.get( index ) - imageB.get( index );
        const glm::dvec3 yuv   = rgbToYuvBt709( imageA.get( index ) ) - rgbToYuvBt709( imageB.get( index ) );
        res.mseRgb_ += rgb * rgb;
        res.mseYuv_ += yuv * yuv;
      }
    }
    res.mseRgb_ /= size;
    res.mseYuv_ /= size;
    res.psnrRgb_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak * peak ) / res.mseRgb_ ) );
    res.psnrYuv_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak * peak ) / res.mseYuv_ ) );

    // Compute RGB combined channel PSNR (1:1:1)
    double mseRgbAvg = ( res.mseRgb_.x + res.mseRgb_.y + res.mseRgb_.z ) / 3.0;
    res.psnrRgbAvg_ = ( mseRgbAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak * peak ) / mseRgbAvg ) ) : 999.99;

    // Compute YUV combined channel PSNR (6:1:1)
    double mseYuvAvg = ( 6.0 * res.mseYuv_.x + res.mseYuv_.y + res.mseYuv_.z ) / 8.0;
    res.psnrYuvAvg_ = ( mseYuvAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak * peak ) / mseYuvAvg ) ) : 999.99;

    // Compute SSIM metrics (if needed)
    if ( computeSsimMetrics_ ) {
      // Compute SSIM and OM-SSIM for each channel
      for ( int c = 0; c < 3; c++ ) {
        // Standard SSIM (full image)
        res.ssim_[c] = computeSSIM( yuvA[c], yuvB[c] );

        // OM-SSIM (only valid pixel regions) - if needed
        if ( computeOmMetrics_ ) { res.ssimOm_[c] = computeMaskedSSIM( yuvA[c], yuvB[c], mask.plane( 0 ) ); }
      }

      // Compute weighted average (Y:4, U:1, V:1)
      res.ssimAvg_ = ( 4.0 * res.ssim_[0] + res.ssim_[1] + res.ssim_[2] ) / 6.0;
      if ( computeOmMetrics_ ) { res.ssimOmAvg_ = ( 4.0 * res.ssimOm_[0] + res.ssimOm_[1] + res.ssimOm_[2] ) / 6.0; }
    }
    computeIVSSIM( res, imageA, imageB );
  }
  results_.push_back( res );
}

template <typename T, int32_t N>
void convertBuffer( PMBB::xPicP& Dst, const Image<T, N>& Src ) {
  using namespace PMBB;

  const int32 Width     = Dst.getWidth();
  const int32 Height    = Dst.getHeight();
  const int32 BitDepth  = Dst.getBitDepth();
  const int32 DstStride = Dst.getStride();

  for ( int32 c = 0; c < 3; c++ ) {
    const Plane<T>& SrcPlane = Src.plane( c );
    uint16*         DstPtr   = Dst.getAddr( eCmp( c ) );

    for ( int32 y = 0; y < Height; y++ ) {
      for ( int32 x = 0; x < Width; x++ ) {
        T SrcA = SrcPlane.get( x, y );
        if constexpr ( std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ) {
          DstPtr[x] = SrcA;
        } else if constexpr ( std::is_same_v<T, float> ) {
          DstPtr[x] = round( SrcA * xBitDepth2MaxValue( BitDepth ) );
        }
      }
      DstPtr += DstStride;
    }
  }
}

template <typename T, int32_t N>
void Metric::computeIVSSIM( Results& Result, const Image<T, N>& imageA, const Image<T, N>& imageB ) {
  using namespace PMBB;

  const int32 width  = imageA.width();
  const int32 height = imageA.height();

  int32 bitDepth = NOT_VALID;
  if constexpr ( std::is_same_v<T, uint8_t> ) {
    bitDepth = 8;
  } else if constexpr ( std::is_same_v<T, uint16_t> ) {
    bitDepth = 16;
  } else if constexpr ( std::is_same_v<T, float> ) {
    bitDepth = 16;
  } else {
    static_assert( false, "Unsupported Image<T, N> type" );
  }

  static_assert( N == 3, "Unsupported number of components" );

  // create PMBB image buffers
  xPicP picA( { width, height }, bitDepth, 4 );
  xPicP picB( { width, height }, bitDepth, 4 );

  // create IVSSIM calculator
  xIVSSIM procIvSsim;
  procIvSsim.create( { width, height }, bitDepth, 4, false );
  procIvSsim.createThrdPoolIntf( nullptr,
                                 0 );  // create empty thread pool interface - don't use parallel processing here
  procIvSsim.setStructSimParams( xSSIM::eMode::BlockAveraged, false, 8, 4 );
  procIvSsim.initRowBuffers( height );

  // copy/convert data from Image<T, N> to xPicP
  convertBuffer<T, N>( picA, imageA );
  assert( picA.check( "imageA" ) );
  picA.extend();

  convertBuffer<T, N>( picB, imageB );
  assert( picB.check( "imageB" ) );
  picB.extend();

  // calculate RGB IV-SSIM
  procIvSsim.setCmpWeightsSearch( xIVSSIM::c_EqualCmpWeights );
  procIvSsim.setCmpWeightsAverage( xIVSSIM::c_EqualCmpWeights );
  double rgbIvSsim = procIvSsim.calcPicIVSSIM( &picA, &picB );

  // convert colorspace (in-place)
  xColorSpace::ConvertRGB2YCbCr( picA.getAddr( eCmp::LM ),  //
                                 picA.getAddr( eCmp::CB ),  //
                                 picA.getAddr( eCmp::CR ),  //
                                 picA.getAddr( eCmp::R ),   //
                                 picA.getAddr( eCmp::G ),   //
                                 picA.getAddr( eCmp::B ),   //
                                 picA.getStride(),          //
                                 picA.getStride(),          //
                                 picA.getWidth(),           //
                                 picA.getHeight(),          //
                                 picA.getBitDepth(),        //
                                 eClrSpcLC::BT709 );
  picA.extend();

  xColorSpace::ConvertRGB2YCbCr( picB.getAddr( eCmp::LM ),  //
                                 picB.getAddr( eCmp::CB ),  //
                                 picB.getAddr( eCmp::CR ),  //
                                 picB.getAddr( eCmp::R ),   //
                                 picB.getAddr( eCmp::G ),   //
                                 picB.getAddr( eCmp::B ),   //
                                 picB.getStride(),          //
                                 picB.getStride(),          //
                                 picB.getWidth(),           //
                                 picB.getHeight(),          //
                                 picB.getBitDepth(),        //
                                 eClrSpcLC::BT709 );
  picB.extend();

  // calculate YCbCr IV-SSIM
  procIvSsim.setCmpWeightsSearch( xIVSSIM::c_DefaultCmpWeights );
  procIvSsim.setCmpWeightsAverage( xIVSSIM::c_DefaultCmpWeights );
  double ycbcrIvSsim = procIvSsim.calcPicIVSSIM( &picA, &picB );

  // store results
  Result.rgbIvSsim_ = rgbIvSsim;
  Result.yuvIvSsim_ = ycbcrIvSsim;

  // cleanup
  procIvSsim.destroy();
}

// Standard SSIM computation function
double Metric::computeSSIM( const Plane<double>& img1, const Plane<double>& img2 ) const {
  const int    width       = img1.width();
  const int    height      = img1.height();
  const int    window_size = 11;
  const double K1          = 0.01;
  const double K2          = 0.03;
  const double L           = 255.0;
  const double C1          = ( K1 * L ) * ( K1 * L );
  const double C2          = ( K2 * L ) * ( K2 * L );

  // Initialize Gaussian window (if needed)
  if ( gaussian_window_.empty() || window_size_ != window_size || gaussian_sigma_ != 1.5 ) {
    window_size_     = window_size;
    gaussian_sigma_  = 1.5;
    gaussian_window_ = generate_gaussian_window( window_size, gaussian_sigma_ );
  }

  double    ssim_total    = 0.0;
  int       valid_windows = 0;
  const int half_win      = window_size / 2;

  // Create padded images
  Plane<double> padded_img1, padded_img2;
  padded_img1.resize( width + 2 * half_win, height + 2 * half_win, 0.0 );
  padded_img2.resize( width + 2 * half_win, height + 2 * half_win, 0.0 );

  //  Copy original images and perform edge padding
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      padded_img1.set( x + half_win, y + half_win, img1.get( x, y ) );
      padded_img2.set( x + half_win, y + half_win, img2.get( x, y ) );
    }
  }
  // Pad edges
  for ( int y = 0; y < half_win; y++ ) {
    for ( int x = 0; x < width + 2 * half_win; x++ ) {
      // Top edge
      int src_y = std::min( half_win, height - 1 );
      padded_img1.set( x, y,
                       img1.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), src_y ) ) );
      padded_img2.set( x, y,
                       img2.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), src_y ) ) );

      // Bottom edge
      int bottom_y = height + half_win;
      padded_img1.set( x, bottom_y + y,
                       img1.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_img2.set( x, bottom_y + y,
                       img2.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
    }
  }
  for ( int x = 0; x < half_win; x++ ) {
    for ( int y = 0; y < height + 2 * half_win; y++ ) {
      // Left edge
      int src_x = std::min( half_win, width - 1 );
      padded_img1.set( x, y, img1.get( src_x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_img2.set( x, y, img2.get( src_x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );

      // Right edge
      int right_x = width + half_win;
      padded_img1.set( right_x + x, y, img1.get( width - 1 - x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_img2.set( right_x + x, y, img2.get( width - 1 - x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
    }
  }
  // Compute SSIM for each pixel
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      double mu1 = 0.0, mu2 = 0.0;
      double sigma1_sq = 0.0, sigma2_sq = 0.0, sigma12 = 0.0;

      // Compute mean and variance
      for ( int j = 0; j < window_size; j++ ) {
        for ( int i = 0; i < window_size; i++ ) {
          int    px     = x + i;
          int    py     = y + j;
          double weight = gaussian_window_[i][j];
          double val1   = padded_img1.get( px, py );
          double val2   = padded_img2.get( px, py );

          mu1 += weight * val1;
          mu2 += weight * val2;
        }
      }

      for ( int j = 0; j < window_size; j++ ) {
        for ( int i = 0; i < window_size; i++ ) {
          int    px     = x + i;
          int    py     = y + j;
          double weight = gaussian_window_[i][j];
          double val1   = padded_img1.get( px, py );
          double val2   = padded_img2.get( px, py );

          sigma1_sq += weight * ( val1 - mu1 ) * ( val1 - mu1 );
          sigma2_sq += weight * ( val2 - mu2 ) * ( val2 - mu2 );
          sigma12 += weight * ( val1 - mu1 ) * ( val2 - mu2 );
        }
      }

      // Compute SSIM
      double numerator   = ( 2 * mu1 * mu2 + C1 ) * ( 2 * sigma12 + C2 );
      double denominator = ( mu1 * mu1 + mu2 * mu2 + C1 ) * ( sigma1_sq + sigma2_sq + C2 );

      if ( denominator > 0 ) {
        ssim_total += numerator / denominator;
        valid_windows++;
      }
    }
  }

  return ( valid_windows > 0 ) ? ssim_total / valid_windows : 1.0;
}

// Masked SSIM computation function (OM-SSIM)
double Metric::computeMaskedSSIM( const Plane<double>&  img1,
                                  const Plane<double>&  img2,
                                  const Plane<uint8_t>& mask ) const {
  const int    width       = img1.width();
  const int    height      = img1.height();
  const int    window_size = 11;
  const double K1          = 0.01;
  const double K2          = 0.03;
  const double L           = 255.0;
  const double C1          = ( K1 * L ) * ( K1 * L );
  const double C2          = ( K2 * L ) * ( K2 * L );
  const double threshold   = 0.5;  // Valid window threshold

  // Initialize Gaussian window
  if ( gaussian_window_.empty() || window_size_ != window_size || gaussian_sigma_ != 1.5 ) {
    window_size_     = window_size;
    gaussian_sigma_  = 1.5;
    gaussian_window_ = generate_gaussian_window( window_size, gaussian_sigma_ );
  }

  const int half_win      = window_size / 2;
  double    ssim_total    = 0.0;
  int       valid_windows = 0;

  // Create padded images and mask
  Plane<double> padded_img1, padded_img2;
  Plane<double> padded_mask;
  padded_img1.resize( width + 2 * half_win, height + 2 * half_win, 0.0 );
  padded_img2.resize( width + 2 * half_win, height + 2 * half_win, 0.0 );
  padded_mask.resize( width + 2 * half_win, height + 2 * half_win, 0.0 );

  // Copy original images and perform edge padding
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      padded_img1.set( x + half_win, y + half_win, img1.get( x, y ) );
      padded_img2.set( x + half_win, y + half_win, img2.get( x, y ) );
      padded_mask.set( x + half_win, y + half_win, mask.get( x, y ) / 255.0 );
    }
  }

  // Pad edges
  for ( int y = 0; y < half_win; y++ ) {
    for ( int x = 0; x < width + 2 * half_win; x++ ) {
      // Top edge
      int src_y = std::min( half_win, height - 1 );
      padded_img1.set( x, y,
                       img1.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), src_y ) ) );
      padded_img2.set( x, y,
                       img2.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), src_y ) ) );
      padded_mask.set( x, y,
                       mask.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), src_y ) ) /
                           255.0 );

      // Bottom edge
      int bottom_y = height + half_win;
      padded_img1.set( x, bottom_y + y,
                       img1.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_img2.set( x, bottom_y + y,
                       img2.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_mask.set( x, bottom_y + y,
                       mask.get( std::min( std::max( x - half_win, 0 ), width - 1 ),
                                 std::min( std::max( y - half_win, 0 ), height - 1 ) ) /
                           255.0 );
    }
  }

  for ( int x = 0; x < half_win; x++ ) {
    for ( int y = 0; y < height + 2 * half_win; y++ ) {
      // Left edge
      int src_x = std::min( half_win, width - 1 );
      padded_img1.set( x, y, img1.get( src_x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_img2.set( x, y, img2.get( src_x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_mask.set( x, y, mask.get( src_x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) / 255.0 );

      // Right edge
      int right_x = width + half_win;
      padded_img1.set( right_x + x, y, img1.get( width - 1 - x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_img2.set( right_x + x, y, img2.get( width - 1 - x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) );
      padded_mask.set( right_x + x, y,
                       mask.get( width - 1 - x, std::min( std::max( y - half_win, 0 ), height - 1 ) ) / 255.0 );
    }
  }

  // Compute SSIM for each pixel
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      double mu1 = 0.0, mu2 = 0.0;
      double sigma1_sq = 0.0, sigma2_sq = 0.0, sigma12 = 0.0;
      double mask_sum = 0.0;

      // Compute weighted mean and mask sum
      for ( int j = 0; j < window_size; j++ ) {
        for ( int i = 0; i < window_size; i++ ) {
          int    px       = x + i;
          int    py       = y + j;
          double weight   = gaussian_window_[i][j];
          double mask_val = padded_mask.get( px, py );
          double val1     = padded_img1.get( px, py );
          double val2     = padded_img2.get( px, py );

          mu1 += weight * mask_val * val1;
          mu2 += weight * mask_val * val2;
          mask_sum += weight * mask_val;
        }
      }

      // Skip windows with insufficient mask coverage
      if ( mask_sum < threshold ) { continue; }

      mu1 /= mask_sum;
      mu2 /= mask_sum;

      // Compute variance and covariance
      for ( int j = 0; j < window_size; j++ ) {
        for ( int i = 0; i < window_size; i++ ) {
          int    px       = x + i;
          int    py       = y + j;
          double weight   = gaussian_window_[i][j];
          double mask_val = padded_mask.get( px, py );
          double val1     = padded_img1.get( px, py );
          double val2     = padded_img2.get( px, py );

          sigma1_sq += weight * mask_val * ( val1 - mu1 ) * ( val1 - mu1 );
          sigma2_sq += weight * mask_val * ( val2 - mu2 ) * ( val2 - mu2 );
          sigma12 += weight * mask_val * ( val1 - mu1 ) * ( val2 - mu2 );
        }
      }

      sigma1_sq /= mask_sum;
      sigma2_sq /= mask_sum;
      sigma12 /= mask_sum;

      // Compute SSIM
      double numerator   = ( 2 * mu1 * mu2 + C1 ) * ( 2 * sigma12 + C2 );
      double denominator = ( mu1 * mu1 + mu2 * mu2 + C1 ) * ( sigma1_sq + sigma2_sq + C2 );

      if ( denominator > 0 ) {
        ssim_total += numerator / denominator;
        valid_windows++;
      }
    }
  }

  return ( valid_windows > 0 ) ? ssim_total / valid_windows : 1.0;
}

// Preserve original maskless version
template <typename T, int32_t N>
void Metric::compute( const Image<T, N>& imageA, const Image<T, N>& imageB ) {
  // Create all-valid mask
  Image<uint8_t, 1> emptyMask;
  emptyMask.resize( imageA.width(), imageA.height() );
  emptyMask.fill( 255 );  // All pixels valid

  compute( imageA, imageB, emptyMask );
}

Results Metric::getAverage() {
  Results res;
  for ( auto& el : results_ ) res += el;
  res /= results_.size();
  return res;
}

// Explicit instantiations
template void Metric::compute<uint8_t, 3>( const Image<uint8_t, 3>&,
                                           const Image<uint8_t, 3>&,
                                           const Image<uint8_t, 1>& );
template void Metric::compute<uint16_t, 3>( const Image<uint16_t, 3>&,
                                            const Image<uint16_t, 3>&,
                                            const Image<uint8_t, 1>& );
template void Metric::compute<float, 3>( const Image<float, 3>&, const Image<float, 3>&, const Image<uint8_t, 1>& );

template void Metric::compute<uint8_t>( const Image<uint8_t>&, const Image<uint8_t>& );
template void Metric::compute<uint16_t>( const Image<uint16_t>&, const Image<uint16_t>& );
template void Metric::compute<float>( const Image<float>&, const Image<float>& );