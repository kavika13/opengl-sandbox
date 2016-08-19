#define GL3_PROTOTYPES 1
#include <OpenGL/gl3.h>
#include <SDL2/SDL.h>
#include <fstream>
#include <sstream>
#include <iostream>

void FatalError(std::function<void(std::ostringstream&)> formatErrorMessage, bool showSdlMessageBox = true) {
    std::ostringstream errorMessage;

    formatErrorMessage(errorMessage);

    std::cerr << errorMessage.str();
    if (showSdlMessageBox) {
        SDL_ShowSimpleMessageBox(0, "OpenGL Sandbox", errorMessage.str().c_str(), 0);
    }
    throw std::runtime_error(errorMessage.str());
}

struct SDL {
    SDL() {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            FatalError([](std::ostringstream& errorMessage) {
                errorMessage << "Failed to initialized SDL: " << SDL_GetError();
            }, false);
        }
    }

    ~SDL() {
        SDL_Quit();
    }
};

struct MainWindow {
    MainWindow() {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Required by OSX
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        mainWindow_ = SDL_CreateWindow(
            "OpenGL Sandbox",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            640,
            640,
            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);

        if (!mainWindow_) {
            FatalError([](std::ostringstream& errorMessage) {
                errorMessage << "Failed to create main window: " << SDL_GetError();
            });
        }
    }

    ~MainWindow() {
        SDL_DestroyWindow(mainWindow_);
    }

    operator SDL_Window*() const {
        return mainWindow_;
    }

 private:
    SDL_Window* mainWindow_;
};

struct OpenGL {
    explicit OpenGL(SDL_Window* window) : context_(SDL_GL_CreateContext(window)) {
        if (!context_) {
            FatalError([](std::ostringstream& errorMessage) {
                errorMessage << "Failed to create OpenGL context: " << SDL_GetError();
            });
        }

        if (SDL_GL_SetSwapInterval(1) < 0) {
            std::cout << "could not set the swap interval\n";
        }
    }

    ~OpenGL() {
        SDL_GL_DeleteContext(context_);
    }

    operator SDL_GLContext() const {
        return context_;
    }

 private:
    const SDL_GLContext context_;
};

struct ResourceLoader {
    ResourceLoader() : basePath_(SDL_GetBasePath()) {
    }

    std::string LoadFile(const std::string& filename) const {
        std::ostringstream resourcePath;
        resourcePath << basePath_ << filename;

        std::ifstream infile(resourcePath.str(), std::ios::in);

        if (infile) {
            std::string contents;
            infile.seekg(0, std::ios::end);
            contents.resize(infile.tellg());
            infile.seekg(0, std::ios::beg);

            infile.read(&contents[0], contents.size());
            infile.close();

            return contents;
        }

        FatalError([&](std::ostringstream& errorMessage) {
            errorMessage << "Error opening file: " << resourcePath.str() << " - " << strerror(errno) << "\n";
        });

        return 0;  // Never gets here - quiet the warning
    }

 private:
    const char* basePath_;
};

struct Shader {
 public:
    operator GLuint() const {
        return shaderHandle_;
    }

 protected:
    Shader(GLenum type, const std::string& source) : shaderHandle_(glCreateShader(type)) {
        const char* sourceChars = &source[0];
        glShaderSource(shaderHandle_, 1, &sourceChars, NULL);
        glCompileShader(shaderHandle_);

        GLint param;

        glGetShaderiv(shaderHandle_, GL_COMPILE_STATUS, &param);

        if (!param) {
            glGetShaderiv(shaderHandle_, GL_INFO_LOG_LENGTH, &param);
            std::string log;
            log.reserve(param);

            glGetShaderInfoLog(shaderHandle_, log.size(), NULL, &log[0]);

            FatalError([&](std::ostringstream& errorMessage) {
                errorMessage << "Error compiling " << type << " shader:\n" << &log[0] << "\n";;
            });
        }
    }

    ~Shader() {
        if (shaderHandle_) {
            glDeleteShader(shaderHandle_);
        }
    }

 private:
    GLuint shaderHandle_;
};

struct VertexShader : public Shader {
    explicit VertexShader(const std::string& source) : Shader(GL_VERTEX_SHADER, source) {
    }
};

struct FragmentShader : public Shader {
    explicit FragmentShader(const std::string& source) : Shader(GL_FRAGMENT_SHADER, source) {
    }
};

struct ShaderProgram {
    ShaderProgram(const std::string& vertexShaderFilename, const std::string& fragmentShaderFilename) {
        ResourceLoader resourceLoader;
        std::string vertexShaderSource = resourceLoader.LoadFile(vertexShaderFilename);
        std::string fragmentShaderSource = resourceLoader.LoadFile(fragmentShaderFilename);

        VertexShader vertexShader(vertexShaderSource);
        FragmentShader fragmentShader(fragmentShaderSource);

        LinkProgram(vertexShader, fragmentShader);
    }

    ShaderProgram(const VertexShader& vertexShader, const FragmentShader& fragmentShader) {
        LinkProgram(vertexShader, fragmentShader);
    }

    ~ShaderProgram() {
        glDeleteProgram(shaderProgramHandle_);
    }

    operator GLuint() const {
        return shaderProgramHandle_;
    }

 private:
    void LinkProgram(const VertexShader& vertexShader, const FragmentShader& fragmentShader) {
        shaderProgramHandle_ = glCreateProgram();

        glAttachShader(shaderProgramHandle_, vertexShader);
        glAttachShader(shaderProgramHandle_, fragmentShader);
        glLinkProgram(shaderProgramHandle_);

        GLint param;
        glGetProgramiv(shaderProgramHandle_, GL_LINK_STATUS, &param);

        if (!param) {
            glGetProgramiv(shaderProgramHandle_, GL_INFO_LOG_LENGTH, &param);
            std::string log;
            log.reserve(param);

            glGetProgramInfoLog(shaderProgramHandle_, log.size(), NULL, &log[0]);

            FatalError([&](std::ostringstream& errorMessage) {
                errorMessage << "Error linking vertex and fragment shaders:\n" << &log[0] << "\n";;
            });
        }
    }

    GLuint shaderProgramHandle_;
};

struct VertexBuffer {
    template<typename T, size_t N>
    explicit VertexBuffer(const T (&bufferData)[N]) {
        glGenBuffers(1, &bufferHandle_);

        glBindBuffer(GL_ARRAY_BUFFER, bufferHandle_);
            glBufferData(GL_ARRAY_BUFFER, sizeof(bufferData), bufferData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    ~VertexBuffer() {
        glDeleteBuffers(1, &bufferHandle_);
    }

    operator GLuint() const {
        return bufferHandle_;
    }

 private:
    GLuint bufferHandle_;
};

struct VertexArray {
    explicit VertexArray(std::function<void()> configureVertexArray) {
        glGenVertexArrays(1, &arrayHandle_);

        glBindVertexArray(arrayHandle_);
            configureVertexArray();
        glBindVertexArray(0);
    }

    ~VertexArray() {
        glDeleteVertexArrays(1, &arrayHandle_);
    }

    operator GLuint() const {
        return arrayHandle_;
    }

 private:
    GLuint arrayHandle_;
};

struct Scene {
    template<typename T, size_t N>
    Scene(
        const ShaderProgram& shaderProgram,
        const VertexArray& vertexArray,
        const T (&vertices)[N],
        size_t vertexSize,
        GLint angleParameter)
            : program(shaderProgram)
            , vertexArray(vertexArray)
            , vertexCount(sizeof(vertices) / sizeof(vertices[0]) / vertexSize)
            , angleShaderParameter(angleParameter) {
    }

    bool isRunning = true;

    GLuint program;
    GLuint vertexArray;
    size_t vertexCount;

    GLint angleShaderParameter;
    float angle = 0.0f;
};

void Update(Scene* scene, double secondsSinceLastUpdate) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            scene->isRunning = false;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_q:
                    scene->isRunning = false;
                    break;
            }
            break;
        }
    }

    scene->angle += 1.0 * secondsSinceLastUpdate;

    if (scene->angle > 2 * M_PI) {
        scene->angle -= 2 * M_PI;
    }
}

void Draw(SDL_Window* mainWindow, const Scene& scene) {
    glClearColor(0.15, 0.15, 0.15, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(scene.program);
        glUniform1f(scene.angleShaderParameter, scene.angle);
        glBindVertexArray(scene.vertexArray);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, scene.vertexCount);
        glBindVertexArray(0);
    glUseProgram(0);

    SDL_GL_SwapWindow(mainWindow);
}

void GameLoop(SDL_Window* mainWindow, Scene* scene) {
    const double ticksPerSecond = SDL_GetPerformanceFrequency();
    const double secondsPerUpdate = 1.0 / 60.0;
    double previousTickCount = SDL_GetPerformanceCounter();
    double accumulatedTime = 0.0;

    while (scene->isRunning) {
        double currentTickCount = SDL_GetPerformanceCounter();
        double deltaTickCount = currentTickCount - previousTickCount;
        accumulatedTime += deltaTickCount / ticksPerSecond;
        previousTickCount = currentTickCount;

        if (accumulatedTime > secondsPerUpdate) {
            accumulatedTime -= secondsPerUpdate;
            Update(scene, secondsPerUpdate);
        }

        Draw(mainWindow, *scene);
    }
}

int main(int argc, char* argv[]) {
    SDL sdl;
    MainWindow mainWindow;
    OpenGL openGL(mainWindow);

    ShaderProgram shaderProgram("shader.vert", "shader.frag");
    GLint angleShaderParameter = glGetUniformLocation(shaderProgram, "angle");

    size_t vertexSize = 2;
    float vertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
         1.0f, -1.0f
    };
    VertexBuffer vertexBuffer(vertices);
    VertexArray vertexArray([&]() {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            // Attribute 0 -> 2 float values per vertex, tightly packed, 0 offset
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    });

    Scene scene {
        shaderProgram,
        vertexArray,
        vertices,
        vertexSize,
        angleShaderParameter
    };

    GameLoop(mainWindow, &scene);

    return 0;
}
