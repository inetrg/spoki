#!/bin/bash

ROOT_DIR=$(pwd)

#SCAMPER_FOLDER=scamper-cvs-20181219
#SCAMPER_TAR=$SCAMPER_FOLDER.tar.gz
#SCAMPER_URL=https://www.caida.org/tools/measurement/scamper/code/$SCAMPER_TAR
# https://www.caida.org/tools/measurement/scamper/code/scamper-cvs-20191102a.tar.gz
SCAMPER_FOLDER=scamper-cvs-20200923
SCAMPER_TAR=$SCAMPER_FOLDER.tar.gz
SCAMPER_URL=https://www.caida.org/tools/measurement/scamper/code/$SCAMPER_TAR

ZLIB_FOLDER=zlib-1.2.11
ZLIB_TAR=$ZLIB_FOLDER.tar.gz
ZLIB_URL=https://www.zlib.net/$ZLIB_TAR

# WANDIO_FOLDER=wandio-4.2.3
# WANDIO_TAR=$WANDIO_FOLDER.tar.gz
# WANDIO_URL=https://research.wand.net.nz/software/wandio/$WANDIO_TAR
WANDIO_FOLDER=4.2.3-1
WANDIO_TAR=$WANDIO_FOLDER.tar.gz
WANDIO_URL=https://github.com/wanduow/wandio/archive/$WANDIO_TAR

#LIBTRACE_VERSION=4.0.9
#LIBTRACE_FOLDER=libtrace-$VERSION
#LIBTRACE_TAR=$VERSION.tar.gz
#LIBTRACE_URL=https://github.com/LibtraceTeam/libtrace/archive/$LIBTRACE_TAR

# curl -O https://research.wand.net.nz/software/libtrace/libtrace-latest.tar.bz2
LIBTRACE_FOLDER=libtrace-4.0.17
LIBTRACE_TAR=libtrace-latest.tar.bz2
LIBTRACE_URL=https://research.wand.net.nz/software/libtrace/$LIBTRACE_TAR
# tar xjf

NLOHMAN_JSON_TAG=v3.9.1

cores=4
if [ "$(uname)" == "Darwin" ]; then
  cores=$(sysctl -n hw.ncpu)
elif [ "$(uname)" == "FreeBSD" ]; then
  cores=$(sysctl -n hw.ncpu)
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
  cores=$(nproc --all)
fi

INSTALL_PREFIX="${PWD}/deps"

echo "installing to '${INSTALL_PREFIX}'"
mkdir ${INSTALL_PREFIX}

echo "Using $cores cores for compilation."

echo ""
echo "# ---------------------------------------------------------------------------- #"
echo "# -- Building CAF ------------------------------------------------------------ #"
echo "# ---------------------------------------------------------------------------- #"
# This only works with newer git versions:
# git -C actor-framework pull || git clone https://github.com/actor-framework/actor-framework.git
if cd $ROOT_DIR/actor-framework
then
  git pull
else
  cd $ROOT_DIR
  git clone https://github.com/actor-framework/actor-framework.git
  cd actor-framework
  git checkout tags/0.18.5
fi
cd $ROOT_DIR/actor-framework
./configure --build-type=release --disable-openssl-module --disable-examples --disable-tools --prefix=${INSTALL_PREFIX}
make -C build -j$cores
make -C build install
cd $ROOT_DIR


echo ""
echo "# ---------------------------------------------------------------------------- #"
echo "# -- Building scmaper -------------------------------------------------------- # "
echo "# ---------------------------------------------------------------------------- #"
if cd $ROOT_DIR/scamper
then
  echo "scamper already downloaded"
else
  cd $ROOT_DIR
  curl -LO $SCAMPER_URL
  tar xzf $SCAMPER_TAR
  mv $SCAMPER_FOLDER scamper
  rm $SCAMPER_TAR
  # patch -p 0 -d scamper < patches/patch-ping-tcp-win.txt
  patch -p 0 -d scamper < patches/writebuf-scamper.patch
  patch -p 0 -d scamper < patches/patch-pingtimes.txt
  cd scamper
  touch NEWS README AUTHORS ChangeLog
fi
cd $ROOT_DIR/scamper
autoreconf -vfi
./configure --prefix=${INSTALL_PREFIX} --exec-prefix=${INSTALL_PREFIX}
make -j 4
make install
cd $ROOT_DIR

#echo "Building zlib"
#if cd $ROOT_DIR/zlib
#then
#  echo "zlib already downloaded"
#else
#  cd $ROOT_DIR
#  curl -LO $ZLIB_URL
#  tar xzf $ZLIB_TAR
#  mv $ZLIB_FOLDER zlib
#  rm $ZLIB_TAR
#fi
#cd $ROOT_DIR/zlib
#./configure
#make
#cd $ROOT_DIR

echo ""
echo "# ---------------------------------------------------------------------------- #"
echo "# -- Building wandio --------------------------------------------------------- #"
echo "# ---------------------------------------------------------------------------- #"
if cd $ROOT_DIR/wandio
then
  echo "wandio already downloaded"
else
  cd $ROOT_DIR
  curl -LO $WANDIO_URL
  tar xzf $WANDIO_TAR
  mv wandio-$WANDIO_FOLDER wandio
  rm $WANDIO_TAR
fi
cd $ROOT_DIR/wandio
./bootstrap.sh
./configure --prefix=${INSTALL_PREFIX}
make
make install
cd $ROOT_DIR

echo ""
echo "# ---------------------------------------------------------------------------- #"
echo "# -- Building libtrace ------------------------------------------------------- #"
echo "# ---------------------------------------------------------------------------- #"
if cd $ROOT_DIR/libtrace
then
  echo "libtrace already cloned"
else
  cd $ROOT_DIR
  curl -L -O $LIBTRACE_URL
  tar xjf $LIBTRACE_TAR
  mv $LIBTRACE_FOLDER libtrace
  rm $LIBTRACE_TAR
fi
cd $ROOT_DIR/libtrace
CPPFLAGS=-I$ROOT_DIR/deps/include/ LDFLAGS=-L$ROOT_DIR/deps/lib/ ./configure --prefix=${INSTALL_PREFIX}
CPPFLAGS=-I$ROOT_DIR/deps/include/ LDFLAGS=-L$ROOT_DIR/deps/lib/ make
CPPFLAGS=-I$ROOT_DIR/deps/include/ LDFLAGS=-L$ROOT_DIR/deps/lib/ make install
cd $ROOT_DIR

echo ""
echo "# ---------------------------------------------------------------------------- #"
echo "# -- Downloading JSON -------------------------------------------------------- #"
echo "# ---------------------------------------------------------------------------- #"
if cd json
then
  echo "JSON lib already pulled"
else
  cd $ROOT_DIR
  git clone https://github.com/nlohmann/json.git
fi
cd $ROOT_DIR/json
git pull
git checkout tags/$NLOHMAN_JSON_TAG
cd $ROOT_DIR


echo ""
echo ""
echo "Done."
echo "(On macos you might want to 'export DYLD_LIBRARY_PATH=${ROOT_DIR}/deps/libs/'.)"
echo "(On Linux try: 'export LD_LIBRARY_PATH=${ROOT_DIR}/deps/libs'"
