CXX      = g++
CXXFLAGS = -std=c++14 -O2 -Wall -Wno-unused-variable
LDFLAGS  = -lopengl32 -lglu32 -lgdi32 -luser32 -lkernel32 -mwindows
INCLUDES = -Isrc

TARGET   = raytracer_editor

SRC      = src/platform/win32_main.cpp

all: $(TARGET)

$(TARGET): $(SRC) \
    src/core/vec3.h src/core/ray.h src/core/math.h \
    src/scene/material.h src/scene/object.h src/scene/scene.h \
    src/render/camera.h src/render/tracer.h \
    src/editor/editor.h src/platform/ui.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "Build complete: $(TARGET).exe"

clean:
	del /Q $(TARGET).exe 2>nul || rm -f $(TARGET).exe 2>/dev/null; true

run: all
	./$(TARGET).exe

.PHONY: all clean run