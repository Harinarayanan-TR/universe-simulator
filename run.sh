#!/bin/bash
set -e

JAVA_HOME="/home/error_404_hari/.vscode/extensions/redhat.java-1.54.0-linux-x64/jre/21.0.10-linux-x86_64"
echo "Using Java from: $JAVA_HOME"
echo "Building universe simulation..."

cd "$(dirname "$0")"

gcc -fPIC -O3 -march=native -ffast-math -Wall -Wno-unused-parameter \
    -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux" -I. \
    -c physics_engine.c -o physics_engine.o

echo "  C physics engine: OK"

gcc -fPIC -O3 -march=native -ffast-math -Wall -Wno-unused-parameter \
    -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux" -I. \
    -c universe_jni.c -o universe_jni.o

echo "  JNI bridge: OK"

gcc -shared -lm -o libuniverse_physics.so physics_engine.o universe_jni.o

echo "  Shared library: OK (libuniverse_physics.so)"

"$JAVA_HOME/bin/javac" UniverseSimulator.java

echo "  Java classes: OK"
echo ""
echo "Compilation complete! Starting universe simulation..."
echo ""

"$JAVA_HOME/bin/java" -Djava.library.path=. UniverseSimulator
