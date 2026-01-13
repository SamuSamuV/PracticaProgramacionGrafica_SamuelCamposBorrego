// Este código es de dominio público
// samuel.campos@alumnos.udit.es

#ifndef NODE_HEADER
#define NODE_HEADER

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <glad/gl.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <SOIL2.h>

namespace udit
{
    class Node
    {
    protected:
        glm::mat4 transform;
        std::vector<std::shared_ptr<Node>> children;

    public:
        Node() : transform(glm::mat4(1.0f)) {}
        virtual ~Node() = default;

        void add_child(std::shared_ptr<Node> child) {
            children.push_back(child);
        }

        void set_transform(const glm::mat4& matrix) { transform = matrix; }
        glm::mat4 get_transform() const { return transform; }

        // Método recursivo que recorre el grafo
        virtual void render(const glm::mat4& parent_transform, const glm::mat4& view, const glm::mat4& proj, float time,
            const glm::vec3& fog_color, float fog_near, float fog_far)
        {
            glm::mat4 global_transform = parent_transform * transform;

            // Se dibuja a sí mismo
            draw(global_transform, view, proj, time, fog_color, fog_near, fog_far);

            // Dibuja a los hijos (recursión)
            for (auto& child : children) {
                child->render(global_transform, view, proj, time, fog_color, fog_near, fog_far);
            }
        }

        // Cada objeto implementará su propio dibujo aquí
        virtual void draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj, float time,
            const glm::vec3& fog_color, float fog_near, float fog_far) {
        }

    public:
        // --- UTILIDADES ESTÁTICAS ---
        static GLuint load_texture(const std::string& path) {
            int w, h, c;
            unsigned char* data = SOIL_load_image(path.c_str(), &w, &h, &c, SOIL_LOAD_RGBA);
            if (!data) { std::cerr << "Error textura: " << path << std::endl; return 0; }
            GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            SOIL_free_image_data(data);
            return id;
        }

        static GLuint compile_shaders(const std::string& vs_src, const std::string& fs_src) {
            GLuint p = glCreateProgram();
            GLuint v = glCreateShader(GL_VERTEX_SHADER);
            GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
            const char* vs = vs_src.c_str(); const char* fs = fs_src.c_str();
            glShaderSource(v, 1, &vs, NULL); glShaderSource(f, 1, &fs, NULL);
            glCompileShader(v); glCompileShader(f);
            glAttachShader(p, v); glAttachShader(p, f);
            glLinkProgram(p);
            glDeleteShader(v); glDeleteShader(f);
            return p;
        }
    };
}
#endif