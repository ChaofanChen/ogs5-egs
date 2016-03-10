OpenGeoSys5-EGS
============

[![Tag](https://img.shields.io/github/tag/norihiro-w/ogs5-egs.svg?style=flat-square)](https://github.com/norihiro-w/ogs5-egs/releases)
[![BSD License (modified)](http://img.shields.io/badge/license-BSD-blue.svg?style=flat-square)](https://github.com/norihiro-w/ogs5-egs/blob/master/LICENSE.txt)
[![Travis](https://img.shields.io/travis/norihiro-w/ogs5-egs.svg?style=flat-square)](https://travis-ci.org/norihiro-w/ogs5-egs)

This code is a branch of OpenGeoSys5 for modeling deep geothermal reservoirs. 

[OpenGeoSys][ogs] (OGS) is a scientific open source project for the development of
numerical methods for the simulation of thermo-hydro-mechanical-chemical
(THMC) processes in porous and fractured media. OGS is implemented in C++, it
is object-oriented with an focus on the numerical solution of coupled multi-field
problems (multi-physics). Parallel versions of OGS are available relying on
both MPI and OpenMP concepts. Application areas of OGS are currently CO2
sequestration, geothermal energy, water resources management, hydrology and
waste deposition. OGS is developed by the
[OpenGeoSys Community][ogs].

- General homepage: http://www.opengeosys.org
- Wiki: https://svn.ufz.de/ogs
- Build instructions: https://docs.opengeosys.org/docs/devguide5/getting-started/introduction

## Quickstart ##

``` bash
cd [source-directory]
mkdir build
cd build
cmake ..
```

Open the Visual Studio solution which was created in the build-directory or just type `make` on Linux.

## License ##

OpenGeoSys is distributed under a Modified BSD License which encourages users to
attribute the work of the OpenGeoSys Community especially in scientific
publications. See the [LICENSE.txt][license-source] for the license text.

[ogs]: http://www.opengeosys.org

