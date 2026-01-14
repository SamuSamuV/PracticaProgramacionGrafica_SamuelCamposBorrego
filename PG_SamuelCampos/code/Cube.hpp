#pragma once
#include "Node.hpp"
#include "Light.hpp"
#include <glad/gl.h>
#include <string>

namespace udit
{
    class Cube : public Node
    {
    private:
        // Identificadores para nuestros buffers de datos
        enum {
            COORDINATES_VBO, // Posiciones (x,y,z)
            TEXTURE_UVS_VBO, // Coordenadas de textura (u,v)
            NORMALS_VBO,     // Normales para la luz
            INDICES_IBO,     // Orden de dibujado
            VBO_COUNT
        };

        // Datos estáticos de la geometría del cubo
        static const GLfloat coordinates[];
        static const GLfloat texture_uvs[];
        static const GLfloat normals[];
        static const GLubyte indices[];

        // IDs de OpenGL
        GLuint vbo_ids[VBO_COUNT];
        GLuint vao_id;
        GLuint program_id; // El Shader Program
        GLuint texture1_id, texture2_id;

        // Locations de los uniforms (las "conexiones" con el shader)
        GLint model_loc, view_loc, proj_loc, normal_matrix_loc;
        GLint alpha_loc;
        GLint light_pos_loc, light_color_loc, light_ambient_loc, light_diffuse_loc, view_pos_loc;

        float opacity; // Para controlar la transparencia

        void compile_shaders();
        GLuint load_texture(const std::string& path);

    public:
        Cube();
        ~Cube();

        void set_opacity(float o) { opacity = o; }

        // Renderizamos el cubo usando la cámara y la luz que nos pasan
        virtual void render(const Camera& camera, Light* light = nullptr) override;
    };
}