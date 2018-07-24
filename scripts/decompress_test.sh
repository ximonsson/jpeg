ITERATIONS=1000
IMG=y.jpg
#IMG=img/b.jpg
echo ""

echo " ==== standard C ==== "
bin/jpeg decompress 1920 1080 $IMG $ITERATIONS

echo " ==== x86 Assembly ==== "
bin/jpeg_asm decompress 1920 1080 $IMG $ITERATIONS

echo " ==== OpenCL ==== "
bin/jpeg_hw decompress 1920 1080 $IMG $ITERATIONS
