// Este código es de dominio público
// samuel.campos@alumnos.udit.es

#ifndef SCENE_HEADER
#define SCENE_HEADER

#include "Camera.hpp"
#include "Skybox.hpp"
#include "Terrain.hpp"
#include "Cube.hpp"
#include "Water.hpp"

#include <Color_Buffer.hpp>
#include <memory>   // NECESARIO PARA shared_ptr
#include <string>
#include <vector>
#include <fstream>  // NECESARIO PARA LEER ARCHIVOS
#include <sstream>  // NECESARIO PARA PARSEAR
#include <glad/gl.h>

namespace udit
{
    class Scene
    {
    private:
        // SHADERS
        static const std::string terrain_vertex_shader;
        static const std::string terrain_fragment_shader;
        static const std::string cube_vertex_shader;
        static const std::string cube_fragment_shader;
        static const std::string effect_vertex_shader;
        static const std::string effect_fragment_shader;
        static const std::string water_vertex_shader;
        static const std::string water_fragment_shader;

        GLuint compile_shaders(const std::string& vs_source, const std::string& fs_source);
        void   show_compilation_error(GLuint shader_id);
        void   show_linkage_error(GLuint program_id);
        void   configure_light(GLuint program_id);
        void   configure_fog(GLuint program_id);

        void   build_framebuffer();
        void   render_framebuffer();

        // FUNCIÓN NUEVA PARA CARGAR ESCENA
        void   load_scene(const std::string& path);

        // AHORA SON PUNTEROS INTELIGENTES
        std::shared_ptr<Camera>  camera;
        std::shared_ptr<Skybox>  skybox;
        std::shared_ptr<Terrain> terrain;
        std::shared_ptr<Cube>    cube;
        std::shared_ptr<Water>   water;

        // Variables Framebuffer
        GLuint framebuffer_id;
        GLuint depthbuffer_id;
        GLuint out_texture_id;
        GLuint effect_program_id;
        GLuint framebuffer_quad_vao;
        GLuint framebuffer_quad_vbos[2];

        // Variables Agua
        GLuint water_program_id;
        GLuint water_texture_id;
        bool   there_is_water_texture;
        GLint  water_mv_id;
        GLint  water_proj_id;
        GLint  water_time_id;
        GLint  water_sampler_id;
        float  time;

        // Variables Terreno
        GLuint terrain_program_id;
        GLuint texture_id;
        bool   there_is_texture;
        GLuint terrain_diffuse_texture_id;
        bool   there_is_terrain_diffuse;

        GLint  terrain_mv_id;
        GLint  terrain_proj_id;
        GLint  terrain_max_height_id;
        GLint  terrain_sampler_id;
        GLint  terrain_diffuse_sampler_id;
        GLint  terrain_normal_matrix_id;

        // Variables Cubo
        GLuint cube_program_id;
        GLuint cube_texture_1_id;
        bool   there_is_cube_texture_1;
        GLuint cube_texture_2_id;
        bool   there_is_cube_texture_2;
        GLint  cube_mv_id;
        GLint  cube_proj_id;

        // Niebla
        float     fog_near;
        float     fog_far;
        glm::vec3 fog_color;

        // Control de camara
        int    width;
        int    height;
        float  cube_angle;
        float  angle_around_x;
        float  angle_around_y;
        float  angle_delta_x;
        float  angle_delta_y;
        bool   pointer_pressed;
        float  last_pointer_x;
        float  last_pointer_y;
        float  camera_speed;
        bool   keys_pressed[6];
        bool   boost_camera_speed = false;

    public:
        Scene(int width, int height);
        ~Scene();

        void update();
        void render();
        void resize(int width, int height);

        void on_drag(float pointer_x, float pointer_y);
        void on_click(float pointer_x, float pointer_y, bool down);
        void on_key(int key, bool pressed);
    };
}
#endif