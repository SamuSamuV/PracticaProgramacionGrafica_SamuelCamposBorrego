#pragma once
#include "Node.hpp"
#include <glad/gl.h>
#include <string>

namespace udit
{
    class Water : public Node
    {
    private:
        GLuint vao_id;
        GLuint vbo_ids[2];
        GLuint program_id;
        GLuint texture_id;

        GLint model_loc, view_loc, projection_loc;
        GLint time_loc, sampler_loc;
        GLsizei vertex_count;

        float time_accum; // Variable para acumular el tiempo y animar el agua

        GLuint compile_shaders();
        GLuint load_texture(const std::string& path);

    public:
        Water(float width, float depth);
        ~Water();

        // Necesitamos update para sumar tiempo
        virtual void update() override;

        virtual void render(const Camera& camera, Light* light = nullptr) override;
    };
}