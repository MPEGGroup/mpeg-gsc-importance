
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
#include "image.hpp"
#include "pointcloud.hpp"
#include "rasterizer.hpp"
#include "viewpoints.hpp"
#include "program-options-lite/program_options_lite.h"
#include "version.hpp"
#include "metric.hpp"
#include "timer.hpp"

// Parse input command line
struct Parameters {
  bool        verbose_        = false;
  bool        help_           = false;
  bool        useCPU_         = false;
  bool        useFloat_       = false;
  bool        display_        = false;
  bool        saveViewpoints_ = false;
  bool        save_           = false;
  std::string sourcePath_     = "";
  std::string decodePath_     = "";
  std::string viewpointPath_  = "";
  int32_t     startFrame_     = 0;
  int32_t     frameCount_     = 1;
  int32_t     numPoints_      = 16;
  int32_t     width_          = 1024;
  int32_t     height_         = 768;

  bool parse( int32_t argc, char* argv[] ) {
    namespace po = df::program_options_lite;
    po::Options opts;
    /* clang-format off */
    opts.addOptions()
      (po::Section("Common"))
        ("h,help",            help_,            false,            "This help text")
        ("c,config",          po::parseConfigFile,                "Configuration file name")
        ("v,verbose",         verbose_,         verbose_,         "Verbose")
      ( po::Section("Input paths") )   
        ( "a,source",         sourcePath_,      sourcePath_,      "Source 3DGS path" )
        ( "b,decode",         decodePath_,      decodePath_,      "Decode 3DGS path" )

      (po::Section("Sequence information"))   
        ("i,startFrame",      startFrame_,      startFrame_,      "First frame number")
        ("f,frameCount",      frameCount_,      frameCount_,      "Number of frames")
        ("width",             width_,           width_,           "Width of the generated images")
        ("height",            height_,          height_,          "Height of the generated images")
      
      (po::Section("Viewpoints"))
        ("viewpoint",         viewpointPath_,   viewpointPath_,   "Viewpoint path")
        ("n,numPoints",       numPoints_,       numPoints_,       "Number of viewpoints")
        ("display",           display_,         display_,         "Show 3D model and viewpoints")
        ("saveViewpoints",    saveViewpoints_,  saveViewpoints_,  "Save the generated viewpoints")

      (po::Section("Rendering"))
        ("cpu",               useCPU_,          useCPU_,          "Use CPU rendering")
        ("float",             useFloat_,        useFloat_,        "Use floatting point images")

      ( po::Section("Options") )     
        ("s,save",            save_,            save_,            "Save generated videos")
      ;

    /* clang-format on */

    po::setDefaults( opts );
    po::ErrorReporter             err;
    const std::list<const char*>& argv_unhandled = po::scanArgv( opts, argc, (const char**)argv, err );

    for ( const auto* const arg : argv_unhandled ) { err.warn() << "Unhandled argument ignored: " << arg << '\n'; }

    if ( argc == 1 || help_ || verbose_ || sourcePath_.empty() || decodePath_.empty() ) {
      std::cout << "usage: " << argv[0] << " [arguments...] \n\n";
      po::doHelp( std::cout, opts, 78 );

      if ( sourcePath_.empty() || decodePath_.empty() ) printf( "Warning: input files are not defined. \n" );
      return !verbose_;
    }

    if ( useFloat_ && save_ ) {
      printf(
          "Warning: Floating point images are not compatible with the recording option, disabling the creation of "
          "output videos\n" );
      save_ = false;
    }
    return 0;
  }
};

int main( int32_t argc, char* argv[] ) {
  printf( "MPEG GSC metric version %s \n", ::gscm::version );
  fflush( stdout );

  Parameters params;
  if ( params.parse( argc, argv ) ) { return 1; }

  Viewpoints viewpoints;
  if ( !params.viewpointPath_.empty() ) { viewpoints.read( params.viewpointPath_ ); }
  Image<uint8_t> imageI[2];
  Image<float>   imageF[2];
  if ( params.useFloat_ )
    for ( int32_t i = 0; i < 2; i++ ) imageF[i].resize( params.width_, params.height_ );
  else
    for ( int32_t i = 0; i < 2; i++ ) imageI[i].resize( params.width_, params.height_ );
  Video<uint8_t> video[2];
  Metric         metric;
  Rasterizer     rasterizer( params.useCPU_ );
  Timer          timer;
  for ( int32_t frameIndex = 0; frameIndex < params.frameCount_; frameIndex++ ) {
    // Load pointclouds
    Pointcloud pc[2];
    timer.tic( "Read" );
    if ( !pc[0].read( params.sourcePath_, params.startFrame_ + frameIndex ) ) return 1;
    if ( !pc[1].read( params.decodePath_, params.startFrame_ + frameIndex ) ) return 1;
    timer.toc( "Read", params.verbose_ );
    if ( params.verbose_ )
      for ( int32_t i = 0; i < 2; i++ ) pc[i].trace();

    // Create viewpoint list
    if ( viewpoints.size() == 0 ) {
      auto [minBound, maxBound] = pc[0].getBoundingBox();
      viewpoints.create( params.numPoints_, ( maxBound + minBound ) * 0.5f );
      if ( params.saveViewpoints_ ) viewpoints.write( "viewpoint.txt" );
    }
    if ( params.display_ ) {
      viewpoints.trace();
      viewpoints.display( pc[1], params.width_, params.height_ );
      return 0;
    }

    // Rendering
    for ( size_t viewpointIndex = 0; viewpointIndex < viewpoints.size(); viewpointIndex++ ) {
      auto& viewpoint = viewpoints[viewpointIndex];
      if ( params.verbose_ ) {
        printf( "Frame %3d / %3d: viewpoints %3zu / %3zu \n", frameIndex, params.frameCount_, viewpointIndex,
                viewpoints.size() );
        fflush( stdout );
      }
      // Rasterize
      timer.tic( "Rasterize" );
      for ( int32_t i = 0; i < 2; i++ )
        if ( params.useFloat_ )
          rasterizer.render( pc[i], viewpoint, imageF[i] );
        else
          rasterizer.render( pc[i], viewpoint, imageI[i] );
      timer.toc( "Rasterize", params.verbose_ );

      // Save
      if ( params.save_ )
        for ( int32_t i = 0; i < 2; i++ ) video[i].add( imageI[i] );

      // Metric
      timer.tic( "Metric" );
      if ( params.useFloat_ )
        metric.compute( imageF[0], imageF[1] );
      else
        metric.compute( imageI[0], imageI[1] );
      timer.toc( "Metric", params.verbose_ );
    }
  }

  // Show metric results
  metric.getAverage().log( getBasename( params.decodePath_ ) );
  if ( params.verbose_ ) timer.trace();

  // Save
  if ( !params.useFloat_ && params.save_ ) {
    video[0].save( getRemoveExtension( getBasename( params.sourcePath_ ) ) + "_video" );
    video[1].save( getRemoveExtension( getBasename( params.decodePath_ ) ) + "_video" );
  }

  fflush( stdout );
  return 0;
}