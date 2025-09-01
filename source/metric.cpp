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

Metric::Metric( bool computeOm, bool computeSsim, bool computeIvssimMetrics, bool computeQmiv ) :
    computeOmMetrics_( computeOm ),
    computeSsimMetrics_( computeSsim ),
    computeIvssimMetrics_( computeIvssimMetrics ),
    computeQmiv_( computeQmiv ) {}

Metric::~Metric() { results_.clear(); }

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
void Metric::compute( const Image<T, N>&       imageA,
                      const Image<T, N>&       imageB,
                      const Image<uint8_t, 1>& mask,
                      const int32_t            frameIndex ) {
  Results res;
  res.frameIndex_ = frameIndex;
  if ( imageA.size() != imageB.size() || ( computeOmMetrics_ && imageA.size() != mask.size() ) ) {
    printf( "Error: Images or mask are not the same size\n" );
    return;
  } else {
    double peak = 1.0;
    if constexpr ( std::is_integral_v<T> ) {
      peak = static_cast<double>( std::numeric_limits<T>::max() );
    } else if constexpr ( std::is_floating_point_v<T> ) {
      peak = 1.0;
    }
    double     peak2  = peak * peak;
    const auto size   = imageA.size();
    const int  width  = imageA.width();
    const int  height = imageA.height();

    // Initialize statistical variables
    int32_t    validPixelCount = 0;
    glm::dvec3 mseRgbOm        = { 0.0, 0.0, 0.0 };
    glm::dvec3 mseYuvOm        = { 0.0, 0.0, 0.0 };

    // Original MSE and OM-MSE computation
    for ( int32_t index = 0; index < size; index++ ) {
      const glm::dvec3 rgb = imageA.get( index ) - imageB.get( index );
      const glm::dvec3 yuv = computeQmiv_ ? imageA.getYuvQmiv( index ) - imageB.getYuvQmiv( index )
                                          : imageA.getYuv( index ) - imageB.getYuv( index );
      res.mseRgb_ += rgb * rgb;
      res.mseYuv_ += yuv * yuv;
      // Use mask to detect valid pixels
      if ( computeOmMetrics_ && mask[0].get( index ) != 0 ) {
        validPixelCount++;
        mseRgbOm += rgb * rgb;
        mseYuvOm += yuv * yuv;
      }
    }

    // Computation PSNR full image
    res.mseRgb_ /= size;
    res.mseYuv_ /= size;
    res.psnrRgb_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak2 ) / res.mseRgb_ ) );
    res.psnrYuv_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak2 ) / res.mseYuv_ ) );

    // Compute RGB combined channel PSNR (1:1:1)
    if ( computeQmiv_ ) {
      res.psnrRgbAvg_ = ( res.psnrRgb_.x + res.psnrRgb_.y + res.psnrRgb_.z ) / 3.0;
    } else {
      double mseRgbAvg = ( res.mseRgb_.x + res.mseRgb_.y + res.mseRgb_.z ) / 3.0;
      res.psnrRgbAvg_  = ( mseRgbAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseRgbAvg ) ) : 999.99;
    }

    // Compute YUV combined channel PSNR (4:1:1)
    if ( computeQmiv_ ) {
      res.psnrYuvAvg_ = ( 4.0 * res.psnrYuv_.x + res.psnrYuv_.y + res.psnrYuv_.z ) / 6.0;
    } else {
      double mseYuvAvg = ( 4.0 * res.mseYuv_.x + res.mseYuv_.y + res.mseYuv_.z ) / 6.0;
      res.psnrYuvAvg_  = ( mseYuvAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseYuvAvg ) ) : 999.99;
    }

    // Compute OM-PSNR for valid pixels
    if ( computeOmMetrics_ && validPixelCount > 0 ) {
      // Compute RGB MSE
      res.mseRgbOm_  = mseRgbOm / static_cast<double>( validPixelCount );
      res.psnrRgbOm_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak2 ) / ( res.mseRgbOm_ ) ) );

      // Compute YUV MSE
      res.mseYuvOm_  = mseYuvOm / static_cast<double>( validPixelCount );
      res.psnrYuvOm_ = glm::min( glm::dvec3( 999.99 ), 10.0 * log10( ( peak2 ) / ( res.mseYuvOm_ ) ) );

      // Compute RGB combined channel OM-PSNR (1:1:1)
      if ( computeQmiv_ ) {
        res.psnrRgbOmAvg_ = ( res.psnrRgbOm_.x + res.psnrRgbOm_.y + res.psnrRgbOm_.z ) / 3.0;
      } else {
        double mseRgbOmAvg = ( res.mseRgbOm_.x + res.mseRgbOm_.y + res.mseRgbOm_.z ) / 3.0;
        res.psnrRgbOmAvg_ =
            ( mseRgbOmAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseRgbOmAvg ) ) : 999.99;
      }

      // Compute YUV combined channel OM-PSNR (4:1:1)
      if ( computeQmiv_ ) {
        res.psnrYuvOmAvg_ = ( 4.0 * res.psnrYuvOm_.x + res.psnrYuvOm_.y + res.psnrYuvOm_.z ) / 6.0;
      } else {
        double mseYuvOmAvg = ( 4.0 * res.mseYuvOm_.x + res.mseYuvOm_.y + res.mseYuvOm_.z ) / 6.0;
        res.psnrYuvOmAvg_ =
            ( mseYuvOmAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseYuvOmAvg ) ) : 999.99;
      }

      // Compute occupancy rate
      res.occupancyRate_ = 100.0 * validPixelCount / size;
    }

    // Compute SSIM metrics
    if ( computeSsimMetrics_ ) {
      // Create YUV images
      Image<double, 3> yuvA( width, height );
      Image<double, 3> yuvB( width, height );
      for ( int32_t index = 0; index < size; index++ ) {
        yuvA.set( index, computeQmiv_ ? imageA.getYuvQmiv( index ) : imageA.getYuv( index ) );
        yuvB.set( index, computeQmiv_ ? imageB.getYuvQmiv( index ) : imageB.getYuv( index ) );
      }

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

    // Compute IVSSIM metrics
    if ( computeIvssimMetrics_ ) { computeIVSSIM( res, imageA, imageB ); }
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

template <typename>
inline constexpr bool always_false_v = false;

template <typename T, int32_t N>
void Metric::computeIVSSIM( Results& result, const Image<T, N>& imageA, const Image<T, N>& imageB ) {
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
    static_assert( always_false_v<T>, "Unsupported Image<T, N> type" );
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
  result.rgbIvSsim_ = rgbIvSsim;
  result.yuvIvSsim_ = ycbcrIvSsim;

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

Results Metric::getAverage() {
  Results res;
  for ( auto& el : results_ ) res += el;
  res /= results_.size();
  return res;
}

Results Metric::getLastFrame() {
  Results res;
  if ( results_.size() == 0 ) return res;
  int32_t frameIndex = results_.back().frameIndex_;
  int32_t numFrames  = 0;
  for ( auto& el : results_ )
    if ( el.frameIndex_ == frameIndex ) {
      res += el;
      numFrames++;
    }
  res /= numFrames;
  return res;
}

// Explicit instantiations
template void Metric::compute<uint8_t, 3>( const Image<uint8_t, 3>&,
                                           const Image<uint8_t, 3>&,
                                           const Image<uint8_t, 1>&,
                                           const int32_t );
template void Metric::compute<uint16_t, 3>( const Image<uint16_t, 3>&,
                                            const Image<uint16_t, 3>&,
                                            const Image<uint8_t, 1>&,
                                            const int32_t );
template void Metric::compute<float, 3>( const Image<float, 3>&,
                                         const Image<float, 3>&,
                                         const Image<uint8_t, 1>&,
                                         const int32_t );

template void Metric::compute<uint8_t>( const Image<uint8_t>&, const Image<uint8_t>&, const int32_t );
template void Metric::compute<uint16_t>( const Image<uint16_t>&, const Image<uint16_t>&, const int32_t );
template void Metric::compute<float>( const Image<float>&, const Image<float>&, const int32_t );