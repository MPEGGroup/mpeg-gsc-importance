
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

#include "common.hpp"
#include "pointcloud.hpp"
#include "rasterizer.hpp"
#include "viewpoints.hpp"
#include "program-options-lite/program_options_lite.h"
#include "version.hpp"
#include "timer.hpp"

#if defined( USE_OPENMP )
#include <omp.h>
#endif

Viewpoint makeDownscaledViewpoint( const Viewpoint &viewpoint, float downscaleFactor, int32_t minimumWidth ) {
  int32_t originalWidth  = viewpoint.originalWidth();
  int32_t originalHeight = viewpoint.originalHeight();
  if ( originalWidth <= 0 || originalHeight <= 0 ) { return viewpoint; }

  int32_t downscaledWidth =
      static_cast<int32_t>( std::round( static_cast<double>( originalWidth ) / downscaleFactor ) );
  int32_t newWidth = std::max( minimumWidth, std::max( downscaledWidth, 1 ) );
  float   ratio    = static_cast<float>( newWidth ) / static_cast<float>( originalWidth );
  int32_t newHeight =
      std::max( static_cast<int32_t>( std::round( static_cast<double>( originalHeight ) * ratio ) ), 1 );
  glm::vec2 newFocal = viewpoint.focal() * ratio;

  Viewpoint scaled;
  scaled.set( viewpoint.pos(), viewpoint.view(), viewpoint.up(), newFocal, newWidth, newHeight );
  return scaled;
}

void saveImportanceScores( const std::vector<std::vector<float>> &impScores,
                           const std::string                     &filePath,
                           bool                                   append ) {
  std::ofstream out( filePath, append ? std::ios::app : std::ios::out );

  out << std::setprecision( std::numeric_limits<float>::max_digits10 );
  for ( size_t i = 0; i < impScores[0].size(); i++ ) {
    float acc = 0.0f;
    for ( size_t viewIndex = 0; viewIndex < impScores.size(); viewIndex++ ) { acc += impScores[viewIndex][i]; }
    out << acc << '\n';
  }

  out << '\n';

  out.close();
}

// Parse input command line
struct Parameters {
  bool        verbose_           = false;
  bool        help_              = false;
  std::string sourcePath_        = "";
  std::string viewpointPath_     = "";
  int32_t     startFrame_        = 0;
  int32_t     frameCount_        = 1;
  int32_t     numThreads_        = 0;
  float       downscaleFactor_   = 1.0f;
  int32_t     downscaleMinWidth_ = 1920;
  std::string impScoresPath_     = "";

  bool parse( int32_t argc, char *argv[] ) {
    namespace po = df::program_options_lite;
    po::Options opts;
    /* clang-format off */
    opts.addOptions()
      (po::Section("Common"))
        ("h,help",            help_,              false,              "This help text")
        ("c,config",          po::parseConfigFile,                    "Configuration file name")
        ("v,verbose",         verbose_,           verbose_,           "Verbose")
        ("threads",           numThreads_,        numThreads_,        "Number of threads to use by OpenMP (0=auto)")
      ( po::Section("Input paths") )      
        ( "a,source",         sourcePath_,        sourcePath_,        "Source 3DGS path" )

      (po::Section("Sequence information"))       
        ("i,startFrame",      startFrame_,        startFrame_,        "First frame number")
        ("f,frameCount",      frameCount_,        frameCount_,        "Number of frames")
        ("downscaleFactor",   downscaleFactor_,   downscaleFactor_,   "Downscale factor applied to camera resolution")
        ("downscaleMinWidth", downscaleMinWidth_, downscaleMinWidth_, "Minimum width after downscaling; height inferred from aspect ratio")
      
      (po::Section("Viewpoints"))        
        ("viewpoint",         viewpointPath_,     viewpointPath_,     "Viewpoint path used to load viewpoints")
      ( po::Section("Output") )      
        ("impScore",        impScoresPath_,     impScoresPath_,     "Output text file for importance score of source (no hardware rasterizer support)")
      ;

    /* clang-format on */
    po::setDefaults( opts );
    po::ErrorReporter              err;
    const std::list<const char *> &argv_unhandled = po::scanArgv( opts, argc, (const char **)argv, err );
    if ( argc == 1 || help_ || sourcePath_.empty() || impScoresPath_.empty() || argv_unhandled.size() != 0 ||
         err.is_errored ) {
      std::cout << "\nusage: " << argv[0] << " [arguments...] \n";
      po::doHelp( std::cout, opts, 78 );
      std::cout << "\n";
      if ( sourcePath_.empty() ) err.error() << "Undefined input path \n";
      if ( impScoresPath_.empty() ) printf( "Warning: Importance Score output file is not defined. \n" );
      for ( const auto *const arg : argv_unhandled ) { err.warn() << "Unhandled argument ignored: " << arg << '\n'; }
      if ( err.is_errored ) err.error() << "Unhandled argument \n";
      return 1;
    }
    if ( downscaleFactor_ < 1.0f ) {
      err.error() << "downscaleFactor must be >= 1.0 \n";
      return 1;
    }
    if ( downscaleMinWidth_ < 1 ) {
      err.error() << "downscaleMinWidth must be >= 1 \n";
      return 1;
    }
    return 0;
  }
};

int main( int32_t argc, char *argv[] ) {
  printf( "MPEG GSC metric version %s \n", ::gscm::version );
  printf( "Computing importance scores from views\n" );
  fflush( stdout );

  Parameters params;
  if ( params.parse( argc, argv ) ) { return 1; }
  auto       start = std::chrono::high_resolution_clock::now();
  Rasterizer rasterizer;
  Timer      timer;

#if defined( USE_OPENMP )
  omp_set_dynamic( 0 );
  if ( params.numThreads_ > 0 ) {
    omp_set_num_threads( params.numThreads_ );
  } else {
    omp_set_num_threads( omp_get_num_procs() );
  }
#endif

  for ( int32_t frameIndex = 0; frameIndex < params.frameCount_; frameIndex++ ) {
    // Load pointclouds
    Pointcloud pc;
    timer.tic( "Read" );
    if ( !pc.read( params.sourcePath_, params.startFrame_ + frameIndex, params.verbose_ ) ) return 1;
    timer.toc( "Read", params.verbose_ );
    if ( params.verbose_ ) pc.trace();

    // Create viewpoint list
    Viewpoints viewpoints;
    if ( !params.viewpointPath_.empty() ) { viewpoints.read( params.viewpointPath_ ); }
    if ( viewpoints.size() == 0 ) {
      auto &cameraPosition = pc.getCameraPosition();
      if ( cameraPosition.size() == 0 ) {
        printf( "Error: camera position is not correctly defined in the ply files \n" );
        return 1;
      }
      viewpoints.resize( cameraPosition.size() );
      for ( size_t i = 0; i < cameraPosition.size(); i++ ) {
        const auto &el = cameraPosition[i];
        viewpoints[i].set( el.pos(), el.view(), el.up(), el.focal_, el.width_, el.height_ );
        if ( params.verbose_ ) el.log();
      }
    }

    if ( params.downscaleFactor_ > 1.0f ) {
      for ( auto &viewpoint : viewpoints ) {
        viewpoint = makeDownscaledViewpoint( viewpoint, params.downscaleFactor_, params.downscaleMinWidth_ );
      }
    }

    if ( params.verbose_ ) {
      printf( "Using downscaleFactor=%.3f, minimumWidth=%d\n", params.downscaleFactor_, params.downscaleMinWidth_ );
    }

    std::vector<std::vector<float>> impScores = std::vector<std::vector<float>>( viewpoints.size() );

    for ( int32_t viewpointIndex = 0; viewpointIndex < (int32_t)viewpoints.size(); viewpointIndex++ ) {
      impScores[viewpointIndex] = std::vector<float>( pc.size() );
    }
    // Rendering
    timer.tic( "Rasterize" );
#pragma omp parallel for
    for ( int32_t viewpointIndex = 0; viewpointIndex < (int32_t)viewpoints.size(); viewpointIndex++ ) {
      auto &viewpoint = viewpoints[viewpointIndex];
      if ( params.verbose_ ) {
        printf( "Frame %3d / %3d: viewpoints %3d / %3zu ", frameIndex, params.frameCount_, viewpointIndex,
                viewpoints.size() );
#if defined( USE_OPENMP )
        printf( "Thread %2d / %2d (dynamic=%d) ", omp_get_thread_num(), omp_get_num_threads(), omp_get_dynamic() );
#endif
        printf( "\n" );
        fflush( stdout );
      }
      // Rasterize
      rasterizer.computeTransmittanceSums( pc, viewpoint, impScores[viewpointIndex] );
    }
    timer.toc( "Rasterize", params.verbose_ );
    saveImportanceScores( impScores, params.impScoresPath_, frameIndex > 0 );
  }
  if ( params.verbose_ ) timer.trace();
  std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
  printf( "Total time: %.3f seconds\n", elapsed.count() );
  fflush( stdout );
  return 0;
}
