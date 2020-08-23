#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <thread>
#include <chrono>
#include <cstring>
#include <exception>
#include <cassert>
#include <string>

#define GLFW_DLL 1
#include <glad/glad.h>
#include <glfw/glfw3.h>

#define PSIG "[ctri] "

static constexpr float PI = 3.14159;
static constexpr int LOG_BUFFER_SIZE = 512;
static constexpr int STEP_KEY = GLFW_KEY_SPACE;

static bool stepConfirmed = true;

const char *vertShader = R"(
#version 330 core
layout (location = 0) in vec2 cds;
void main() {
    gl_Position = vec4(cds, 0, 1);
}
)";
const char *fragShader = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0, 0, 0, 1);
}
)";

static unsigned int computePolygon(int n, float scl, float thickness, bool circum);
static GLuint compileShader(GLenum type, const char *data);
static GLuint linkShaders(GLuint vertObject, GLuint fragObject);
static const char *getGLErrorStr(GLenum error);
static bool streq(const char *a, const char *b);
static void printHelp();
static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

struct mesh {
    unsigned int vertexObj;
    unsigned int arrayBuf;
    unsigned int elementBuf;
    int indices;
    void alloc() {
        glGenVertexArrays(1, &vertexObj);
        glGenBuffers(1, &arrayBuf);
        glGenBuffers(1, &elementBuf);
    }
    void free() {
        glDeleteBuffers(1, &elementBuf);
        glDeleteBuffers(1, &arrayBuf);
        glDeleteVertexArrays(1, &vertexObj);
    }
    void bind() {
        glBindVertexArray(vertexObj);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuf);
    }
};

int main(int argc, char **argv) {
    int exitCode = 0;

    float lineWidth = 0.01;
    float circleThickness = 0.01;
    int circleFineness = 100;
    float animationIntervalSeconds = 0.5;
    int animationEndPC = 50;
    int startPolygonCount = 3;
    int MSAASamples = 10;
    float scl = 0.95;
    bool step = false;
    bool circumscribed = false;

    for (int i = 1; i < argc; i++) {
        const char *param = argv[i];
        const int length = std::strlen(param);
        if (length > 2 && param[0] == '-') {
            if (param[1] == '-') {
                param += 2;
                int *targetInt = nullptr;
                float *targetFloat = nullptr;
                // No, a hashmap is quite franky overkill
                if (streq(param, "pwidth")) {
                    targetFloat = &lineWidth;
                } else if (streq(param, "cwidth")) {
                    targetFloat = &circleThickness;
                } else if (streq(param, "cres")) {
                    targetInt = &circleFineness;
                } else if (streq(param, "interval")) {
                    targetFloat = &animationIntervalSeconds;
                } else if (streq(param, "pmax")) {
                    targetInt = &animationEndPC;
                } else if (streq(param, "pstart")) {
                    targetInt = &startPolygonCount;
                } else if (streq(param, "samples")) {
                    targetInt = &MSAASamples;
                } else if (streq(param, "cscale")) {
                    targetFloat = &scl;
                } else {
                    std::cerr << PSIG "Ignoring unknown flag \"" << argv[i] << '\"' << '\n';
                    continue;
                }
                if (i+1 < argc) {
                    i++;
                    const char *value = argv[i];
                    if (targetInt != nullptr) {
                        try {
                            *targetInt = std::stoi(value, 0, 0);
                        } catch (std::exception &e) {
                            std::cerr << PSIG "Expected \"" << value << "\" to be an integer: " << e.what() << '\n';
                        }
                    } else {
                        assert(targetFloat != nullptr);
                        try {
                            *targetFloat = std::stof(value);
                        } catch (std::exception &e) {
                            std::cerr << PSIG "Expected \"" << value << "\" to be a number: " << e.what() << '\n';
                        }
                    }
                } else {
                    std::cerr << PSIG "Expected value after \"" << argv[i] << '\"' << '\n';
                }
            } else {
                param += 1;
                if (streq(param, "help")) {
                    printHelp();
                    return 0;
                } else if (streq(param, "step")) {
                    step = true;
                } else if (streq(param, "cscribe")) {
                    circumscribed = true;
                } else {
                    std::cerr << PSIG "Ignoring unknown flag \"" << argv[i] << '\"' << '\n';
                }
            }
        } else {
            std::cerr << PSIG "Ignoring unknown flag \"" << argv[i] << '\"' << '\n';
        }
    }

    if (glfwInit() == GLFW_FALSE) {
        std::cerr << PSIG "CRITICAL: Failed to init GLFW" << '\n';
        return 1;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, MSAASamples);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(500, 500, "Polygon animation", NULL, NULL);
    if (window == NULL) {
        std::cerr << PSIG "CRITICAL: Failed to create GLFW window" << '\n';
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << PSIG "CRITICAL: Failed to load GLAD" << '\n';
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(window, keyCallback);

    glfwSwapInterval(1);

    GLuint vertexShaderObj = compileShader(GL_VERTEX_SHADER, vertShader);
    GLuint fragmentShaderObj = compileShader(GL_FRAGMENT_SHADER, fragShader);
    GLuint shader = linkShaders(vertexShaderObj, fragmentShaderObj);
    glDeleteShader(vertexShaderObj);
    glDeleteShader(fragmentShaderObj);

    glUseProgram(shader);

    mesh polygon;
    polygon.alloc();

    mesh circle;
    circle.alloc();

    circle.bind();
    circle.indices = computePolygon(circleFineness, scl, circleThickness, circumscribed);

    GLenum error;
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << PSIG "GL ERROR (setup): " << getGLErrorStr(error) << '\n';
    }

    int numberPolygons = startPolygonCount;

    float nextFrame = 0;

    while (!glfwWindowShouldClose(window)) {
        if (step ? stepConfirmed : glfwGetTime() > nextFrame) {
            stepConfirmed = false;
            nextFrame = glfwGetTime() + animationIntervalSeconds;

            glClearColor(0.7, 0.7, 0.7, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            float newLnWid = lineWidth;
            // Uncomment this for a cool effect
            // float prop = static_cast<float>(numberPolygons) / (circleFineness / 10.0f);
            // newLnWid = circleThickness * prop + lineWidth * (1 - prop);

            polygon.bind();
            polygon.indices = computePolygon(numberPolygons, scl, newLnWid, circumscribed);
            glDrawElements(GL_TRIANGLES, polygon.indices, GL_UNSIGNED_INT, 0);
            circle.bind();
            glDrawElements(GL_TRIANGLES, circle.indices, GL_UNSIGNED_INT, 0);

            glfwSwapBuffers(window);

            numberPolygons++;
            if (numberPolygons >= animationEndPC) {
                numberPolygons = startPolygonCount;
            }
        } else {
            // To prevent the hogging of system resources
            std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        }

        glfwPollEvents();

        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << PSIG "GL ERROR (render): " << getGLErrorStr(error) << '\n';
            // Prevent console spam and gracefully exit
            glfwSetWindowShouldClose(window, true);
            exitCode = 1;
        }
    }

    circle.free();
    polygon.free();
    glDeleteProgram(shader);
    glfwDestroyWindow(window);

    glfwTerminate();

    return exitCode;
}

unsigned int computePolygon(int n, float scl, float thickness, bool circum) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    const unsigned int maxVertices = n * 2;

    vertices.reserve(maxVertices * 2);
    indices.reserve(n * 6);

    float m;
    if (circum) {
        m = 1 / std::cos(PI / n);
    } else {
        m = 1;
    }

    float rot = 2 * PI / n;
    float rotation = 0;
    for (int i = 0; i < n; i++) {
        float x = m * std::cos(rotation);
        float y = m * std::sin(rotation);
        float ix = x - x * thickness;
        float iy = y - y * thickness;
        unsigned int ofs = vertices.size() / 2;
        vertices.insert(vertices.end(), {
            x * scl, y * scl,
            ix * scl, iy * scl
        });
        rotation += rot;

        unsigned int nxt = (ofs + 2) % maxVertices;
        indices.insert(indices.end(), {
            ofs + 0, nxt + 1, nxt,
            ofs + 0, ofs + 1, nxt + 1
        });
    }

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    return indices.size();
}

GLuint compileShader(GLenum type, const char *data) {
    GLuint shader;
    shader = glCreateShader(type);
    glShaderSource(shader, 1, &data, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        std::array<char, LOG_BUFFER_SIZE> log;
        glGetShaderInfoLog(shader, log.size(), NULL, log.data());
        glDeleteShader(shader);
        std::cerr << PSIG "Failed to compile shader: " << log.data() << '\n';
        return 0;
    }
    return shader;
}

GLuint linkShaders(GLuint vertObject, GLuint fragObject) {
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertObject);
    glAttachShader(shaderProgram, fragObject);
    glLinkProgram(shaderProgram);
    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        std::array<char, LOG_BUFFER_SIZE> log;
        glGetProgramInfoLog(shaderProgram, log.size(), NULL, log.data());
        glDeleteProgram(shaderProgram);
        std::cerr << PSIG "Failed to link shader: " << log.data() << '\n';
        return 0;
    }
    return shaderProgram;
}

const char *getGLErrorStr(GLenum error) {
    switch (error) {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default: return "-Unknown error-";
    }
}

bool streq(const char *a, const char *b) {
    return std::strcmp(a, b) == 0;
}

void printHelp() {
    std::cout <<
R"(ctri - "Circle TRIangle"
Animation to demonstrate how an inscribed or circumscribed regular
polygon can come close to forming a circle as its sides approach
infinity.

Usage:
ctri [flags [values]]
Options:
    -help                 Prints this message and exits
    --pwidth   [number]   Change the line width of the polygon
    --cwidth   [number]   Change the line width of the circle
    --cres     [integer]  Change the resolution of the circle (how
                          many triangles)
    --interval [number]   Change the number to seconds to wait untl
                          the next frame (animation)
    --pmax     [integer]  Change the number of sides at which the
                          animation will reset
    --pstart   [integer]  Change the starting side count of the
                          polygon (does not effect `--pmax`)
    --samples  [integer]  Change the number of samples to take when
                          doing multisampling (higher values result
                          in smoother graphics)
    --cscale  [number]    Sets the scale of the animation 
    -step                 Change the animation to instead add a side
                          when the SPACEBAR is pressed. Causes
                          `--interval` to have to effect.
    -cscribe              Show a circumscribed polygon instead of an
                          inscribed one

number:  A float
integer: An int. You can prefix with "0x" for hexidecimal and "0" for
         octal

Source code, license information, and bug reports at
<https://github.com/KeinR/ctri/>
)";
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == STEP_KEY && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        stepConfirmed = true;
    }
}
