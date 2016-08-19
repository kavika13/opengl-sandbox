SANDBOX_BIN = opengl-sandbox
SANDBOX_APP = $(SANDBOX_BIN).app
SANDBOX_APP_BIN = $(SANDBOX_APP)/Contents/MacOS/$(SANDBOX_BIN)

SDL_VERSION = 2.0.4
SDL_IMAGE_VERSION = 2.0.1
GLM_VERSION = 0.9.7.6

SDL_DIR = extlibs/SDL-$(SDL_VERSION)-osx
SDL_IMAGE_DIR = extlibs/SDL_image-$(SDL_IMAGE_VERSION)-osx
GLM_DIR = extlibs/glm-$(GLM_VERSION)

SANDBOX_SOURCES = $(wildcard src/*.cpp)
SANDBOX_OBJECTS = $(SANDBOX_SOURCES:.cpp=.o)

SANDBOX_RESOURCES = $(wildcard resources/*)
SANDBOX_APP_RESOURCE_DIR = $(SANDBOX_APP)/Contents/Resources

SOURCES = $(SANDBOX_SOURCES)
OBJECTS = $(SANDBOX_OBJECTS)
APPS = $(SANDBOX_APP)

CXXFLAGS = --std=c++14
CPPFLAGS = -F$(SDL_DIR)/Frameworks -F$(SDL_IMAGE_DIR)/Frameworks -I$(GLM_DIR)
LDFLAGS = -Xlinker -rpath -Xlinker @loader_path/../Frameworks
LDLIBS = -framework SDL2 -framework SDL2_image -framework OpenGL

all: CXXFLAGS += -Os
debug: CXXFLAGS += -O0 -g

all: $(APPS)

debug: $(APPS)

opengl-sandbox.app: $(SANDBOX_APP_BIN)
	mkdir -p $@/Contents/Frameworks
	mkdir -p $(SANDBOX_APP_RESOURCE_DIR)
	cp -r $(SDL_DIR)/Frameworks/* $@/Contents/Frameworks
	cp -r $(SDL_IMAGE_DIR)/Frameworks/* $@/Contents/Frameworks
	cp $(SANDBOX_RESOURCES) $(SANDBOX_APP_RESOURCE_DIR)
	cp Info.plist $@/Contents
	cp PkgInfo $@/Contents

clean:
	$(RM) $(OBJECTS)
	$(RM) -r $(APPS)

$(SANDBOX_APP_BIN): $(SANDBOX_OBJECTS)
	mkdir -p `dirname $@`
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@
	echo $(OS_detected) $(OS)
