// Este código es de dominio público
// angel.rodriguez@udit.es
// Modificado por samuel.campos@alumnos.udit.es

#include "Scene.hpp"
#include <iostream>
#include <vector>
#include <cstring> 
#include <cassert>

#include <glm.hpp>                          
#include <gtc/matrix_transform.hpp>         
#include <gtc/type_ptr.hpp>                 

#include <SOIL2.h>
#include <SDL3/SDL_keycode.h>

namespace udit
{
    // Shaders del Post-Procesado
    const std::string Scene::effect_vertex_shader = R"(
        #version 330
        layout (location = 0) in vec3 vertex_coordinates;
        layout (location = 1) in vec2 vertex_texture_uv;
        out vec2 texture_uv;
        void main() {
           gl_Position = vec4(vertex_coordinates, 1.0);
           texture_uv  = vertex_texture_uv;
        }
    )";

    const std::string Scene::effect_fragment_shader = R"(
        #version 330
        uniform sampler2D sampler2d;
        in  vec2 texture_uv;
        out vec4 fragment_color;
        void main() {
            vec3 color = texture(sampler2d, texture_uv).rgb;
            float i = (color.r + color.g + color.b) * 0.2;
            fragment_color = vec4(vec3(i, i, i) * vec3(0.3, 0.8, 0.5), 1.0);
        }
    )";

    // Shaders del terreno
    const std::string Scene::terrain_vertex_shader = R"(
        #version 330
        struct Light { vec4 position; vec3 color; };
        uniform Light light;
        uniform float ambient_intensity;
        uniform float diffuse_intensity;
        uniform vec3  material_color;
        uniform float fog_near;
        uniform float fog_far;
        uniform mat4 model_view_matrix;
        uniform mat4 projection_matrix;
        uniform mat4 normal_matrix;      
        layout (location = 0) in vec2 vertex_xz;
        layout (location = 1) in vec2 vertex_uv;
        
        uniform sampler2D sampler; 
        uniform float     max_height;
        
        out vec3 lighting_color; 
        out float visibility;
        out vec2 v_uv;
        
        void main() {
           float h_center = texture(sampler, vertex_uv).r * max_height;
           float offset = 1.0 / 100.0; 
           float h_right = texture(sampler, vertex_uv + vec2(offset, 0.0)).r * max_height;
           float h_up    = texture(sampler, vertex_uv + vec2(0.0, offset)).r * max_height;
           vec3 v_center = vec3(vertex_xz.x, h_center, vertex_xz.y);
           vec3 v_right  = vec3(vertex_xz.x + 0.1, h_right, vertex_xz.y); 
           vec3 v_up     = vec3(vertex_xz.x, h_up, vertex_xz.y + 0.1);
           vec3 calculated_normal = normalize(cross(v_up - v_center, v_right - v_center));
           vec4 normal_mv = normal_matrix * vec4(calculated_normal, 0.0);
           vec4 position  = model_view_matrix * vec4(vertex_xz.x, h_center, vertex_xz.y, 1.0);
           vec4 light_direction = light.position - position;
           float light_value = max(dot(normalize(normal_mv.xyz), normalize(light_direction.xyz)), 0.0);
           lighting_color = ambient_intensity * material_color + diffuse_intensity * light_value * light.color * material_color;
           visibility = clamp((fog_far + position.z) / (fog_far - fog_near), 0.0, 1.0);
           v_uv = vertex_uv;
           gl_Position  = projection_matrix * position;
        }
    )";

    const std::string Scene::terrain_fragment_shader = R"(
        #version 330
        uniform sampler2D diffuse_sampler;
        uniform vec3 fog_color;
        in  vec3  lighting_color;
        in  float visibility;
        in  vec2  v_uv;
        out vec4  fragment_color;
        void main() {
            vec4 tex_color = texture(diffuse_sampler, v_uv);
            vec3 combined_color = lighting_color * tex_color.rgb;
            fragment_color = mix(vec4(fog_color, 1.0), vec4(combined_color, 1.0), visibility);
        }
    )";

    // Shaders del cubo
    const std::string Scene::cube_vertex_shader = R"(
        #version 330
        uniform mat4 model_view_matrix;
        uniform mat4 projection_matrix;
        uniform float fog_near;
        uniform float fog_far;
        layout (location = 0) in vec3 vertex_coordinates;
        layout (location = 1) in vec2 vertex_texture_uv;
        out vec2 texture_uv;
        out float visibility;
        void main() {
           vec4 position = model_view_matrix * vec4(vertex_coordinates, 1.0);
           gl_Position = projection_matrix * position;
           texture_uv  = vertex_texture_uv;
           visibility = clamp((fog_far + position.z) / (fog_far - fog_near), 0.0, 1.0);
        }
    )";

    const std::string Scene::cube_fragment_shader = R"(
        #version 330
        uniform sampler2D samplers[2]; 
        uniform float alpha_value;
        uniform vec3 fog_color;
        in  vec2 texture_uv;
        in  float visibility;
        out vec4 fragment_color;
        void main() {
            vec4 tex1 = texture(samplers[0], texture_uv);
            vec4 tex2 = texture(samplers[1], texture_uv);
            vec4 mixed_texture = mix(tex1, tex2, 0.5);
            vec4 object_color = vec4(mixed_texture.rgb, alpha_value);
            vec3 final_rgb = mix(fog_color, object_color.rgb, visibility);
            fragment_color = vec4(final_rgb, object_color.a);
        }
    )";

    GLuint safe_create_texture(const std::string& path)
    {
        int width = 0, height = 0, channels = 0;
        unsigned char* loaded_pixels = SOIL_load_image(path.c_str(), &width, &height, &channels, SOIL_LOAD_RGBA);
        if (!loaded_pixels) { std::cerr << "Error textura: " << path << std::endl; return 0; }
        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, loaded_pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        SOIL_free_image_data(loaded_pixels);
        return texture_id;
    }


    Scene::Scene(int width, int height)
        : width(width), height(height)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glClearColor(0.5f, 0.5f, 0.5f, 1.f);

        build_framebuffer();

        camera = std::make_shared<Camera>();
        camera->set_location(0.f, 5.f, 15.f);

        load_scene("../../../shared/assets/Scene.txt");

        resize(width, height);

        fog_near = 5.0f;
        fog_far = 50.0f;
        fog_color = glm::vec3(0.5f, 0.5f, 0.5f);

        time = 0.0f;

        effect_program_id = compile_shaders(effect_vertex_shader, effect_fragment_shader);

        // Terreno
        terrain_program_id = compile_shaders(terrain_vertex_shader, terrain_fragment_shader);

        terrain_mv_id = glGetUniformLocation(terrain_program_id, "model_view_matrix");
        terrain_proj_id = glGetUniformLocation(terrain_program_id, "projection_matrix");
        terrain_max_height_id = glGetUniformLocation(terrain_program_id, "max_height");
        terrain_sampler_id = glGetUniformLocation(terrain_program_id, "sampler");
        terrain_diffuse_sampler_id = glGetUniformLocation(terrain_program_id, "diffuse_sampler");
        terrain_normal_matrix_id = glGetUniformLocation(terrain_program_id, "normal_matrix");

        configure_light(terrain_program_id);
        configure_fog(terrain_program_id);

        texture_id = safe_create_texture("../../../shared/assets/height-map.png");
        there_is_texture = (texture_id > 0);

        terrain_diffuse_texture_id = safe_create_texture("../../../shared/assets/MountainTexture.png");
        there_is_terrain_diffuse = (terrain_diffuse_texture_id > 0);

        glUseProgram(terrain_program_id);
        if (terrain_max_height_id != -1) glUniform1f(terrain_max_height_id, 5.0f);
        if (terrain_sampler_id != -1)         glUniform1i(terrain_sampler_id, 0);
        if (terrain_diffuse_sampler_id != -1) glUniform1i(terrain_diffuse_sampler_id, 1);
        glUseProgram(0);

        // Cubos
        cube_program_id = compile_shaders(cube_vertex_shader, cube_fragment_shader);

        cube_mv_id = glGetUniformLocation(cube_program_id, "model_view_matrix");
        cube_proj_id = glGetUniformLocation(cube_program_id, "projection_matrix");

        configure_fog(cube_program_id);

        cube_texture_1_id = safe_create_texture("../../../shared/assets/uv-checker.png");
        there_is_cube_texture_1 = (cube_texture_1_id > 0);

        cube_texture_2_id = safe_create_texture("../../../shared/assets/wood.png");
        there_is_cube_texture_2 = (cube_texture_2_id > 0);

        glUseProgram(cube_program_id);
        glUniform1i(glGetUniformLocation(cube_program_id, "samplers[0]"), 0);
        glUniform1i(glGetUniformLocation(cube_program_id, "samplers[1]"), 1);
        glUseProgram(0);

        cube_angle = 0.0f;

        angle_around_x = angle_delta_x = 0.0;
        angle_around_y = angle_delta_y = 0.0;
        pointer_pressed = false;
        camera_speed = 0.1f;
        boost_camera_speed = false;
        for (int i = 0; i < 6; ++i) keys_pressed[i] = false;
    }

    Scene::~Scene()
    {
        glDeleteProgram(terrain_program_id);
        glDeleteProgram(cube_program_id);
        glDeleteProgram(effect_program_id);

        glDeleteFramebuffers(1, &framebuffer_id);
        glDeleteTextures(1, &out_texture_id);
        glDeleteRenderbuffers(1, &depthbuffer_id);
        glDeleteVertexArrays(1, &framebuffer_quad_vao);
        glDeleteBuffers(2, framebuffer_quad_vbos);

        if (there_is_texture) glDeleteTextures(1, &texture_id);
        if (there_is_terrain_diffuse) glDeleteTextures(1, &terrain_diffuse_texture_id);
        if (there_is_cube_texture_1) glDeleteTextures(1, &cube_texture_1_id);
        if (there_is_cube_texture_2) glDeleteTextures(1, &cube_texture_2_id);
    }

    void Scene::load_scene(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "AVISO: No se pudo abrir " << path << ". Cargando escena por defecto." << std::endl;
            terrain = std::make_shared<Terrain>(20.f, 20.f, 100, 100);
            water = std::make_shared<Water>(50.f, 50.f);
            skybox = std::make_shared<Skybox>("../../../shared/assets/sky-cube-map-");
            cube = std::make_shared<Cube>();
            return;
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;

            std::stringstream iss(line);
            std::string type;
            iss >> type;

            if (type == "SKYBOX")
            {
                std::string texture_path;
                iss >> texture_path;
                skybox = std::make_shared<Skybox>(texture_path);
            }
            else if (type == "TERRAIN")
            {
                float w, d;
                int xs, zs;
                iss >> w >> d >> xs >> zs;
                terrain = std::make_shared<Terrain>(w, d, xs, zs);
            }
            else if (type == "WATER")
            {
                float w, d;
                iss >> w >> d;
                water = std::make_shared<Water>(w, d);
            }
            else if (type == "CUBE_MALLA")
            {
                cube = std::make_shared<Cube>();
            }
        }
        std::cout << "Escena cargada desde " << path << std::endl;
    }

    void Scene::update()
    {
        cube_angle += 0.01f;
        time += 0.01f;

        angle_around_x += angle_delta_x;
        angle_around_y += angle_delta_y;
        if (angle_around_x < -1.5f) angle_around_x = -1.5f;
        if (angle_around_x > +1.5f) angle_around_x = +1.5f;

        glm::mat4 camera_rotation(1);
        camera_rotation = glm::rotate(camera_rotation, angle_around_y, glm::vec3(0.f, 1.f, 0.f));
        camera_rotation = glm::rotate(camera_rotation, angle_around_x, glm::vec3(1.f, 0.f, 0.f));

        auto current_loc = camera->get_location();
        camera->set_target(current_loc[0], current_loc[1], current_loc[2] - 1.0f);
        camera->rotate(camera_rotation);

        glm::vec4 forward_v4 = camera->get_target() - camera->get_location();
        glm::vec3 forward = glm::normalize(glm::vec3(forward_v4));
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
        glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
        float current_speed = boost_camera_speed ? (camera_speed * 5.0f) : camera_speed;

        glm::vec3 translation(0.f);
        if (keys_pressed[0]) translation += forward * current_speed;
        if (keys_pressed[2]) translation -= forward * current_speed;
        if (keys_pressed[1]) translation -= right * current_speed;
        if (keys_pressed[3]) translation += right * current_speed;
        if (keys_pressed[4]) translation -= up * current_speed;
        if (keys_pressed[5]) translation += up * current_speed;
        camera->move(translation);
    }

    void Scene::build_framebuffer()
    {
        glGenFramebuffers(1, &framebuffer_id);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);

        glGenTextures(1, &out_texture_id);
        glBindTexture(GL_TEXTURE_2D, out_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glGenRenderbuffers(1, &depthbuffer_id);
        glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer_id);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture_id, 0);
        const GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &draw_buffer);

        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        static const GLfloat quad_positions[] = {
            +1.0f, -1.0f, 0.0f,  +1.0f, +1.0f, 0.0f,  -1.0f, +1.0f, 0.0f,
            -1.0f, +1.0f, 0.0f,  -1.0f, -1.0f, 0.0f,  +1.0f, -1.0f, 0.0f,
        };
        static const GLfloat quad_texture_uvs[] = {
            +1.0f, 0.0f,  +1.0f, +1.0f,  0.0f, +1.0f,
             0.0f, +1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        };

        glGenVertexArrays(1, &framebuffer_quad_vao);
        glGenBuffers(2, framebuffer_quad_vbos);

        glBindVertexArray(framebuffer_quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, framebuffer_quad_vbos[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_positions), quad_positions, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, framebuffer_quad_vbos[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_texture_uvs), quad_texture_uvs, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    void Scene::render()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera->get_transform_matrix_inverse();
        glm::mat4 projection = camera->get_projection_matrix();

        if (skybox) skybox->render(*camera);

        // Render Terreno
        if (terrain)
        {
            glUseProgram(terrain_program_id);
            glm::mat4 terrain_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, -2.f, 0.f));
            glm::mat4 terrain_view_model = view * terrain_model;
            glUniformMatrix4fv(terrain_mv_id, 1, GL_FALSE, glm::value_ptr(terrain_view_model));
            glUniformMatrix4fv(terrain_proj_id, 1, GL_FALSE, glm::value_ptr(projection));
            glm::mat4 normal_matrix = glm::transpose(glm::inverse(terrain_view_model));
            glUniformMatrix4fv(terrain_normal_matrix_id, 1, GL_FALSE, glm::value_ptr(normal_matrix));

            if (there_is_texture) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture_id); }
            if (there_is_terrain_diffuse) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, terrain_diffuse_texture_id); }

            terrain->render();
        }

        // Agua
        if (water)
        {
            water->render(view, projection, time);
        }

        // Render Cubos
        if (cube)
        {
            glUseProgram(cube_program_id);
            glUniformMatrix4fv(cube_proj_id, 1, GL_FALSE, glm::value_ptr(projection));
            if (there_is_cube_texture_1) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, cube_texture_1_id); }
            if (there_is_cube_texture_2) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, cube_texture_2_id); }

            GLint alpha_loc = glGetUniformLocation(cube_program_id, "alpha_value");

            // Cubo Opaco
            glUniform1f(alpha_loc, 1.0f);
            glm::mat4 model1 = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, 2.f, -5.f));
            model1 = glm::rotate(model1, cube_angle, glm::vec3(0.f, 1.f, 0.f));
            glUniformMatrix4fv(cube_mv_id, 1, GL_FALSE, glm::value_ptr(view * model1));
            cube->render();

            // Cubo Transparente
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUniform1f(alpha_loc, 0.5f);
            glm::mat4 model2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, 6.f, -5.f));
            model2 = glm::rotate(model2, cube_angle, glm::vec3(0.f, 1.f, 0.f));
            glUniformMatrix4fv(cube_mv_id, 1, GL_FALSE, glm::value_ptr(view * model2));
            cube->render();
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glUseProgram(0);
        }

        render_framebuffer();
    }

    void Scene::render_framebuffer()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(effect_program_id);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, out_texture_id);

        glBindVertexArray(framebuffer_quad_vao);
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(0);
    }

    void Scene::configure_fog(GLuint program_id)
    {
        glUseProgram(program_id);
        glUniform1f(glGetUniformLocation(program_id, "fog_near"), fog_near);
        glUniform1f(glGetUniformLocation(program_id, "fog_far"), fog_far);
        glUniform3fv(glGetUniformLocation(program_id, "fog_color"), 1, glm::value_ptr(fog_color));
        glUseProgram(0);
    }

    void Scene::configure_light(GLuint program_id)
    {
        glUseProgram(program_id);
        glUniform4f(glGetUniformLocation(program_id, "light.position"), 10.0f, 20.0f, 10.0f, 1.0f);
        glUniform3f(glGetUniformLocation(program_id, "light.color"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(program_id, "ambient_intensity"), 0.3f);
        glUniform1f(glGetUniformLocation(program_id, "diffuse_intensity"), 0.9f);
        glUniform3f(glGetUniformLocation(program_id, "material_color"), 1.0f, 1.0f, 1.0f);
        glUseProgram(0);
    }

    void Scene::resize(int new_width, int new_height)
    {
        width = new_width;
        height = new_height;
        if (height == 0) height = 1;
        camera->set_ratio(float(width) / height);
        glViewport(0, 0, width, height);
    }

    void Scene::on_drag(float pointer_x, float pointer_y)
    {
        if (pointer_pressed)
        {
            angle_delta_x = 0.00003f * (last_pointer_y - pointer_y);
            angle_delta_y = 0.00003f * (last_pointer_x - pointer_x);
        }
    }

    void Scene::on_click(float pointer_x, float pointer_y, bool down)
    {
        if ((pointer_pressed = down) == true)
        {
            last_pointer_x = pointer_x;
            last_pointer_y = pointer_y;
        }
        else
        {
            angle_delta_x = angle_delta_y = 0.0;
        }
    }

    void Scene::on_key(int key, bool pressed)
    {
        if (key == 'W' || key == 'w') keys_pressed[0] = pressed;
        if (key == 'A' || key == 'a') keys_pressed[1] = pressed;
        if (key == 'S' || key == 's') keys_pressed[2] = pressed;
        if (key == 'D' || key == 'd') keys_pressed[3] = pressed;
        if (key == 'Q' || key == 'q') keys_pressed[4] = pressed;
        if (key == 'E' || key == 'e') keys_pressed[5] = pressed;
        if (key == SDLK_LSHIFT || key == SDLK_RSHIFT) boost_camera_speed = pressed;
    }

    GLuint Scene::compile_shaders(const std::string& vs_source, const std::string& fs_source)
    {
        GLint succeeded = GL_FALSE;
        GLuint vs_id = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs_id = glCreateShader(GL_FRAGMENT_SHADER);

        const char* vs_ptr = vs_source.c_str();
        const char* fs_ptr = fs_source.c_str();

        glShaderSource(vs_id, 1, &vs_ptr, NULL);
        glShaderSource(fs_id, 1, &fs_ptr, NULL);

        glCompileShader(vs_id);
        glGetShaderiv(vs_id, GL_COMPILE_STATUS, &succeeded);
        if (!succeeded) show_compilation_error(vs_id);

        glCompileShader(fs_id);
        glGetShaderiv(fs_id, GL_COMPILE_STATUS, &succeeded);
        if (!succeeded) show_compilation_error(fs_id);

        GLuint pid = glCreateProgram();
        glAttachShader(pid, vs_id);
        glAttachShader(pid, fs_id);
        glLinkProgram(pid);

        glGetProgramiv(pid, GL_LINK_STATUS, &succeeded);
        if (!succeeded) show_linkage_error(pid);

        glDeleteShader(vs_id);
        glDeleteShader(fs_id);
        return pid;
    }

    void Scene::show_compilation_error(GLuint shader_id)
    {
        std::string info_log;
        GLint info_log_length;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
        info_log.resize(info_log_length);
        glGetShaderInfoLog(shader_id, info_log_length, NULL, &info_log.front());
        std::cerr << "Shader Error: " << info_log.c_str() << std::endl;
    }

    void Scene::show_linkage_error(GLuint program_id)
    {
        std::string info_log;
        GLint info_log_length;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
        info_log.resize(info_log_length);
        glGetProgramInfoLog(program_id, info_log_length, NULL, &info_log.front());
        std::cerr << "Link Error: " << info_log.c_str() << std::endl;
    }
}