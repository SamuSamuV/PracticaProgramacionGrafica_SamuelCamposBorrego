#pragma once
#include "Node.hpp"
#include <vector>
#include <string>
#include <glad/gl.h>

namespace udit
{
    class Tree : public Node
    {
    private:
        GLuint vao_id;
        GLuint vbo_ids[3]; // 0: Posición, 1: UVs, 2: Normales
        GLuint program_id;

        // Texturas
        GLuint texture_diffuse_id;  // Color (Hojas/Corteza)
        GLuint texture_specular_id; // Brillo (mapa _Spec)

        GLsizei vertex_count;

        // Uniform Locations
        GLint model_loc, view_loc, proj_loc, normal_matrix_loc;
        GLint view_pos_loc, light_pos_loc, light_color_loc;
        GLint sampler_diffuse_loc, sampler_specular_loc;

        void compile_shaders();
        GLuint load_texture(const std::string& path);
        bool load_obj(const std::string& path);

    public:
        // Constructor: Recibe la ruta del OBJ y las dos texturas
        Tree(const std::string& obj_path, const std::string& diff_path, const std::string& spec_path);
        ~Tree();

        virtual void render(const Camera& camera, Light* light = nullptr) override;
    };
}