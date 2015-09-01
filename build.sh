#!/bin/bash

set -e

app_name='Explore.app'
executable_name='explore'

shared_flags='
-g -Wall -Wextra -std=c++11 -Wno-missing-field-initializers
'
optimalization='-O0'
internal=''

libraries="
  -I./libs/assimp/include
  ./build/$app_name/Contents/Frameworks/libassimp.3.1.1.dylib

  -I./libs/glew/include
  ./libs/glew/lib/libGLEW.a

  -I./libs/jemalloc/include
  ./libs/jemalloc/lib/libjemalloc.a

  -I./libs/glm
  -I./libs/vcache
  -I./libs/stb

  -F./build/$app_name/Contents/Frameworks
  -rpath @executable_path/../Frameworks
"

engine_main="src/osx_main.cpp $libraries"
engine_flags="
  -framework SDL2
  -framework OpenGl
"

game_main="src/app.cpp $libraries"
game_flags="
  -ffast-math
  -framework OpenGl
"

build_assimp() {
  pushd assimp > /dev/null
    cmake -G 'Unix Makefiles'
    make -j > /dev/null
    install_name_tool -id @rpath/libassimp.3.1.1.dylib lib/libassimp.3.1.1.dylib
    cp lib/libassimp.3.1.1.dylib ../../build/$app_name/Contents/Frameworks
  popd > /dev/null
}

build_jemalloc() {
  pushd jemalloc > /dev/null
    ./autogen.sh > /dev/null
    make -j > /dev/null
  popd > /dev/null
}

build_glew() {
  pushd glew > /dev/null
    make -j > /dev/null
  popd > /dev/null
}

build_libraries() {
  echo 'Building libraries'
  echo '=================='

  pushd libs > /dev/null

  build_assimp &
  build_glew &
  build_jemalloc &

  popd > /dev/null

  wait
}

build_engine() {
  echo 'Building engine'
  echo '=================='

  clang++ $engine_main -o build/$app_name/Contents/MacOS/$executable_name $shared_flags $engine_flags $optimalization $internal
}

build_game() {
  echo 'Building game'
  echo '=================='

  clang++ -dynamiclib $game_main -o build/$app_name/Contents/Resources/app.dylib $shared_flags $game_flags $optimalization $internal
}

main() {
  release=false
  libs=false
  game=false
  engine=false

  for i in "$@"
  do
    case $i in
      -i|--internal)
        internal='-DINTERNAL'
      ;;
      -r|--release)
        optimalization='-O3'
      ;;
      libs)
        libs=true
      ;;
      all)
        libs=true
        game=true
        engine=true
      ;;
      game)
        game=true
      ;;
      engine)
        engine=true
      ;;
    esac
    shift
  done

  if [ $libs = true ]; then
    build_libraries
  fi

  if [ $game = true ]; then
    build_game &
  fi

  if [ $engine = true ]; then
    build_engine &
  fi

  wait
}

main $@
