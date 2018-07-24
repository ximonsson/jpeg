# JPEG codec implementation

Not a full implemenation of the JPEG codec, but some work for an old thesis that never was finished.

Four versions of the codec was implemented, first with standard libraries, second is a multithreaded solution of the first one, third used x86 Assembly instructions and SIMD registers, last the fourth one is hardware accelerated using OpenCL, as to compare the speed gained notably during the transforming from spatial domain to frequency domain.

This version only support 1080p and YUV 4:2:2 images, maybe one day more support would be added, but it would be quite hard for the x86 Assembly version.

## Dependencies

* `SDL2 ttf`
* `nasm`
* `OpenCL`

## TODO

* Support other sizes and color domains?
* It all used to work but now the x86 Assembly version no longer compiles. Might need to look into that.

