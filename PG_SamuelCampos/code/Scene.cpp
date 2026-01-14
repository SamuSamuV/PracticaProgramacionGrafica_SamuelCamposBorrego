#include "Scene.hpp"
#include "Terrain.hpp"
#include "Water.hpp"
#include "Light.hpp"
#include "Tree.hpp"
#include <iostream>
#include <gtc/matrix_transform.hpp>
#include <SDL3/SDL_keycode.h>

namespace udit
{
    // Shader de Post-Procesado (Filtro Verdoso)
    const std::string effect_vs = R"(
        #version 330
        layout (location = 0) in vec3 vertex_coordinates;
        layout (location = 1) in vec2 vertex_texture_uv;
        out vec2 texture_uv;
        void main() {
           gl_Position = vec4(vertex_coordinates, 1.0);
           texture_uv  = vertex_texture_uv;
        }
    )";

    const std::string effect_fs = R"(
        #version 330
        uniform sampler2D sampler2d;
        in  vec2 texture_uv;
        out vec4 fragment_color;
        void main() {
            vec3 color = texture(sampler2d, texture_uv).rgb;
            // Efecto: convertimos a gris y luego tintamos de verde
            float i = (color.r + color.g + color.b) * 0.2;
            fragment_color = vec4(vec3(i, i, i) * vec3(0.3, 0.8, 0.5), 1.0);
        }
    )";

    Scene::Scene(int width, int height)
        : width(width), height(height), cube_angle(0.0f)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        build_framebuffer();
        effect_program_id = compile_shaders(effect_vs, effect_fs);

        camera = std::make_shared<Camera>();
        camera->set_location(0.f, 5.f, 15.f);

        root = std::make_shared<Node>();

        // Inicializamos punteros a nulo para evitar problemas si no se cargan
        main_light = nullptr;
        cube_opaque = nullptr;
        cube_transparent = nullptr;

        // En lugar de crear los objetos a mano aquí, leemos el archivo de configuración
        // que define qué objetos hay y dónde están colocados.
        load_scene("../../../shared/assets/scene.txt");

        // Si el archivo no existía o no tenía luz, creamos una por defecto para que se vea algo
        if (!main_light) {
            main_light = new Light();
            root->add_child(main_light);
        }

        skybox = std::make_shared<Skybox>("../../../shared/assets/sky-cube-map-");

        camera_speed = 0.1f;
        angle_around_x = angle_around_y = 0.0f;
        for (int i = 0; i < 6; i++) keys_pressed[i] = false;

        resize(width, height);
    }

    void Scene::load_scene(const std::string& path)
    {
        // Intentamos abrir el archivo de texto
        std::ifstream file(path);

        if (!file.is_open()) {
            std::cerr << "No se ha podido abrir el archivo de escena: " << path << std::endl;
            return;
        }

        std::string line;

        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;

            std::stringstream ss(line);
            std::string type;

            // La primera palabra de la línea nos dice qué tipo de objeto es
            ss >> type;

            if (type == "LIGHT") {
                // Leemos posición (x,y,z) y color (r,g,b)
                float x, y, z, r, g, b;
                ss >> x >> y >> z >> r >> g >> b;

                main_light = new Light();
                main_light->set_position({ x, y, z });
                main_light->set_color({ r, g, b });
                root->add_child(main_light);
            }

            else if (type == "TERRAIN") {
                // Leemos tamaño físico, resolución y posición
                float w, d, px, py, pz;
                unsigned sx, sz;
                ss >> w >> d >> sx >> sz >> px >> py >> pz;

                auto terrain = new Terrain(w, d, sx, sz);
                terrain->set_position({ px, py, pz });
                root->add_child(terrain);
            }

            else if (type == "WATER") {
                // Leemos dimensiones y posición
                float w, d, px, py, pz;
                ss >> w >> d >> px >> py >> pz;

                auto water = new Water(w, d);
                water->set_position({ px, py, pz });
                root->add_child(water);
            }

            else if (type == "CUBE") {
                // Leemos posición y opacidad (para transparencia)
                float x, y, z, opacity;
                ss >> x >> y >> z >> opacity;

                auto cube = new Cube();
                cube->set_position({ x, y, z });
                cube->set_opacity(opacity);
                root->add_child(cube);

                // Guardamos las referencias en las variables de la clase para poder animarlos luego
                if (opacity >= 1.0f && !cube_opaque) {
                    cube_opaque = cube;
                }
                else if (opacity < 1.0f && !cube_transparent) {
                    cube_transparent = cube;
                }
            }

            else if (type == "TREE") {
                // Formato: TREE scale x y z
                float s, x, y, z;
                ss >> s >> x >> y >> z;

                // NOTA: Usa la textura de HOJAS como principal porque tiene transparencia.
                // El modelo Tree.obj suele tener mapeado el tronco y hojas junto, 
                // así que usaremos la de las hojas para el efecto.
                // Si el tronco se ve raro, habría que separar el OBJ en dos partes.
                std::string objPath = "../../../shared/assets/Tree.obj";
                std::string texDiff = "../../../shared/assets/DB2X2_L01.png";       // Hojas Diffuse
                std::string texSpec = "../../../shared/assets/DB2X2_L01_Spec.png";  // Hojas Specular

                auto tree = new Tree(objPath, texDiff, texSpec);
                tree->set_position({ x, y, z });
                tree->set_scale({ s, s, s }); // Escala uniforme
                root->add_child(tree);
            }
        }

        std::cout << "Escena cargada correctamente desde " << path << std::endl;
        file.close();
    }

    Scene::~Scene() {
        // Limpiamos recursos de OpenGL
        glDeleteProgram(effect_program_id);
        glDeleteFramebuffers(1, &framebuffer_id);
        glDeleteTextures(1, &out_texture_id);
        glDeleteRenderbuffers(1, &depthbuffer_id);
        glDeleteVertexArrays(1, &framebuffer_quad_vao);
        glDeleteBuffers(2, framebuffer_quad_vbos);

        // NO borramos 'root' ni los cubos manualmente. 
        // shared_ptr<Node> root se encargará de borrar todo el árbol recursivamente.
    }

    void Scene::update()
    {
        // 1. Animación de Cubos
        cube_angle += 0.005f; // Velocidad de rotación
        float degrees = glm::degrees(cube_angle);

        if (cube_opaque) cube_opaque->set_rotation({ 0, degrees, 0 });
        if (cube_transparent) cube_transparent->set_rotation({ 0, degrees, 0 });

        // 2. Actualizar matrices del Grafo
        if (root) root->update();

        // 3. Control de Cámara (Órbita)
        angle_around_x += angle_delta_x;
        angle_around_y += angle_delta_y;
        if (angle_around_x < -1.5f) angle_around_x = -1.5f;
        if (angle_around_x > +1.5f) angle_around_x = +1.5f;

        glm::mat4 cam_rot(1);
        cam_rot = glm::rotate(cam_rot, angle_around_y, glm::vec3(0, 1, 0));
        cam_rot = glm::rotate(cam_rot, angle_around_x, glm::vec3(1, 0, 0));

        auto loc = camera->get_location();
        camera->set_target(loc.x, loc.y, loc.z - 1.0f);
        camera->rotate(cam_rot);

        // 4. Movimiento libre (WASD)
        glm::vec3 fwd = glm::normalize(glm::vec3(camera->get_target() - camera->get_location()));
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        glm::vec3 up(0, 1, 0);
        float speed = boost_camera_speed ? camera_speed * 5.0f : camera_speed;

        glm::vec3 move(0);
        if (keys_pressed[0]) move += fwd * speed;
        if (keys_pressed[2]) move -= fwd * speed;
        if (keys_pressed[1]) move -= right * speed;
        if (keys_pressed[3]) move += right * speed;
        if (keys_pressed[4]) move -= up * speed;
        if (keys_pressed[5]) move += up * speed;
        camera->move(move);
    }

    void Scene::render()
    {
        // 1. DIBUJAR EN EL FRAMEBUFFER (OFF-SCREEN)
        // Todo lo que dibujemos aquí no sale en pantalla, sino en 'out_texture_id'
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glViewport(0, 0, width, height);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Fondo
        if (skybox) skybox->render(*camera);

        // Escena 3D (Pasamos la cámara y la luz principal)
        if (root) root->render(*camera, main_light);

        // 2. DIBUJAR EN PANTALLA (POST-PROCESADO)
        // Usamos la textura generada en el paso anterior y la pintamos en un cuadrado plano
        render_framebuffer();
    }

    void Scene::render_framebuffer() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Volver al buffer de pantalla
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(effect_program_id);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, out_texture_id); // La imagen de la escena
        glUniform1i(glGetUniformLocation(effect_program_id, "sampler2d"), 0);

        glBindVertexArray(framebuffer_quad_vao);
        glDisable(GL_DEPTH_TEST); // Desactivamos profundidad porque es un dibujo 2D plano
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);
    }

    void Scene::build_framebuffer() {
        glGenFramebuffers(1, &framebuffer_id); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glGenTextures(1, &out_texture_id); glBindTexture(GL_TEXTURE_2D, out_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenRenderbuffers(1, &depthbuffer_id); glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer_id);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture_id, 0);
        GLenum dr[] = { GL_COLOR_ATTACHMENT0 }; glDrawBuffers(1, dr);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        static const GLfloat qp[] = { 1,-1,0, 1,1,0, -1,1,0, -1,1,0, -1,-1,0, 1,-1,0 };
        static const GLfloat qu[] = { 1,0, 1,1, 0,1, 0,1, 0,0, 1,0 };
        glGenVertexArrays(1, &framebuffer_quad_vao); glGenBuffers(2, framebuffer_quad_vbos);
        glBindVertexArray(framebuffer_quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, framebuffer_quad_vbos[0]); glBufferData(GL_ARRAY_BUFFER, sizeof(qp), qp, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, framebuffer_quad_vbos[1]); glBufferData(GL_ARRAY_BUFFER, sizeof(qu), qu, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    void Scene::resize(int w, int h) {
        width = w; height = h;
        if (h == 0)h = 1;
        camera->set_ratio((float)width / (float)height);
        glViewport(0, 0, width, height);
        glBindTexture(GL_TEXTURE_2D, out_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    }

    //Funciones para el movimiento de la camara
    void Scene::on_drag(float x, float y) { if (pointer_pressed) { angle_delta_x = 0.00003f * (last_pointer_y - y); angle_delta_y = 0.00003f * (last_pointer_x - x); } }
    void Scene::on_click(float x, float y, bool down) { pointer_pressed = down; if (down) { last_pointer_x = x; last_pointer_y = y; } else { angle_delta_x = angle_delta_y = 0; } }
    void Scene::on_key(int k, bool p) {
        if (k == 'w' || k == 'W') keys_pressed[0] = p; if (k == 'a' || k == 'A') keys_pressed[1] = p;
        if (k == 's' || k == 'S') keys_pressed[2] = p; if (k == 'd' || k == 'D') keys_pressed[3] = p;
        if (k == 'q' || k == 'Q') keys_pressed[4] = p; if (k == 'e' || k == 'E') keys_pressed[5] = p;
        if (k == SDLK_LSHIFT) boost_camera_speed = p;
    }

    GLuint Scene::compile_shaders(const std::string& vs, const std::string& fs) {
        GLuint p = glCreateProgram(); GLuint v = glCreateShader(GL_VERTEX_SHADER); GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        const char* vsp = vs.c_str(), * fsp = fs.c_str();
        glShaderSource(v, 1, &vsp, NULL); glCompileShader(v);
        glShaderSource(f, 1, &fsp, NULL); glCompileShader(f);
        glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
        glDeleteShader(v); glDeleteShader(f); return p;
    }
}