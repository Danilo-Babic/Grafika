//Autor: Danilo Babić SV42/2020
//Opis: Specifikacija 18: Neboderi

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <thread>        
#include <chrono>    

#include <iostream>
#include <map>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

//stb_image.h je header-only biblioteka za ucitavanje tekstura.
//Potrebno je definisati STB_IMAGE_IMPLEMENTATION prije njenog ukljucivanja
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Character {
    unsigned int TextureID;  // ID teksture sa glyfom
    glm::ivec2 Size;         // veličina glyph u pikselima
    glm::ivec2 Bearing;      // offset od bazne linije do levo-gore tacke glyph
    unsigned int Advance;    // horizontalni offset za sledeći karakter
};

std::map<char, Character> Characters;
unsigned int textVAO, textVBO;
unsigned int textShaderProgram;

unsigned int compileShader(GLenum type, const char* source);
unsigned int createShader(const char* vsSource, const char* fsSource);
static unsigned loadImageToTexture(const char* filePath); //Ucitavanje teksture, izdvojeno u funkciju
void renderText(GLuint shader, std::string text, float x, float y, float scale, glm::vec3 color, glm::mat4 projection);
void initTextRendering(const std::string& fontPath);


// Cube vertices with consistent winding order
// Cube vertices with consistent winding order
float cubeVertices[] = {
    // Positions          // Colors
    // Bottom face (CCW when viewed from below)
    -0.5f, 0.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 0.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 0.0f,  0.5f,  0.7f, 0.7f, 0.7f,
    -0.5f, 0.0f,  0.5f,  0.7f, 0.7f, 0.7f,

    // Top face (CCW when viewed from above)
    -0.5f, 1.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 1.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 1.0f,  0.5f,  0.7f, 0.7f, 0.7f,
    -0.5f, 1.0f,  0.5f,  0.7f, 0.7f, 0.7f,

    // Front face (CCW when viewed from front)
    -0.5f, 0.0f,  0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 0.0f,  0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 1.0f,  0.5f,  0.7f, 0.7f, 0.7f,
    -0.5f, 1.0f,  0.5f,  0.7f, 0.7f, 0.7f,

    // Back face (CW when viewed from front)
    -0.5f, 0.0f, -0.5f,  0.7f, 0.7f, 0.7f,
    -0.5f, 1.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 1.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     0.5f, 0.0f, -0.5f,  0.7f, 0.7f, 0.7f,

     // Left face (CCW when viewed from left)
     -0.5f, 0.0f, -0.5f,  0.7f, 0.7f, 0.7f,
     -0.5f, 0.0f,  0.5f,  0.7f, 0.7f, 0.7f,
     -0.5f, 1.0f,  0.5f,  0.7f, 0.7f, 0.7f,
     -0.5f, 1.0f, -0.5f,  0.7f, 0.7f, 0.7f,

     // Right face (CCW when viewed from right)
      0.5f, 0.0f, -0.5f,  0.7f, 0.7f, 0.7f,
      0.5f, 0.0f,  0.5f,  0.7f, 0.7f, 0.7f,
      0.5f, 1.0f,  0.5f,  0.7f, 0.7f, 0.7f,
      0.5f, 1.0f, -0.5f,  0.7f, 0.7f, 0.7f
};

unsigned int cubeIndices[] = {
    // Front face
    8, 9, 10, 10, 11, 8,
    // Back face
    12, 13, 14, 14, 15, 12,
    // Left face
    16, 17, 18, 18, 19, 16,
    // Right face
    20, 21, 22, 22, 23, 20
    // Top and Bottom faces su izostavljene
};
// Ground vertices
float groundVertices[] = {
    -1000.0f, 0.0f, -1000.0f, 0.4f, 0.8f, 0.4f,
     1000.0f, 0.0f, -1000.0f, 0.4f, 0.8f, 0.4f,
     1000.0f, 0.0f,  1000.0f, 0.4f, 0.8f, 0.4f,
    -1000.0f, 0.0f,  1000.0f, 0.4f, 0.8f, 0.4f
};

unsigned int groundIndices[] = {
    0, 1, 2, 2, 3, 0
};


// Kamera
glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
float yaw = -90.0f;
float pitch = 0.0f;
float cameraSpeed = 0.1f;
float cameraSensitivity = 1.5f;

glm::vec3 getCameraFront() {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(front);
}

bool isCollidingWithBuildings(glm::vec3 pos) {
    float halfSize = 0.75f;
    for (int x = -10; x <= 10; x++) {
        for (int z = -10; z <= 10; z++) {
            glm::vec3 bPos = glm::vec3(x * 5.0f, 0.0f, z * 5.0f);
            float height = 5.0f + std::abs(std::sin(x * z)) * 20.0f;

            if (pos.x > bPos.x - halfSize && pos.x < bPos.x + halfSize &&
                pos.z > bPos.z - halfSize && pos.z < bPos.z + halfSize &&
                pos.y < height)
                return true;
        }
    }
    return false;
}

glm::vec3 groundColor(0.8f, 0.8f, 0.8f);
glm::vec3 skyColor(0.6f, 0.8f, 1.0f);

// Pomoćna funkcija za generisanje boja po poziciji
glm::vec3 colorForBuilding(int x, int z) {
    float r = std::abs(std::sin(x * 0.3f + z * 0.1f));
    float g = std::abs(std::cos(z * 0.5f + x * 0.2f));
    float b = std::abs(std::sin(x * 0.2f - z * 0.3f));
    return glm::vec3(r, g, b);
}


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Neboder Sim", nullptr, nullptr);
    if (!window) {
        std::cout << "Prozor nije kreiran!\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();

    initTextRendering("res/Roboto.ttf");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_FRONT);       // Sakrivaj zadnje strane
    //glFrontFace(GL_CCW);       // Koristi counter-clockwise kao prednju stranu

    unsigned shader = createShader("basic.vert", "basic.frag");
    textShaderProgram = createShader("text.vert", "text.frag");

    // --- Podloga ---
    float planeVertices[] = {
        -1000.0f, 0.0f, -1000.0f, groundColor.r, groundColor.g, groundColor.b,
         1000.0f, 0.0f, -1000.0f, groundColor.r, groundColor.g, groundColor.b,
         1000.0f, 0.0f,  1000.0f, groundColor.r, groundColor.g, groundColor.b,
        -1000.0f, 0.0f,  1000.0f, groundColor.r, groundColor.g, groundColor.b,
    };
    unsigned int planeIndices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int groundVAO, groundVBO, groundEBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices), planeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // --- Neboderi ---
    unsigned int buildingVAO, buildingVBO, buildingEBO;
    glGenVertexArrays(1, &buildingVAO);
    glGenBuffers(1, &buildingVBO);
    glGenBuffers(1, &buildingEBO);

    glBindVertexArray(buildingVAO);
    glBindBuffer(GL_ARRAY_BUFFER, buildingVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buildingEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    const double targetFrameTime = 1.0 / 60.0;

    // --- Glavna petlja ---
    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now(); //  za FPS kontrolu
        glm::vec3 oldPos = cameraPos;
        glm::vec3 front = getCameraFront();
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) //  izlaz na ESC
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += front * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= front * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= right * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += right * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            yaw -= cameraSensitivity;
            //glDisable(GL_CULL_FACE);
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            yaw += cameraSensitivity;
            //glDisable(GL_CULL_FACE);
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            skyColor = glm::vec3(0.6f, 0.8f, 1.0f);
            groundColor = glm::vec3(0.8f, 0.8f, 0.8f);
        }
        else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            skyColor = glm::vec3(0.02f, 0.02f, 0.1f);
            groundColor = glm::vec3(0.2f, 0.2f, 0.2f);
        }

        if (isCollidingWithBuildings(cameraPos)) cameraPos = oldPos;

        // --- Render ---
        glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + getCameraFront(), glm::vec3(0, 1, 0));
        //glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 800.0f, 0.1f, 1000.0f);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 800.0f, 0.1f, 10000.0f);

        // Podloga
        glBindVertexArray(groundVAO);
        glm::mat4 groundModel = glm::mat4(1.0f);
        glm::mat4 mvp = projection * view * groundModel;
        glUniformMatrix4fv(glGetUniformLocation(shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3fv(glGetUniformLocation(shader, "uColorOverride"), 1, glm::value_ptr(groundColor));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Neboderi
        glBindVertexArray(buildingVAO);
        for (int x = -10; x <= 10; x++) {
            for (int z = -10; z <= 10; z++) {
                float height = 5.0f + std::abs(std::sin(x * z)) * 20.0f;
                glm::vec3 color = colorForBuilding(x, z);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x * 5.0f, 0.0f, z * 5.0f));
                model = glm::scale(model, glm::vec3(1.5f, height, 1.5f));

                glm::mat4 mvp = projection * view * model;
                glUniformMatrix4fv(glGetUniformLocation(shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
                glUniform3fv(glGetUniformLocation(shader, "uColorOverride"), 1, glm::value_ptr(color));
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
        }

        glm::mat4 ortho = glm::ortho(0.0f, 1280.0f, 0.0f, 800.0f);
        renderText(textShaderProgram, "DANILO BABIC SV42/2020", 700.0f, 50.0f, 1.0f, glm::vec3(1.0f, 0.0f, 0.0f), ortho);
        glfwSwapBuffers(window);


        glfwPollEvents();

        // --- Ograničenje na 60 FPS ---
        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = frameEnd - frameStart;
        double sleepTime = targetFrameTime - elapsed.count();
        if (sleepTime > 0)
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
    }

    glDeleteBuffers(1, &groundVBO);
    glDeleteBuffers(1, &groundEBO);
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &buildingVBO);
    glDeleteVertexArrays(1, &buildingVAO);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}

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


// =========================== POMOCNA ZA TEKST ======================================

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