# 依赖 pio 命令（PlatformIO）。若未安装请先：pip install platformio 或参考官方文档。
if ! command -v pio >/dev/null 2>&1; then
  echo "ERR: pio not found. Install PlatformIO first."
  exit 1
fi

# function
cd ..
basedir=$PWD

### compile ###
cd $basedir

if [ $# -ne 1 ]; then
  echo "ERR: parameter [VERSION] is required."
  echo "Usage: ./build.sh [VERSION]"
  echo "   e.g. ./build.sh 1.0.5"
  exit 1
fi

version=$1

echo $version

rm -f ./dist/*

pio run -t clean

export PLATFORMIO_BUILD_FLAGS=-DJ_VERSION=\\\"${version}\\\"
pio run -e z98 -e z21 -e z15 -e 1680

if [ $? -ne 0 ]; then
  echo "ERR: build err when firmware of Z98."
  exit 1
else
  cp $basedir/.pio/build/z98/bootloader.bin $basedir/dist/bootloader.bin
  cp $basedir/.pio/build/z98/partitions.bin $basedir/dist/partitions.bin
  cp $basedir/.pio/build/z98/firmware.bin $basedir/dist/tdisplay_${version}_z98.bin
  cp $basedir/.pio/build/z15/firmware.bin $basedir/dist/tdisplay_${version}_z15.bin
  cp $basedir/.pio/build/z21/firmware.bin $basedir/dist/tdisplay_${version}_z21.bin
  cp $basedir/.pio/build/1680/firmware.bin $basedir/dist/tdisplay_${version}_1680.bin
fi

echo
echo
echo
echo ":::  Build finish.  :::"
echo "  Please commit & push to Github."