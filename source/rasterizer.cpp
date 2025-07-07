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

#include "rasterizer.hpp"
#include "rasterizerSoftware.hpp"
#if defined( USE_GLFW )
#include "rasterizerHardware.hpp"
#endif

Rasterizer::Rasterizer( bool useSoftware ) {
  useSoftware_ = useSoftware;

#if defined( USE_GLFW )
  if ( useSoftware_ ) {
    rasterizerSoftware_ = std::make_unique<RasterizerSoftware>();
  } else {
    rasterizerHardware_ = std::make_unique<RasterizerHardware>();
  }
#else
  rasterizerSoftware_ = std::make_unique<RasterizerSoftware>();
#endif
}
Rasterizer::~Rasterizer() {
  rasterizerSoftware_.reset();
#if defined( USE_GLFW )
  rasterizerHardware_.reset();
#endif
}

template <typename T, int32_t N>
void Rasterizer::render( const Pointcloud&  pc,
                         const Viewpoint&   viewpoint,
                         Image<T, N>&       image,
                         Image<uint8_t, 1>& ocm ) {
#if defined( USE_GLFW )
  if ( useSoftware_ )
    rasterizerSoftware_->render( pc, viewpoint, image, ocm );
  else
    rasterizerHardware_->render( pc, viewpoint, image, ocm );
#else
  rasterizerSoftware_->render( pc, viewpoint, image, ocm );
#endif
}

template void Rasterizer::render<uint8_t>( Pointcloud const&,
                                           const Viewpoint&,
                                           Image<uint8_t, 3>&,
                                           Image<uint8_t, 1>& );
template void Rasterizer::render<uint16_t>( Pointcloud const&,
                                            const Viewpoint&,
                                            Image<uint16_t, 3>&,
                                            Image<uint8_t, 1>& );
template void Rasterizer::render<float>( Pointcloud const&, const Viewpoint&, Image<float, 3>&, Image<uint8_t, 1>& );
