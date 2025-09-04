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
std::vector<std::vector<double>> generateGaussianWindow( int windowSize, double sigma ) {
  std::vector<std::vector<double>> window( windowSize, std::vector<double>( windowSize ) );
  double                           sum      = 0.0;
  int                              halfSize = windowSize / 2;

  for ( int i = -halfSize; i <= halfSize; i++ ) {
    for ( int j = -halfSize; j <= halfSize; j++ ) {
      double value                       = std::exp( -( i * i + j * j ) / ( 2 * sigma * sigma ) );
      window[i + halfSize][j + halfSize] = value;
      sum += value;
    }
  }

  // Normalize
  for ( auto& row : window ) {
    for ( auto& val : row ) { val /= sum; }
  }

  return window;
}

// Preserve original maskless version
template <typename T, int32_t N>
void Metric::compute( const Image<T, N>& imageA, const Image<T, N>& imageB, const int32_t frameIndex ) {
  Image<uint8_t, 1> emptyMask( imageA.width(), imageA.height(), 1 );
  compute( imageA, imageB, emptyMask, frameIndex );
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
    const double peak2  = peak * peak;
    const auto   size   = imageA.size();
    const int    width  = imageA.width();
    const int    height = imageA.height();

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

    // Compute RGB combined channel PSNR (QMIV = 1:1:1 MPEG = 1:1:1)
    if ( computeQmiv_ ) {
      res.psnrRgbAvg_ = ( res.psnrRgb_.x + res.psnrRgb_.y + res.psnrRgb_.z ) / 3.0;
    } else {
      double mseRgbAvg = ( res.mseRgb_.x + res.mseRgb_.y + res.mseRgb_.z ) / 3.0;
      res.psnrRgbAvg_  = ( mseRgbAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseRgbAvg ) ) : 999.99;
    }

    // Compute YUV combined channel PSNR (QMIV = 4:1:1 MPEG = 1:1:1)
    if ( computeQmiv_ ) {
      res.psnrYuvAvg_ = ( 4.0 * res.psnrYuv_.x + res.psnrYuv_.y + res.psnrYuv_.z ) / 6.0;
    } else {
      double mseYuvAvg = ( res.mseYuv_.x + res.mseYuv_.y + res.mseYuv_.z ) / 3.0;
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

      // Compute RGB combined channel OM-PSNR (QMIV = 1:1:1 MPEG = 1:1:1)
      if ( computeQmiv_ ) {
        res.psnrRgbOmAvg_ = ( res.psnrRgbOm_.x + res.psnrRgbOm_.y + res.psnrRgbOm_.z ) / 3.0;
      } else {
        double mseRgbOmAvg = ( res.mseRgbOm_.x + res.mseRgbOm_.y + res.mseRgbOm_.z ) / 3.0;
        res.psnrRgbOmAvg_ =
            ( mseRgbOmAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseRgbOmAvg ) ) : 999.99;
      }

      // Compute YUV combined channel OM-PSNR (QMIV = 4:1:1 MPEG = 1:1:1)
      if ( computeQmiv_ ) {
        res.psnrYuvOmAvg_ = ( 4.0 * res.psnrYuvOm_.x + res.psnrYuvOm_.y + res.psnrYuvOm_.z ) / 6.0;
      } else {
        double mseYuvOmAvg = ( res.mseYuvOm_.x + res.mseYuvOm_.y + res.mseYuvOm_.z ) / 3.0;
        res.psnrYuvOmAvg_ =
            ( mseYuvOmAvg > 0 ) ? std::min( 999.99, 10.0 * std::log10( ( peak2 ) / mseYuvOmAvg ) ) : 999.99;
      }

      // Compute occupancy rate
      res.occupancyRate_ = 100.0 * validPixelCount / size;
    }

    // Compute SSIM and OM-SSIM metrics
    if ( computeSsimMetrics_ ) {
      // Create YUV images
      Image<double, 3> yuvA( width, height );
      Image<double, 3> yuvB( width, height );
      for ( int32_t index = 0; index < size; index++ ) {
        yuvA.set( index, computeQmiv_ ? imageA.getYuvQmiv( index ) : imageA.getYuv( index ) );
        yuvB.set( index, computeQmiv_ ? imageB.getYuvQmiv( index ) : imageB.getYuv( index ) );
      }

      // Compute SSIM for each channel
      for ( int c = 0; c < 3; c++ ) { res.ssim_[c] = computeSSIM( yuvA[c], yuvB[c] ); }

      // Compute SSIM weighted average (QMIV = 4:1:1 MPEG = 1:1:1)
      if ( computeQmiv_ ) {
        res.ssimAvg_ = ( 4.0 * res.ssim_[0] + res.ssim_[1] + res.ssim_[2] ) / 6.0;
      } else {
        res.ssimAvg_ = ( res.ssim_[0] + res.ssim_[1] + res.ssim_[2] ) / 3.0;
      }

      if ( computeOmMetrics_ ) {
        // Compute OM-SSIM for each channel
        for ( int c = 0; c < 3; c++ ) { res.ssimOm_[c] = computeMaskedSSIM( yuvA[c], yuvB[c], mask.plane( 0 ) ); }

        // Compute OM-SSIM weighted average (QMIV = 4:1:1 MPEG = 1:1:1)
        if ( computeQmiv_ ) {
          res.ssimOmAvg_ = ( 4.0 * res.ssimOm_[0] + res.ssimOm_[1] + res.ssimOm_[2] ) / 6.0;
        } else {
          res.ssimOmAvg_ = ( res.ssimOm_[0] + res.ssimOm_[1] + res.ssimOm_[2] ) / 3.0;
        }
      }
    }

    // Compute IVSSIM metrics
    if ( computeIvssimMetrics_ ) { computeIVSSIM( res, imageA, imageB ); }
  }
  results_.push_back( res );
}

template <typename T, int32_t N>
void convertBuffer( PMBB::xPicP& dst, const Image<T, N>& src ) {
  using namespace PMBB;

  const int32 width     = dst.getWidth();
  const int32 height    = dst.getHeight();
  const int32 bitDepth  = dst.getBitDepth();
  const int32 dstStride = dst.getStride();

  for ( int32 c = 0; c < 3; c++ ) {
    const Plane<T>& srcPlane = src.plane( c );
    uint16*         dstPtr   = dst.getAddr( eCmp( c ) );

    for ( int32 y = 0; y < height; y++ ) {
      for ( int32 x = 0; x < width; x++ ) {
        T srcA = srcPlane.get( x, y );
        if constexpr ( std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ) {
          dstPtr[x] = srcA;
        } else if constexpr ( std::is_same_v<T, float> ) {
          dstPtr[x] = round( srcA * xBitDepth2MaxValue( bitDepth ) );
        }
      }
      dstPtr += dstStride;
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
  procIvSsim.createThrdPoolIntf( nullptr, 0 );  // create empty thread pool interface, don't use parallel processing
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
  const int    width      = img1.width();
  const int    height     = img1.height();
  const int    windowSize = 11;
  const double k1         = 0.01;
  const double k2         = 0.03;
  const double l          = 255.0;
  const double c1         = ( k1 * l ) * ( k1 * l );
  const double c2         = ( k2 * l ) * ( k2 * l );

  // Initialize Gaussian window (if needed)
  if ( gaussianWindow_.empty() || windowSize_ != windowSize || gaussianSigma_ != 1.5 ) {
    windowSize_     = windowSize;
    gaussianSigma_  = 1.5;
    gaussianWindow_ = generateGaussianWindow( windowSize, gaussianSigma_ );
  }

  double    ssimTotal    = 0.0;
  int       validWindows = 0;
  const int halfWin      = windowSize / 2;

  // Create padded images
  Plane<double> paddedImg1, paddedImg2;
  paddedImg1.resize( width + 2 * halfWin, height + 2 * halfWin, 0.0 );
  paddedImg2.resize( width + 2 * halfWin, height + 2 * halfWin, 0.0 );

  //  Copy original images and perform edge padding
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      paddedImg1.set( x + halfWin, y + halfWin, img1.get( x, y ) );
      paddedImg2.set( x + halfWin, y + halfWin, img2.get( x, y ) );
    }
  }
  // Pad edges
  const int srcX    = std::min( halfWin, width - 1 );
  const int srcY    = std::min( halfWin, height - 1 );
  const int bottomY = height + halfWin;
  const int rightX  = width + halfWin;
  for ( int y = 0; y < halfWin; y++ ) {
    const int yt = std::min( std::max( y - halfWin, 0 ), srcY );
    const int yb = std::min( std::max( y - halfWin, 0 ), height - 1 );
    for ( int x = 0; x < width + 2 * halfWin; x++ ) {
      // Top edge
      const int x0 = std::min( std::max( x - halfWin, 0 ), width - 1 );
      paddedImg1.set( x, y, img1.get( x0, yt ) );
      paddedImg2.set( x, y, img2.get( x0, yt ) );
      // Bottom edge
      paddedImg1.set( x, bottomY + y, img1.get( x0, yb ) );
      paddedImg2.set( x, bottomY + y, img2.get( x0, yb ) );
    }
  }
  for ( int x = 0; x < halfWin; x++ ) {
    for ( int y = 0; y < height + 2 * halfWin; y++ ) {
      // Left edge
      const int y0 = std::min( std::max( y - halfWin, 0 ), height - 1 );
      paddedImg1.set( x, y, img1.get( srcX, y0 ) );
      paddedImg2.set( x, y, img2.get( srcX, y0 ) );
      // Right edge
      paddedImg1.set( rightX + x, y, img1.get( width - 1 - x, y0 ) );
      paddedImg2.set( rightX + x, y, img2.get( width - 1 - x, y0 ) );
    }
  }
  // Compute SSIM for each pixel
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      double mu1 = 0.0, mu2 = 0.0;
      double sigma1Sq = 0.0, sigma2Sq = 0.0, sigma12 = 0.0;

      // Compute mean and variance
      for ( int j = 0; j < windowSize; j++ ) {
        for ( int i = 0; i < windowSize; i++ ) {
          int    px     = x + i;
          int    py     = y + j;
          double weight = gaussianWindow_[i][j];
          double val1   = paddedImg1.get( px, py );
          double val2   = paddedImg2.get( px, py );
          mu1 += weight * val1;
          mu2 += weight * val2;
        }
      }

      for ( int j = 0; j < windowSize; j++ ) {
        for ( int i = 0; i < windowSize; i++ ) {
          int    px     = x + i;
          int    py     = y + j;
          double weight = gaussianWindow_[i][j];
          double val1   = paddedImg1.get( px, py );
          double val2   = paddedImg2.get( px, py );

          sigma1Sq += weight * ( val1 - mu1 ) * ( val1 - mu1 );
          sigma2Sq += weight * ( val2 - mu2 ) * ( val2 - mu2 );
          sigma12 += weight * ( val1 - mu1 ) * ( val2 - mu2 );
        }
      }

      // Compute SSIM
      double numerator   = ( 2 * mu1 * mu2 + c1 ) * ( 2 * sigma12 + c2 );
      double denominator = ( mu1 * mu1 + mu2 * mu2 + c1 ) * ( sigma1Sq + sigma2Sq + c2 );

      if ( denominator > 0 ) {
        ssimTotal += numerator / denominator;
        validWindows++;
      }
    }
  }

  return ( validWindows > 0 ) ? ssimTotal / validWindows : 1.0;
}

// Masked SSIM computation function (OM-SSIM)
double Metric::computeMaskedSSIM( const Plane<double>&  img1,
                                  const Plane<double>&  img2,
                                  const Plane<uint8_t>& mask ) const {
  const int    width      = img1.width();
  const int    height     = img1.height();
  const int    windowSize = 11;
  const double k1         = 0.01;
  const double k2         = 0.03;
  const double l          = 255.0;
  const double c1         = ( k1 * l ) * ( k1 * l );
  const double c2         = ( k2 * l ) * ( k2 * l );
  const double threshold  = 0.5;  // Valid window threshold

  // Initialize Gaussian window
  if ( gaussianWindow_.empty() || windowSize_ != windowSize || gaussianSigma_ != 1.5 ) {
    windowSize_     = windowSize;
    gaussianSigma_  = 1.5;
    gaussianWindow_ = generateGaussianWindow( windowSize, gaussianSigma_ );
  }

  const int halfWin      = windowSize / 2;
  double    ssimTotal    = 0.0;
  int       validWindows = 0;

  // Create padded images and mask
  Plane<double> paddedImg1, paddedImg2;
  Plane<double> paddedMask;
  paddedImg1.resize( width + 2 * halfWin, height + 2 * halfWin, 0.0 );
  paddedImg2.resize( width + 2 * halfWin, height + 2 * halfWin, 0.0 );
  paddedMask.resize( width + 2 * halfWin, height + 2 * halfWin, 0.0 );

  // Copy original images and perform edge padding
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      paddedImg1.set( x + halfWin, y + halfWin, img1.get( x, y ) );
      paddedImg2.set( x + halfWin, y + halfWin, img2.get( x, y ) );
      paddedMask.set( x + halfWin, y + halfWin, mask.get( x, y ) );
    }
  }

  // Pad edges
  const int srcX    = std::min( halfWin, width - 1 );
  const int srcY    = std::min( halfWin, height - 1 );
  const int bottomY = height + halfWin;
  const int rightX  = width + halfWin;
  for ( int y = 0; y < halfWin; y++ ) {
    const int yt = std::min( std::max( y - halfWin, 0 ), srcY );
    const int yb = std::min( std::max( y - halfWin, 0 ), height - 1 );
    for ( int x = 0; x < width + 2 * halfWin; x++ ) {
      // Top edge
      const int x0 = std::min( std::max( x - halfWin, 0 ), width - 1 );
      paddedImg1.set( x, y, img1.get( x0, yt ) );
      paddedImg2.set( x, y, img2.get( x0, yt ) );
      paddedMask.set( x, y, mask.get( x0, yt ) );
      // Bottom edge
      paddedImg1.set( x, bottomY + y, img1.get( x0, yb ) );
      paddedImg2.set( x, bottomY + y, img2.get( x0, yb ) );
      paddedMask.set( x, bottomY + y, mask.get( x0, yb ) );
    }
  }
  for ( int x = 0; x < halfWin; x++ ) {
    for ( int y = 0; y < height + 2 * halfWin; y++ ) {
      // Left edge
      const int y0 = std::min( std::max( y - halfWin, 0 ), height - 1 );
      paddedImg1.set( x, y, img1.get( srcX, y0 ) );
      paddedImg2.set( x, y, img2.get( srcX, y0 ) );
      paddedMask.set( x, y, mask.get( srcX, y0 ) );
      // Right edge
      paddedImg1.set( rightX + x, y, img1.get( width - 1 - x, y0 ) );
      paddedImg2.set( rightX + x, y, img2.get( width - 1 - x, y0 ) );
      paddedMask.set( rightX + x, y, mask.get( width - 1 - x, y0 ) );
    }
  }

  // Compute SSIM for each pixel
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      double mu1 = 0.0, mu2 = 0.0;
      double sigma1Sq = 0.0, sigma2Sq = 0.0, sigma12 = 0.0;
      double maskSum = 0.0;

      // Compute weighted mean and mask sum
      for ( int j = 0; j < windowSize; j++ ) {
        for ( int i = 0; i < windowSize; i++ ) {
          int    px      = x + i;
          int    py      = y + j;
          double weight  = gaussianWindow_[i][j];
          double maskVal = paddedMask.get( px, py );
          double val1    = paddedImg1.get( px, py );
          double val2    = paddedImg2.get( px, py );

          mu1 += weight * maskVal * val1;
          mu2 += weight * maskVal * val2;
          maskSum += weight * maskVal;
        }
      }

      // Skip windows with insufficient mask coverage
      if ( maskSum < threshold ) { continue; }

      mu1 /= maskSum;
      mu2 /= maskSum;

      // Compute variance and covariance
      for ( int j = 0; j < windowSize; j++ ) {
        for ( int i = 0; i < windowSize; i++ ) {
          int    px      = x + i;
          int    py      = y + j;
          double weight  = gaussianWindow_[i][j];
          double maskVal = paddedMask.get( px, py );
          double val1    = paddedImg1.get( px, py );
          double val2    = paddedImg2.get( px, py );

          sigma1Sq += weight * maskVal * ( val1 - mu1 ) * ( val1 - mu1 );
          sigma2Sq += weight * maskVal * ( val2 - mu2 ) * ( val2 - mu2 );
          sigma12 += weight * maskVal * ( val1 - mu1 ) * ( val2 - mu2 );
        }
      }

      sigma1Sq /= maskSum;
      sigma2Sq /= maskSum;
      sigma12 /= maskSum;

      // Compute SSIM
      double numerator   = ( 2 * mu1 * mu2 + c1 ) * ( 2 * sigma12 + c2 );
      double denominator = ( mu1 * mu1 + mu2 * mu2 + c1 ) * ( sigma1Sq + sigma2Sq + c2 );

      if ( denominator > 0 ) {
        ssimTotal += numerator / denominator;
        validWindows++;
      }
    }
  }
  return ( validWindows > 0 ) ? ssimTotal / validWindows : 1.0;
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

template void Metric::compute<uint8_t, 3>( const Image<uint8_t, 3>&, const Image<uint8_t, 3>&, const int32_t );
template void Metric::compute<uint16_t, 3>( const Image<uint16_t, 3>&, const Image<uint16_t, 3>&, const int32_t );
template void Metric::compute<float, 3>( const Image<float, 3>&, const Image<float, 3>&, const int32_t );