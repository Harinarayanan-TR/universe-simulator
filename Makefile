JAVA_HOME := /usr/lib/jvm/java-21-openjdk-amd64
ifeq ($(shell test -d $(JAVA_HOME) && echo yes),yes)
else
JAVA_HOME := $(shell dirname $$(dirname $$(readlink -f /home/error_404_hari/.vscode/extensions/redhat.java-1.54.0-linux-x64/jre/21.0.10-linux-x86_64/bin/javac 2>/dev/null) 2>/dev/null) 2>/dev/null)
endif
JAVA_HOME_VSCODE := /home/error_404_hari/.vscode/extensions/redhat.java-1.54.0-linux-x64/jre/21.0.10-linux-x86_64
CC = gcc
CFLAGS = -fPIC -O3 -march=native -ffast-math -Wall -Wextra -Wno-unused-parameter
JNI_CFLAGS = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -I.
LDFLAGS = -shared -lm
TARGET = libuniverse_physics.so

all: $(TARGET) UniverseSimulator.class

$(TARGET): physics_engine.o universe_jni.o neural_net.o renderer_3d.o
	$(CC) $(LDFLAGS) -o $@ $^

physics_engine.o: physics_engine.c physics_engine.h
	$(CC) $(CFLAGS) $(JNI_CFLAGS) -c $< -o $@

universe_jni.o: universe_jni.c physics_engine.h neural_net.h renderer_3d.h
	$(CC) $(CFLAGS) $(JNI_CFLAGS) -c $< -o $@

neural_net.o: neural_net.c neural_net.h physics_engine.h
	$(CC) $(CFLAGS) $(JNI_CFLAGS) -c $< -o $@

renderer_3d.o: renderer_3d.c renderer_3d.h physics_engine.h
	$(CC) $(CFLAGS) $(JNI_CFLAGS) -c $< -o $@

UniverseSimulator.class: UniverseSimulator.java
	$(JAVA_HOME_VSCODE)/bin/javac -h . $<

clean:
	rm -f *.o *.so *.class

run: all
	$(JAVA_HOME_VSCODE)/bin/java -Djava.library.path=. UniverseSimulator

.PHONY: all clean run
