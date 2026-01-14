#pragma once
#include "Node.hpp"
#include "Light.hpp"
#include <glad/gl.h>
#include <vector>
#include <string>

namespace udit
{
    class Terrain : public Node
    {
    private:
        enum { COORDINATES_VBO, TEXTURE_UVS_VBO, INDICES_VBO, VBO_COUNT };
        GLuint vao_id;
        GLuint vbo_ids[VBO_COUNT];
        GLsizei number_of_indices;

        GLuint shader_program_id;
        GLuint height_map_id;   // Textura para la altura (blanco=alto, negro=bajo)
        GLuint diffuse_map_id;  // Textura de color (la roca/tierra)

        // Locations
        GLint model_loc, view_loc, proj_loc;
        GLint normal_matrix_loc;
        GLint sampler_height_loc, sampler_diffuse_loc;
        GLint max_height_loc;
        // Iluminación y Niebla
        GLint light_pos_loc, light_color_loc;
        GLint fog_near_loc, fog_far_loc, fog_color_loc;

        void compile_shaders();
        GLuint load_texture(const std::string& path);

    public:
        // width/depth: Tamaño físico. x/z_slices: Resolución de la malla.
        Terrain(float width, float depth, unsigned x_slices, unsigned z_slices);
        ~Terrain();

        virtual void render(const Camera& camera, Light* light = nullptr) override;
    };
}