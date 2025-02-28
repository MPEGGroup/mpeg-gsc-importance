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
#pragma once

#include "common.hpp"

template <typename T, int32_t N>
class Image;

struct Results {
  glm::dvec3 rgbMSE_  = { 0.0, 0.0, 0.0 };
  glm::dvec3 rgbPSNR_ = { 0.0, 0.0, 0.0 };
  glm::dvec3 yuvMSE_  = { 0.0, 0.0, 0.0 };
  glm::dvec3 yuvPSNR_ = { 0.0, 0.0, 0.0 };

  Results& operator+=( const Results& other ) {
    rgbMSE_ += other.rgbMSE_;
    rgbPSNR_ += other.rgbPSNR_;
    yuvMSE_ += other.yuvMSE_;
    yuvPSNR_ += other.yuvPSNR_;
    return *this;
  }

  Results& operator/=( double divisor ) {
    rgbMSE_ /= divisor;
    rgbPSNR_ /= divisor;
    yuvMSE_ /= divisor;
    yuvPSNR_ /= divisor;
    return *this;
  }

  void log( const std::string name = "" ) {
    printf(
        "Metric %s: "
        "MSE RGB = %12.6f %12.6f %12.6f PSNR RGB = %12.6f %12.6f %12.6f "
        "MSE YUV = %12.6f %12.6f %12.6f PSNR YUV = %12.6f %12.6f %12.6f \n",
        name.empty() ? "" : name.c_str(), rgbMSE_[0], rgbMSE_[1], rgbMSE_[2], rgbPSNR_[0], rgbPSNR_[1], rgbPSNR_[2],
        yuvMSE_[0], yuvMSE_[1], yuvMSE_[2], yuvPSNR_[0], yuvPSNR_[1], yuvPSNR_[2] );
  }
};

class Metric {
 public:
  Metric();
  ~Metric();

  void reset() { results_.clear(); }
  template <typename T, int32_t N>
  void compute( const Image<T, N>& imageA, const Image<T, N>& imageB );

  Results getAverage();

 private:
  std::vector<Results> results_;
};
