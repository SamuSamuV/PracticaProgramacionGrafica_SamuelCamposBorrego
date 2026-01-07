// Este código es de dominio público
// samuel.campos@alumnos.udit.es

#ifndef WATER_HEADER
#define WATER_HEADER

#include <glad/gl.h>
#include <string>
#include <glm.hpp>

namespace udit
{
    class Water
    {
    private:
        static const std::string vertex_shader_source;
        static const std::string fragment_shader_source;

        GLuint vao_id;
        GLuint vbo_ids[2];
        GLuint program_id;
        GLuint texture_id;

        GLint  model_view_loc;
        GLint  projection_loc;
        GLint  time_loc;
        GLint  sampler_loc;

        GLsizei vertex_count;

        GLuint compile_shaders(const std::string& vs, const std::string& fs);
        GLuint load_texture(const std::string& path);

    public:
        Water(float width, float depth);
        ~Water();

        void render(const glm::mat4& view, const glm::mat4& projection, float time);
    };
}

#endif