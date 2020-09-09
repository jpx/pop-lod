set -xe

TARGET="$1"

if [[ -z "$TARGET" ]]; then
    echo "missing target"
    echo -e "usage:\n./build.sh linux|html5"
    exit 1
fi

BUILD_DIR_NAME="build-${TARGET}"

EXTRA_OPTIONS=
if [[ $TARGET == "html5" ]]; then
    EXTRA_OPTIONS+=-DCMAKE_TOOLCHAIN_FILE=${EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake
    EXTRA_OPTIONS+=" -DWITH_WASM=ON"
fi

pushd "$MINKO_HOME"
mkdir -p "${BUILD_DIR_NAME}"
pushd "${BUILD_DIR_NAME}"
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_EXAMPLES=OFF -DWITH_PLUGINS=ON -DMINKO_HOME="${MINKO_HOME}" $EXTRA_OPTIONS
make -j$(nproc)
popd
popd

mkdir -p "${BUILD_DIR_NAME}"
pushd "${BUILD_DIR_NAME}"
echo $MINKO_HOME
cmake .. -DCMAKE_BUILD_TYPE=Release -DMINKO_HOME="${MINKO_HOME}/${BUILD_DIR_NAME}" $EXTRA_OPTIONS
make -j$(nproc)
popd
