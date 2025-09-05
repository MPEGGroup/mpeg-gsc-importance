# MPEG Gaussian splat compression metrics 

Mpeg-gsc-metric computes image-based metrics between two sequences of 3D gaussian splat objects.

# Cloning and Building Instructions

The source code can be obtained by: 
```
git clone https://git.mpeg.expert/MPEG/3dgh/GSC/gsc-software/mpeg-gsc-metrics.git`
```

The software supports Windows, MacOs and Linux operating systems. Compilation can be done as follows:
* Linux:
  * to build use the command `build.sh`.
  * to clean all object use the command `clear.sh`.
* Windows:
  * to build use the CMAKE command or `build.sh` or open MSVC project ./build/Release/PccAppRenderer.sln
  * to clean all object use the command `clear.sh`.
* MacOs:
  * to build use the CMAKE command or `build.sh` or open Xcode.
  * to clean all object use the command `clear.sh`.

Cmake commands can be used to build software:

``` 
cd mpeg-gsc-metric
mkdir build 
cd ./build/
cmake .. 
cmake --build . --config Release --parallel 20 
```

The command `clear.sh all` can be used to remove all dependencies.

# Depensencies

Mpeg-gsc-metric project uses:
- OpenGL  
- [GLFW](https://github.com/glfw/glfw)
- [OpenGL Mathematics(GLM)](https://github.com/g-truc/glm.git)
- program-options-lite.

# Usage 

The mpeg-gsc-metric input parameters are listed below:

| --key=default value | Usage |
|-----|-----|
| **Common**                  | |
| -h,   --help=0              | This help text <br> |
| -c,   --config=...          | Configuration file name <br> |
| -v,   --verbose=0           | Verbose <br> |
| **Input paths**             | |
| -a,   --source=""           |  Source 3DGS path <br> |
| -b,   --decode=""           |  Decode 3DGS path <br> |
| **Sequence information**    | |
| -i,   --startFrame=0        |  First frame number <br> |
| -f,   --frameCount=1        |  Number of frames <br> |
|       --width=1024          |  Width of the generated images <br> |
|       --height=768          |  Height of the generated images <br> |
| **Viewpoints**              | |
|       --viewpoint=""        | Viewpoint path used to load viewpoints <br> |
|       --useCameraPosition=0 | Use camera position stored in ply file <br> |
| -n,   --numPoints=16        | Create N random viewpoints <br> |
|       --display=0           | Show 3D model and viewpoints <br> |
|       --saveViewpoints=0    | Save the generated viewpoints <br> |
|       --onlyViewpoint=-1     | Only render the viewpoint <br> |
| **Rendering**               | |
|       --cpu=0               | Use CPU rendering <br> |
|       --float=0             | Use floatting point images <br> |
|       --compareCpuGpu=0     | Compare CPU/GPU rendering <br> |
| **metrics**                 | |
|       --computeOm=1         | Compute OM metrics (OM-PSNR and OM-SSIM) <br> |
|       --computeSsim=1       | Compute SSIM metrics <br> |
|       --computeIvssim=1     | Compute IVSSIM metrics <br> |
|       --computeQmiv=0       | Compute color transform and average as QMIV <br> |
| **Options**                 | |
| -s,   --save=0              | Save generated videos <br> |
| -o,   --output=""           | Output text file for metrics <br> |

## Examples

The next section presents some example of command line used to compute metric. 

### Automatic metric between two 3DGS files

```bash
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
  -a src.ply \
  -b dec.ply
```

### Automatic viewpoints around the objects

```bash
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
     -a src_%04d.ply \
     -b dec_%04d.ply \
     -i 0 \
     -f 32 \
     -n 16 
```

### Loaded viewpoints

```bash
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
     -a src.ply \
     -b dec.ply 
     --viewpoints ./config/viewpoints.txt
     --cpu 1 \
     -v  
```

### Create video 

The '-s,--save' option stores the 2D images used to calculate metrics in browsable videos. These videos are:
- Source: Contains the images generated with the source PLY sequence.
- Decode: Contains the images generated with the decoded PLY sequence.
- Butterfly: Contains a bufferfly video with the source video on the left and the decoded video on the right.

```bash
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
     -a src.ply \
     -b dec.ply \
     --save=1 \
     -v 1
```

### Only viewpoint

The '--onlyViewport=-1' option forces the renderer to use a single, fixed camera viewpoint for every frame in the animation sequence. By disabling all other viewports and sticking to this one perspective, you can generate a video that shows your scene from exactly the same angle throughout its duration. 

```bash
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
  -a src_%04d.ply \
  -b dec_%04d.ply \
  -f 32 \
  -i 0 \
  -s 1 \
  --useCameraPosition=1 \
  --onlyViewpoint=10 \
  --width=1920 \
  --height=1080 \
```

# Experimentations using gstTools, mpeg-3d-renderer and mpeg-gsc-metrics software

## Apple

### Data set
- 3DGS files: https://content.mpeg.expert/data/Explorations/GSC/gaussian_splat/3dgs/point_cloud/JEE6_1/m71903_bust_dataset/trained_models/apple.zip
- Colmap data: https://content.mpeg.expert/data/Explorations/GSC/gaussian_splat/3dgs/point_cloud/JEE6_1/m71903_bust_dataset/colmap_data/apple/apple.zip


Unzip contents in in folder `./apple/ply/` and `./ apple/colmap/`. 

### Run gstTools

Use [GsTools software](https://git.mpeg.expert/MPEG/Explorations/GSC/gsc-software/mpeg-gsc-tools/-/tree/main/gsTools?ref_type=heads)  to add camera position into ply files. 

```bash
../mpeg-gsc-tools/gsTools/build.sh && \
GSTOOLS=../mpeg-gsc-tools/gsTools/build/msvc/Release/bin/Release/cameraPosition.exe ; \
DIR=/g/gs/mpeg/apple; \
mkdir -p ${DIR}/ply_pos; \
for((i=81;i<=81;i++)) \
do \
  ${GSTOOLS} --input=${DIR}/ply/$( printf %04d $i).ply \
             --camera=${DIR}/colmap/$( printf %06d $i)/sparse/cameras.bin \
             --image=${DIR}/colmap/$( printf %06d $i)/sparse/images.bin \
             --output=${DIR}/ply_pos/apple_$( printf %04d $i).ply \
             --verbose=1; \
done
```

### Run renderer

When you first open a ply object folder, the viewpoint will not be correct. Move around the object with the mouse (left mouse button), use the alignment keys (1 to 6) or the Tab key to navigate to the camera positions.

If you wish, you can save the current viewpoint to the source folder by pressing Ctrl + Y.

```bash
../mpeg-3d-renderer-mpeg/bin/windows/Release/PccAppRenderer.exe \
  -f /g/gs/mpeg/apple/ply_pos/apple_%04d.ply \
  -g 1 \
  -n 32 \
  -i 81

``` 

### Run metrics

```bash
DIR=/g/gs/mpeg/apple && \
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
  -a ${DIR}/ply_pos/apple_%04d.ply \
  -b ${DIR}/ply_pos/apple_%04d.ply \
  -f 32 \
  -i 81 \
  -s 1 \
  --useCameraPosition=1 \
  --width=1920 \
  --height=1080 \
  -v 1 
```

## bartender_stable_m71763

### Data set: 
- 3DGS files: https://content.mpeg.expert/data/Explorations/GSC/gaussian_splat/3dgs/point_cloud/JEE6_1/m71763_bartender_stable/track 
- Colmap data: https://content.mpeg.expert/data/Explorations/GSC/gaussian_splat/3dgs/point_cloud/JEE6_1/m71763_bartender_stable/colmap_data

Unzip contents in in folder `./ apple/ply/' and `./ apple/colmap/'

### Run gstTools

Use [GsTools software](https://git.mpeg.expert/MPEG/Explorations/GSC/gsc-software/mpeg-gsc-tools/-/tree/main/gsTools?ref_type=heads)  to add camera position into ply files. 

```bash
../mpeg-gsc-tools/gsTools/build.sh && \
GSTOOLS=../mpeg-gsc-tools/gsTools/build/msvc/Release/bin/Release/cameraPosition.exe ; \
DIR=/g/gs/mpeg/bartender_stable_m71763/content.mpeg.expert/; \
mkdir -p ${DIR}/track_pos; \
for((i=0;i<=31;i++)) \
do \
  ${GSTOOLS} --input=${DIR}/track/frame$( printf %03d $i).ply \
             --camera=${DIR}/colmap_data/frame000/sparse/cameras.txt \
             --image=${DIR}/colmap_data/frame000/sparse/images.txt \
             --output=${DIR}/track_pos/track_$( printf %04d $i).ply \
             --verbose=1; \
done
```

### Run renderer

When you first open a ply object folder, the viewpoint will not be correct. Move around the object with the mouse (left mouse button), use the alignment keys (1 to 6) or the Tab key to navigate to the camera positions.

If you wish, you can save the current viewpoint to the source folder by pressing Ctrl + Y.

The keyboard shortcut "g" can be used to see camera positions and orientations.

```bash
DIR=/g/gs/mpeg/bartender_stable_m71763/content.mpeg.expert/track_pos; \
../mpeg-3d-renderer-mpeg/bin/windows/Release/PccAppRenderer.exe \
  -f ${DIR}/track_%04d.ply \
  -g 1 \
  -n 32 \
  -i 0
``` 

### Run metrics

```bash
DIR=/g/gs/mpeg_20250707/m71763_bartender_stable/track_pos; \
./build/Release/bin/Release/mpeg-gsc-metrics.exe \
  -a ${DIR}/frame%03d_pos.ply \
  -b ${DIR}/frame%03d_pos.ply \
  -f 1 \
  -i 0 \
  -s 1 \
  --useCameraPosition=1 \
  --width=1920 \
  --height=1080 \
  -v 1 
```

# Notes
Please, don't hesitate to report me any issues with the renderer by the GitLab Issue Tracker or directly by [email](mailto:jricard@global.tencent.com).

# Licence 

The copyright in this software is being made available under the BSD Licence, included below. This software may be subject to other third party and contributor rights, including patent rights, and no such rights are granted under this licence. 

Copyright (c) 2025, ISO/IEC All rights reserved. 

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
* Neither the name of the ISO/IEC nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

# Contact 

* [Julien Ricard](mailto:jricard@global.tencent.com).