BIN=ffmpeg
FILE=bbb.mov
NAME=bbb
TB=1
IN=video/big_buck_bunny_1080p_h264.mov

$BIN -i $IN -an -c:v libvpx-vp9 -r 24 -b:v 9282k -threads 2 -copytb $TB video/bbb.webm
# $BIN -i $IN -an -c:v libschroedinger -r 24 -b:v 9282k -threads 2 -copytb $TB video/bbb.mkv
# $BIN -i $IN -an -c:v mjpeg -r 24 -b:v 9282k -threads 1 -copytb $TB video/bbb.mov
# $BIN -i $IN -an -c:v libopenjpg -r 24 -b:v 9282k -threads 1 -copytb $TB video/bbb.mov
