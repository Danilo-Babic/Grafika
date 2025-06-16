//Autor: Danilo Babić SV 42/2020
//Opis: 2d projekat specifikacija bespilotna letelica (spec br 6)

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>           // Za glm::vec2, glm::vec4
#include <glm/gtc/type_ptr.hpp>  // Za glm::value_ptr
#include <iomanip>
#include <map>
#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <chrono>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//stb_image.h je header-only biblioteka za ucitavanje tekstura.
//Potrebno je definisati STB_IMAGE_IMPLEMENTATION prije njenog ukljucivanja
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"



// Globalne konstante
const float INITIAL_RADIUS = 0.03f;
const float RADIUS_GROWTH_RATE = 0.2f;
const int CIRCLE_SEGMENTS = 100;


unsigned int zoneCircleVAO;
glm::vec2 zonePos = glm::vec2(0.055f, -0.30f);   // Početna pozicija zone
float zoneRadius = 0.03f;                        // Početni radijus

bool dragging = false;
bool growing = false;
glm::vec2 dragOffset = glm::vec2(0.0f);

glm::vec4 originalColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.3f); // crvena
glm::vec4 activeColor = glm::vec4(0.0f, 0.0f, 1.0f, 0.3f); // plava
glm::vec4 currentColor = originalColor;


struct Drone {
    glm::vec2 position;
    float radius;
    bool active;
    float battery; // 0.0 - 1.0
    bool destroyed = false;
    std::string destroyedMessage;
};

Drone drone1 = { glm::vec2(-0.5f, 0.0f), 0.05f, false, 1.0f };
Drone drone2 = { glm::vec2(0.5f, 0.0f), 0.05f, false, 1.0f };

const float BATTERY_DRAIN_RATE = 0.01f; // per frame when active

unsigned int circleVAO;
unsigned int ledVAO;
unsigned int droneShader;
unsigned int ledShader;

unsigned int compileShader(GLenum type, const char* source);
unsigned int createShader(const char* vsSource, const char* fsSource);
static unsigned loadImageToTexture(const char* filePath); //Ucitavanje teksture, izdvojeno u funkciju
void renderText(const std::string& text, float x, float y, float scale, const glm::vec3& color);

unsigned int setupFullScreenQuad();
unsigned int loadTexture(const char* filePath);
void setupShaderUniforms(unsigned int shaderProgram, float overlayAlpha);
void drawTexturedQuad(unsigned int shaderProgram, unsigned int VAO, unsigned int texture, float overlayAlpha);

void renderZoneIndicator(unsigned int shader, unsigned int VAO, const glm::vec2& pos, const glm::vec4& color);
unsigned int createZoneCircleVAO(float radius, int segments, float aspectRatio);

bool isMouseInsideZone(double mouseX, double mouseY, float radius, const glm::vec2& position, float aspectRatio);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

unsigned int createCircleVAO(float radius, int segments, float aspectRatio);
void renderDrone(const Drone& drone, unsigned int shader, unsigned int vao);
void renderLEDIndicator(const Drone& drone, glm::vec2 screenPos, unsigned int shader, unsigned int vao);
bool checkCollision(const Drone& a, const Drone& b, float radius);
bool isInsideZone(const Drone& drone, const glm::vec2& zoneCenter, float zoneRadius, float aspectRatio);
bool isOutsideMap(const glm::vec2& pos);
void destroyDrone(Drone& drone, const std::string& name);
void destroyDrone(Drone& drone, const std::string& name);
unsigned int createBatteryBarVAO(glm::vec2 pos, glm::vec2 size);
void renderBatteryUI(const Drone& drone, const glm::vec2& pos, const glm::vec2& size, unsigned int shader);

void renderText(GLuint shader, std::string text, float x, float y, float scale, glm::vec3 color, glm::mat4 projection);
void initTextRendering(const std::string& fontPath);

constexpr float TARGET_FPS = 60.0f;
constexpr float FRAME_TIME = 1.0f / TARGET_FPS;

struct Character {
    GLuint TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    GLuint Advance;
};

std::map<GLchar, Character> Characters;
GLuint textVAO, textVBO;



int main()
{
    if (!glfwInit())
    {
        std::cerr << "GLFW initialization failed!\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Zabranjena zona letenja - Lopare", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window!\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // **Ograničenje na 60 FPS**
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "GLEW initialization failed!\n";
        glfwTerminate();
        return -1;
    }

    // Callback-ovi
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Transparentnost
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shader i tekstura pozadine
    unsigned int backgroundShader = createShader("basic.vert", "background.frag");
    unsigned int quadVAO = setupFullScreenQuad();
    unsigned int backgroundTexture = loadTexture("res/MAJEVICA_LOPARE.jpg");

    // Shader za zonu
    unsigned int zoneShader = createShader("zone.vert", "zone.frag");

    // Shader za dron
    unsigned int droneShader = createShader("circle.vert", "circle.frag");

    // Shader za bateriju
    unsigned int batteryShader = createShader("battery.vert", "battery.frag");

    unsigned int textShader = createShader("text.vert", "text.frag");
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(1280), 0.0f, static_cast<float>(800));
    initTextRendering("res/Roboto.ttf");


    
    // Pozicije za battery bar-ove
    glm::vec2 batteryBarSize = glm::vec2(0.3f, 0.05f);
    glm::vec2 batteryPos1 = glm::vec2(-0.9f, 0.8f);  // prvi bar levo gore
    glm::vec2 batteryPos2 = glm::vec2(-0.9f, -0.35f); // drugi bar ispod

    // VAO za sivi background bar
    unsigned int backgroundVAO = createBatteryBarVAO(glm::vec2(0.0f), batteryBarSize);

    // Aspect ratio
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspectRatio = static_cast<float>(width) / height;

    // VAO za zonu
    zoneCircleVAO = createZoneCircleVAO(zoneRadius, CIRCLE_SEGMENTS, aspectRatio);
    unsigned int circleVAO = createCircleVAO(zoneRadius, CIRCLE_SEGMENTS, aspectRatio);

    while (!glfwWindowShouldClose(window))
    {
        using clock = std::chrono::high_resolution_clock;
        auto lastTime = clock::now();
        const double targetFrameTime = 1.0 / 60.0; // 60 FPS


        processInput(window);

        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Crtanje pozadine
        drawTexturedQuad(backgroundShader, quadVAO, backgroundTexture, 0.3f);

        // Ažuriranje zone ako raste
        if (growing)
        {
            zoneRadius += RADIUS_GROWTH_RATE * 0.01f;
            zoneCircleVAO = createZoneCircleVAO(zoneRadius, CIRCLE_SEGMENTS, aspectRatio);

            // Uništavanje ako zona "nagrize" aktivne dronove
            if (drone1.active && isInsideZone(drone1, zonePos, zoneRadius, aspectRatio)) {
                destroyDrone(drone1, "1");
            }
            if (drone2.active && isInsideZone(drone2, zonePos, zoneRadius, aspectRatio)) {
                destroyDrone(drone2, "2");
            }
        }

        // Provera kolizije i uslova za uništenje
        if (drone1.active && drone2.active && checkCollision(drone1, drone2, INITIAL_RADIUS)) {
            destroyDrone(drone1, "1");
            destroyDrone(drone2, "2");
        }
        if (drone1.active && isInsideZone(drone1, zonePos, zoneRadius, aspectRatio)) {
            destroyDrone(drone1, "1");
        }
        if (drone2.active && isInsideZone(drone2, zonePos, zoneRadius, aspectRatio)) {
            destroyDrone(drone2, "2");
        }
        if (drone1.active && isOutsideMap(drone1.position)) {
            destroyDrone(drone1, "1");
        }
        if (drone2.active && isOutsideMap(drone2.position)) {
            destroyDrone(drone2, "2");
        }
        if (drone1.active && drone1.battery <= 0.0f) {
            destroyDrone(drone1, "1");
        }
        if (drone2.active && drone2.battery <= 0.0f) {
            destroyDrone(drone2, "2");
        }

        // Iscrtavanje zone
        renderZoneIndicator(zoneShader, zoneCircleVAO, zonePos, currentColor);

        // Baterije opadaju
        if (drone1.active && drone1.battery > 0.0f)
            drone1.battery -= BATTERY_DRAIN_RATE * 0.01f;
        if (drone2.active && drone2.battery > 0.0f)
            drone2.battery -= BATTERY_DRAIN_RATE * 0.01f;

        // Crtanje dronova
        if (!drone1.destroyed) {
            renderDrone(drone1, droneShader, circleVAO);
        }
        //renderDrone(drone1, droneShader, circleVAO);
        if (!drone2.destroyed) {
            renderDrone(drone2, droneShader, circleVAO);
        }
        //renderDrone(drone2, droneShader, circleVAO);

        // LED indikatori
        renderLEDIndicator(drone1, glm::vec2(-0.9f, 0.85f), droneShader, circleVAO);
        renderLEDIndicator(drone2, glm::vec2(-0.8f, 0.85f), droneShader, circleVAO);

        // Battery UI
        renderBatteryUI(drone1, batteryPos1, batteryBarSize, batteryShader);
        renderBatteryUI(drone2, batteryPos2, batteryBarSize, batteryShader);

        if (drone1.destroyed) {
            renderText(textShader, "Dron 1 unisten!", 500.0f, 700.0f, 1.0f, glm::vec3(1.0f, 0.0f, 0.0f), projection);
        }
        if (drone2.destroyed) {
            renderText(textShader, "Dron 2 unisten!", 500.0f, 650.0f, 1.0f, glm::vec3(1.0f, 0.0f, 0.0f), projection);
        }

        // --- Baterija tekst: procenat ---
        if (drone1.active) {
            std::string batteryText1 = std::to_string(static_cast<int>(drone1.battery * 100)) + "%";
            renderText(textShader, batteryText1,
                150.0f, // X pozicija u pikselima
                702.0f, // Y pozicija u pikselima
                0.5f, glm::vec3(1.0f, 0.0f, 0.0f), projection);
        }

        if (drone2.active) {
            std::string batteryText2 = std::to_string(static_cast<int>(drone2.battery * 100)) + "%";
            renderText(textShader, batteryText2,
                150.0f,
                242.0f,
                0.5f, glm::vec3(1.0f, 0.0f, 0.0f), projection);
        }

        // --- Koordinate ---
        if (drone1.active) {
            std::ostringstream ss;
            ss << "X: " << std::fixed << std::setprecision(2) << drone1.position.x
                << " Y: " << drone1.position.y;
            renderText(textShader, ss.str(),
                100.0f, 680.0f,
                0.4f, glm::vec3(1.0f, 0.0f, 0.0f), projection);
        }

        if (drone2.active) {
            std::ostringstream ss;
            ss << "X: " << std::fixed << std::setprecision(2) << drone2.position.x
                << " Y: " << drone2.position.y;
            renderText(textShader, ss.str(),
                100.0f, 220.0f,
                0.4f, glm::vec3(1.0f, 0.0f, 0.0f), projection);
        }

        renderText(textShader, "DANILO BABIC SV42/2020", 970.0f, 15.0f, 0.6f, glm::vec3(1.0f, 0.0f, 0.0f), projection);

        auto now = clock::now();
        std::chrono::duration<double> elapsed = now - lastTime;
        double sleepTime = targetFrameTime - elapsed.count();

        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }
        lastTime = clock::now();
        glfwSwapBuffers(window);
        glfwPollEvents();

    }

    // Čišćenje
    // Brisanje shader-a
    glDeleteProgram(backgroundShader);
    glDeleteProgram(zoneShader);
    glDeleteProgram(droneShader);
    glDeleteProgram(batteryShader);
    glDeleteProgram(textShader);

// Brisanje VAO-ova
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteVertexArrays(1, &backgroundVAO);
    glDeleteVertexArrays(1, &circleVAO);

// Brisanje tekstura
    glDeleteTextures(1, &backgroundTexture);

// Ako koristiš FreeType, oslobodi resurse:
    //cleanUpTextRendering(); // tvoja funkcija iz textRenderer-a

    glDeleteVertexArrays(1, &zoneCircleVAO);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}


//============================================================ POMOCNE FUNKCIJE ZA DRONOVE  ==============================================================
unsigned int createCircleVAO(float radius, int segments, float aspectRatio) {
    std::vector<float> vertices;
    vertices.push_back(0.0f); // center
    vertices.push_back(0.0f);

    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
        vertices.push_back(radius * cos(angle) / aspectRatio);
        vertices.push_back(radius * sin(angle));
    }

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return vao;
}

void renderDrone(const Drone& drone, unsigned int shader, unsigned int vao) {
    glUseProgram(shader);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(drone.position, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));

    // Baterija utiče na boju
    glm::vec3 color = drone.active ? glm::vec3(0.2f + drone.battery, 1.0f * drone.battery, 0.2f) : glm::vec3(0.3f, 0.3f, 0.3f);
    glUniform3f(glGetUniformLocation(shader, "uColor"), color.r, color.g, color.b);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    glBindVertexArray(0);
}

void processInput(GLFWwindow* window)
{
    static bool key1Pressed = false;
    static bool key2Pressed = false;

    // Očitaj sve tastere
    bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    bool keyU = glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS;
    bool keyI = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;

    // Aktivacija/deaktivacija
    if (key1 && keyU && !key1Pressed) {
        drone1.active = true;
        key1Pressed = true;
    }
    else if (key1 && keyI && !key1Pressed) {
        drone1.active = false;
        key1Pressed = true;
    }
    else if (!key1) {
        key1Pressed = false;
    }

    if (key2 && keyU && !key2Pressed) {
        drone2.active = true;
        key2Pressed = true;
    }
    else if (key2 && keyI && !key2Pressed) {
        drone2.active = false;
        key2Pressed = true;
    }
    else if (!key2) {
        key2Pressed = false;
    }

    // Upravljanje dronom 1 (WASD)
    if (drone1.active) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            drone1.position.y += 0.01f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            drone1.position.y -= 0.01f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            drone1.position.x -= 0.01f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            drone1.position.x += 0.01f;
    }

    // Upravljanje dronom 2 (strelice)
    if (drone2.active) {
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            drone2.position.y += 0.01f;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            drone2.position.y -= 0.01f;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            drone2.position.x -= 0.01f;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            drone2.position.x += 0.01f;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void renderLEDIndicator(const Drone& drone, glm::vec2 screenPos, unsigned int shader, unsigned int vao) {
    glUseProgram(shader);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(screenPos, 0.0f));
    model = glm::scale(model, glm::vec3(0.02f)); // LED size
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));

    glm::vec3 color = drone.active ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.2f, 0.2f, 0.2f);
    glUniform3f(glGetUniformLocation(shader, "uColor"), color.r, color.g, color.b);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    glBindVertexArray(0);
}

bool checkCollision(const Drone& a, const Drone& b, float radius) {
    return glm::distance(a.position, b.position) <= radius;
}


bool isInsideZone(const Drone& drone, const glm::vec2& zoneCenter, float zoneRadius, float aspectRatio) {
    // Korekcija X koordinata zbog aspect ratio
    glm::vec2 adjustedDronePos = glm::vec2(drone.position.x * aspectRatio, drone.position.y);
    glm::vec2 adjustedZoneCenter = glm::vec2(zoneCenter.x * aspectRatio, zoneCenter.y);
    return glm::distance(adjustedDronePos, adjustedZoneCenter) <= zoneRadius;
}

bool isOutsideMap(const glm::vec2& pos) {
    return pos.x < -1.0f || pos.x > 1.0f || pos.y < -1.0f || pos.y > 1.0f;
}

void destroyDrone(Drone& drone, const std::string& name) {
    drone.active = false;
    drone.destroyed = true;
    drone.destroyedMessage = "Letjelica " + name + " je unistena!";
}

unsigned int createBatteryBarVAO(glm::vec2 pos, glm::vec2 size) {
    float vertices[] = {
        pos.x,          pos.y,
        pos.x + size.x, pos.y,
        pos.x + size.x, pos.y - size.y,

        pos.x,          pos.y,
        pos.x + size.x, pos.y - size.y,
        pos.x,          pos.y - size.y
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glDeleteBuffers(1, &VBO);

    return VAO;
}

void renderBatteryUI(const Drone& drone, const glm::vec2& pos, const glm::vec2& size, unsigned int shader) {
    if (!drone.active) return;

    float percent = glm::clamp(drone.battery, 0.0f, 1.0f);
    glm::vec2 fillSize = glm::vec2(size.x * percent, size.y);

    // Pozadina
    unsigned int bgVAO = createBatteryBarVAO(pos, size);
    glUseProgram(shader);
    glUniform3f(glGetUniformLocation(shader, "uColor"), 0.2f, 0.2f, 0.2f);
    glBindVertexArray(bgVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteVertexArrays(1, &bgVAO);

    // Punjenje
    unsigned int fillVAO = createBatteryBarVAO(pos, fillSize);
    glUniform3f(glGetUniformLocation(shader, "uColor"), 1.0f - percent, percent, 0.0f);
    glBindVertexArray(fillVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteVertexArrays(1, &fillVAO);

    // Tekst: % i koordinate
    std::string batteryText = std::to_string((int)drone.battery) + "%";
    renderText(batteryText, pos.x + size.x * 0.4f, pos.y - 0.02f, 0.0018f, glm::vec3(1.0f));

    std::ostringstream oss;
    //oss << "X: " << std::fixed << std::setprecision(2) << drone.position.x
    //    << " Y: " << std::fixed << std::setprecision(2) << drone.position.y;

    //renderText(oss.str(), pos.x, pos.y - size.y * 1.6f, 0.0015f, glm::vec3(1.0f));
}
//============================================================ POMOCNE FUNKCIJE ZA POZADINU ====================================================
unsigned int setupFullScreenQuad() {
    float vertices[] = {
        //  X      Y      S     T
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return VAO;
}
unsigned int loadTexture(const char* filePath) {
    unsigned int texture = loadImageToTexture(filePath); // mora da vraća texture ID
    if (texture == 0) {
        std::cerr << "Texture loading failed: " << filePath << std::endl;
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}
void setupShaderUniforms(unsigned int shaderProgram, float overlayAlpha) {
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTex"), 0); // GL_TEXTURE0
    glUniform1f(glGetUniformLocation(shaderProgram, "overlayAlpha"), overlayAlpha);
    glUseProgram(0);
}
void drawTexturedQuad(unsigned int shader, unsigned int VAO, unsigned int texture, float overlayAlpha) {
    glUseProgram(shader);

    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glUniform1f(glGetUniformLocation(shader, "overlayAlpha"), overlayAlpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}


//============================================================= POMOCNE FUNKCIJE ZA ZABRANJENU ZONU ==========================================================
void renderZoneIndicator(unsigned int shader, unsigned int VAO, const glm::vec2& pos, const glm::vec4& color)
{
    glUseProgram(shader);

    int transformLoc = glGetUniformLocation(shader, "transform");
    int colorLoc = glGetUniformLocation(shader, "color");

    glUniform2fv(transformLoc, 1, glm::value_ptr(pos));
    glUniform4fv(colorLoc, 1, glm::value_ptr(color));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    glBindVertexArray(0);
}

unsigned int createZoneCircleVAO(float radius, int segments, float aspectRatio)
{
    std::vector<float> vertices;
    vertices.push_back(0.0f); // centar kruga X
    vertices.push_back(0.0f); // centar kruga Y

    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * M_PI * float(i) / float(segments);
        float x = (radius * cosf(theta)) / aspectRatio;
        float y = radius * sinf(theta);
        vertices.push_back(x);
        vertices.push_back(y);
    }

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    return VAO;
}




// ================================================================= POMOCNE FUNKCIJE MOUSE I KEYBOARD ZABRANJENA ZONA =================================================================
bool isMouseInsideZone(double mouseX, double mouseY, float radius, const glm::vec2& position, float aspectRatio)
{
    // NDC koordinate miša
    float ndcX = static_cast<float>(mouseX) / 640.0f - 1.0f;   // 1280/2
    float ndcY = 1.0f - static_cast<float>(mouseY) / 400.0f;   // 800/2

    ndcX *= aspectRatio;  // uzimanje u obzir širine prozora

    float dx = ndcX - position.x;
    float dy = ndcY - position.y;
    return dx * dx + dy * dy <= radius * radius;
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        dragging = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        dragging = false;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        growing = true;
        currentColor = activeColor;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        growing = false;
        currentColor = originalColor;
    }
}


void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (dragging) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspectRatio = static_cast<float>(width) / height;

        float ndcX = static_cast<float>(xpos) / (width / 2.0f) - 1.0f;
        float ndcY = 1.0f - static_cast<float>(ypos) / (height / 2.0f);
        //ndcX *= aspectRatio;

        zonePos = glm::vec2(ndcX, ndcY);
    }
}


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        float aspectRatio = static_cast<float>(width) / height;
        float ndcX = static_cast<float>(xpos) / (width / 2.0f) - 1.0f;
        float ndcY = 1.0f - static_cast<float>(ypos) / (height / 2.0f);
        //ndcX *= aspectRatio;

        zonePos = glm::vec2(0.055f, -0.30f);
        zoneRadius = INITIAL_RADIUS;
        currentColor = originalColor;

        if (zoneCircleVAO != 0) {
            glDeleteVertexArrays(1, &zoneCircleVAO);
        }

        zoneCircleVAO = createZoneCircleVAO(zoneRadius, CIRCLE_SEGMENTS, aspectRatio);
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}



// ============================================= POMOCNA ZA ISCRTAVANJE STUDENT INFO ===================================================
void renderStudentInfo(unsigned int studentInfoVAO, unsigned int studentTexture, unsigned int shaderProgram)
{
    glUseProgram(shaderProgram);

    glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0);
    float translationX = 0.5;
    float translationY = -0.72;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, studentTexture);
    glUniform2f(glGetUniformLocation(shaderProgram, "transform"), translationX, translationY);

    glBindVertexArray(studentInfoVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}






// ================================================= POMOCNE FUNKCIJE IZ SABLONA ========================================

unsigned int compileShader(GLenum type, const char* source)
{
    std::string content = "";
    std::ifstream file(source);
    std::stringstream ss;
    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Uspjesno procitao fajl sa putanje \"" << source << "\"!" << std::endl;
    }
    else {
        ss << "";
        std::cout << "Greska pri citanju fajla sa putanje \"" << source << "\"!" << std::endl;
    }
     std::string temp = ss.str();
     const char* sourceCode = temp.c_str();

    int shader = glCreateShader(type);
    
    int success;
    char infoLog[512];
    glShaderSource(shader, 1, &sourceCode, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        if (type == GL_VERTEX_SHADER)
            printf("VERTEX");
        else if (type == GL_FRAGMENT_SHADER)
            printf("FRAGMENT");
        printf(" sejder ima gresku! Greska: \n");
        printf(infoLog);
    }
    return shader;
}
unsigned int createShader(const char* vsSource, const char* fsSource)
{
    
    unsigned int program;
    unsigned int vertexShader;
    unsigned int fragmentShader;

    program = glCreateProgram();

    vertexShader = compileShader(GL_VERTEX_SHADER, vsSource);
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource);

    
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    glValidateProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(program, 512, NULL, infoLog);
        std::cout << "Objedinjeni sejder ima gresku! Greska: \n";
        std::cout << infoLog << std::endl;
    }

    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}
static unsigned loadImageToTexture(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;
    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);
    if (ImageData != NULL)
    {
        //Slike se osnovno ucitavaju naopako pa se moraju ispraviti da budu uspravne
        stbi__vertical_flip(ImageData, TextureWidth, TextureHeight, TextureChannels);

        // Provjerava koji je format boja ucitane slike
        GLint InternalFormat = -1;
        switch (TextureChannels) {
        case 1: InternalFormat = GL_RED; break;
        case 2: InternalFormat = GL_RG; break;
        case 3: InternalFormat = GL_RGB; break;
        case 4: InternalFormat = GL_RGBA; break;
        default: InternalFormat = GL_RGB; break;
        }

        unsigned int Texture;
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, TextureWidth, TextureHeight, 0, InternalFormat, GL_UNSIGNED_BYTE, ImageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        // oslobadjanje memorije zauzete sa stbi_load posto vise nije potrebna
        stbi_image_free(ImageData);
        return Texture;
    }
    else
    {
        std::cout << "Textura nije ucitana! Putanja texture: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return 0;
    }
}

void renderText(const std::string& text, float x, float y, float scale, const glm::vec3& color) {
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);

    glColor3f(color.r, color.g, color.b);

    char buffer[99999]; // Enough for ~1000 characters
    int num_quads = stb_easy_font_print(0.0f, 0.0f, const_cast<char*>(text.c_str()), nullptr, buffer, sizeof(buffer));

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);

    glPopMatrix();
}


// ================================================= POMOCNE FUNKCIJE ZA ISPIS TEKSTA =======================================
void initTextRendering(const std::string& fontPath) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library\n";
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font\n";
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    for (GLubyte c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "Failed to load Glyph " << c << "\n";
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<GLuint>(face->glyph->advance.x)
        };

        Characters.insert(std::pair<GLchar, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
void renderText(GLuint shader, std::string text, float x, float y, float scale, glm::vec3 color, glm::mat4 projection) {
    glUseProgram(shader);
    glUniform3f(glGetUniformLocation(shader, "textColor"), color.x, color.y, color.z);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (char c : text) {
        Character ch = Characters[c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ❗ Pomeri x poziciju za sledeći karakter
        x += (ch.Advance >> 6) * scale; // Advance je u 1/64 pixela, pa shiftuj
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}