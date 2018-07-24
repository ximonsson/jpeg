IMG=img/superman.uyvy

echo ""

echo " ==== standard C ==== "
bin/jpeg compress 1920 1080 $IMG x.jpg

echo " ==== x86 Assembly ==== "
bin/jpeg_asm compress 1920 1080 $IMG y.jpg

echo " ==== OpenCL ==== "
bin/jpeg_hw compress 1920 1080 $IMG z.jpg
