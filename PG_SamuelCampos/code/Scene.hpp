#pragma once
#include "Camera.hpp"
#include "Node.hpp"
#include "Skybox.hpp"
#include "Cube.hpp" 
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/gl.h>

namespace udit
{
    class Scene
    {
    private:
        int width, height;

        // Elementos principales compartidos (smart pointers)
        std::shared_ptr<Camera> camera;
        std::shared_ptr<Skybox> skybox;

        // El NODO RAÍZ del grafo. Todos los objetos cuelgan de él.
        std::shared_ptr<Node> root;

        // Punteros a objetos específicos que queremos controlar directamente
        // NOTA: Usamos punteros normales (raw) porque la memoria la gestiona el grafo (root).
        Cube* cube_opaque;
        Cube* cube_transparent;

        // Forward declaration de Light (definida en Light.hpp)
        class Light* main_light;

        // Variables para el Framebuffer (Post-procesado)
        GLuint framebuffer_id, out_texture_id, depthbuffer_id;
        GLuint effect_program_id, framebuffer_quad_vao, framebuffer_quad_vbos[2];

        void build_framebuffer();
        void render_framebuffer();
        GLuint compile_shaders(const std::string& vs, const std::string& fs);

        // Variables de control de cámara e input
        float camera_speed;
        bool keys_pressed[6];
        bool pointer_pressed;
        float last_pointer_x, last_pointer_y;
        float angle_around_x, angle_around_y;
        float angle_delta_x, angle_delta_y;
        bool boost_camera_speed;

        float cube_angle; // Para la animación de rotación

    public:
        void load_scene(const std::string& path);

    public:
        Scene(int width, int height);
        ~Scene();

        void update();
        void render();
        void resize(int width, int height);

        // Callbacks de eventos
        void on_drag(float x, float y);
        void on_click(float x, float y, bool down);
        void on_key(int key, bool pressed);
    };
}