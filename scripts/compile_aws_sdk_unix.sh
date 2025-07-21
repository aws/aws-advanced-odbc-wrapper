CONFIGURATION=$1    # Debug/Release

export ROOT_REPO_PATH=$(cd "$(dirname "$0")/.."; pwd -P)

export AWS_SDK_CPP_TAG="1.11.481"

export AWS_SDK_PATH="${ROOT_REPO_PATH}/aws_sdk"
export SRC_DIR="${AWS_SDK_PATH}/aws_sdk_cpp"
export BUILD_DIR="${AWS_SDK_PATH}/build"
export INSTALL_DIR="${AWS_SDK_PATH}/install"

mkdir -p ${SRC_DIR} ${BUILD_DIR} ${INSTALL_DIR}
pushd $BUILD_DIR

git clone --recurse-submodules -b "$AWS_SDK_CPP_TAG" "https://github.com/aws/aws-sdk-cpp.git" ${SRC_DIR}

cmake -S ${SRC_DIR} \
    -B $BUILD_DIR \
    -D CMAKE_BUILD_TYPE="${CONFIGURATION}" \
    -D CMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -D BUILD_ONLY="rds;secretsmanager;sts" \
    -D ENABLE_TESTING="OFF" \
    -D CPP_STANDARD="20" \
    -D BUILD_SHARED_LIBS="OFF" \
    -D FORCE_SHARED_CRT="ON"

cmake --build . --config=${CONFIGURATION}
cmake --install . --config=${CONFIGURATION}

popd
