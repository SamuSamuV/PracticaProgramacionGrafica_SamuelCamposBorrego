#include "Tree.hpp"
#include "Light.hpp" 
#include <iostream>
#include <fstream>
#include <sstream>
#include <gtc/type_ptr.hpp>
#include <SOIL2.h>

namespace udit
{
    static const std::string tree_vs = R"(
        #version 330
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTex;
        layout (location = 2) in vec3 aNormal;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 normal_matrix;

        out vec2 TexCoords;
        out vec3 Normal;
        out vec3 FragPos;

        void main() {
            // Calculamos posición en el mundo y normales
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(normal_matrix) * aNormal;
            
            // Pasamos las UVs
            TexCoords = aTex; 
            TexCoords.y = 1.0 - TexCoords.y; 
            
            gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";

    static const std::string tree_fs = R"(
        #version 330
        out vec4 FragColor;

        in vec2 TexCoords;
        in vec3 Normal;
        in vec3 FragPos;

        uniform sampler2D diffuseMap;
        uniform sampler2D specularMap;

        uniform vec3 lightPos;
        uniform vec3 lightColor;
        uniform vec3 viewPos;

        void main() {
            vec4 texColor = texture(diffuseMap, TexCoords);
            
            // Si el pixel es transparente (hojas), lo descartamos y no pintamos nada.
            // Así evitamos el típico cuadrado negro alrededor de la hoja.
            if(texColor.a < 0.1)
                discard;

            // Luz ambiental básica para que no se vea todo negro en las sombras
            float ambientStrength = 0.3;
            vec3 ambient = ambientStrength * lightColor * texColor.rgb;

            // Luz difusa (la chicha de la iluminación)
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor * texColor.rgb;

            // Especular (Brillos)
            // Usamos el mapa especular (.r) para que el tronco no brille igual que las hojas si no queremos
            float specularStrength = 0.5;
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
            
            float specMapValue = texture(specularMap, TexCoords).r;
            vec3 specular = specularStrength * spec * lightColor * specMapValue;

            // Juntamos todo y a correr
            FragColor = vec4(ambient + diffuse + specular, 1.0);
        }
    )";

    Tree::Tree(const std::string& obj_path, const std::string& diff_path, const std::string& spec_path)
    {
        // Primero intentamos cargar el modelo. Si falla, avisamos y salimos.
        if (!load_obj(obj_path)) {
            std::cerr << "AVISO: No he podido cargar el modelo " << obj_path << std::endl;
            return;
        }

        compile_shaders();

        // Cargamos las texturas
        texture_diffuse_id = load_texture(diff_path);

        // Si nos pasan un mapa especular guay, si no, reutilizamos el difuso para que no pete
        if (!spec_path.empty())
            texture_specular_id = load_texture(spec_path);
        else
            texture_specular_id = texture_diffuse_id;
    }

    Tree::~Tree()
    {
        // Limpiamos la memoria de la GPU al destruir el objeto
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(3, vbo_ids);
        glDeleteProgram(program_id);
        glDeleteTextures(1, &texture_diffuse_id);
        glDeleteTextures(1, &texture_specular_id);
    }

    void Tree::render(const Camera& camera, Light* light)
    {
        glUseProgram(program_id);

        // Desactivamos el Culling porque las hojas son planas (2D)
        // Si no hacemos esto, al girar la cámara desaparecerían por detrás.
        glDisable(GL_CULL_FACE);

        // Preparamos las matrices
        glm::mat4 model = get_global_matrix();
        glm::mat4 view = camera.get_transform_matrix_inverse();
        glm::mat4 proj = camera.get_projection_matrix();
        // La matriz normal es clave para que la luz rebote bien si escalamos el objeto
        glm::mat4 normal_mat = glm::transpose(glm::inverse(model));

        // Enviamos uniformes al shader
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(normal_matrix_loc, 1, GL_FALSE, glm::value_ptr(normal_mat));

        // Configuramos la luz
        if (light) {
            glm::vec3 lpos = light->get_position();
            glUniform3f(light_pos_loc, lpos.x, lpos.y, lpos.z);
            glm::vec3 lcol = light->get_color();
            glUniform3f(light_color_loc, lcol.x, lcol.y, lcol.z);
        }
        else {
            // Luz por si no hay ninguna en la escena
            glUniform3f(light_pos_loc, 10, 50, 10);
            glUniform3f(light_color_loc, 1, 1, 1);
        }

        glm::vec4 cpos = camera.get_location();
        glUniform3f(view_pos_loc, cpos.x, cpos.y, cpos.z);

        // Bindeamos texturas
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_diffuse_id);
        glUniform1i(sampler_diffuse_loc, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture_specular_id);
        glUniform1i(sampler_specular_loc, 1);

        // Pintamos
        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
        glBindVertexArray(0);

        // Volvemos a activar el Culling para el resto de objetos solidos
        glEnable(GL_CULL_FACE);

        // Importante llamar al padre por si tiene hijos (aunque el árbol no suela tener)
        Node::render(camera, light);
    }

    // Parser manual de OBJ, algo rudimentario pero funcional para lo que queremos
    bool Tree::load_obj(const std::string& path)
    {
        std::vector<glm::vec3> temp_vertices;
        std::vector<glm::vec2> temp_uvs;
        std::vector<glm::vec3> temp_normals;

        std::vector<float> final_vertices;
        std::vector<float> final_uvs;
        std::vector<float> final_normals;

        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line))
        {
            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v") {
                glm::vec3 v; ss >> v.x >> v.y >> v.z;
                temp_vertices.push_back(v);
            }
            else if (type == "vt") {
                glm::vec2 uv; ss >> uv.x >> uv.y;
                temp_uvs.push_back(uv);
            }
            else if (type == "vn") {
                glm::vec3 n; ss >> n.x >> n.y >> n.z;
                temp_normals.push_back(n);
            }
            else if (type == "f") {
                // Procesamos las caras
                std::string vertex_str;
                for (int i = 0; i < 3; i++) {
                    ss >> vertex_str;
                    std::stringstream vss(vertex_str);
                    std::string segment;
                    std::vector<std::string> indices;
                    while (std::getline(vss, segment, '/')) indices.push_back(segment);

                    // Ajustamos índices
                    if (indices.size() > 0 && !indices[0].empty()) {
                        glm::vec3 v = temp_vertices[stoi(indices[0]) - 1];
                        final_vertices.push_back(v.x); final_vertices.push_back(v.y); final_vertices.push_back(v.z);
                    }
                    if (indices.size() > 1 && !indices[1].empty()) {
                        glm::vec2 uv = temp_uvs[stoi(indices[1]) - 1];
                        final_uvs.push_back(uv.x); final_uvs.push_back(uv.y);
                    }
                    if (indices.size() > 2 && !indices[2].empty()) {
                        glm::vec3 n = temp_normals[stoi(indices[2]) - 1];
                        final_normals.push_back(n.x); final_normals.push_back(n.y); final_normals.push_back(n.z);
                    }
                }
            }
        }
        file.close();

        vertex_count = final_vertices.size() / 3;

        // Le paso todo a la GPU
        glGenVertexArrays(1, &vao_id);
        glGenBuffers(3, vbo_ids);

        glBindVertexArray(vao_id);

        // 1. Posiciones
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[0]);
        glBufferData(GL_ARRAY_BUFFER, final_vertices.size() * sizeof(float), final_vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        // 2. Texturas
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[1]);
        glBufferData(GL_ARRAY_BUFFER, final_uvs.size() * sizeof(float), final_uvs.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);

        // 3. Normales
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[2]);
        glBufferData(GL_ARRAY_BUFFER, final_normals.size() * sizeof(float), final_normals.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        return true;
    }

    void Tree::compile_shaders()
    {
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        const char* vs = tree_vs.c_str(); const char* fs = tree_fs.c_str();
        glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        program_id = glCreateProgram();
        glAttachShader(program_id, v); glAttachShader(program_id, f); glLinkProgram(program_id);
        glDeleteShader(v); glDeleteShader(f);

        model_loc = glGetUniformLocation(program_id, "model");
        view_loc = glGetUniformLocation(program_id, "view");
        proj_loc = glGetUniformLocation(program_id, "projection");
        normal_matrix_loc = glGetUniformLocation(program_id, "normal_matrix");
        view_pos_loc = glGetUniformLocation(program_id, "viewPos");
        light_pos_loc = glGetUniformLocation(program_id, "lightPos");
        light_color_loc = glGetUniformLocation(program_id, "lightColor");
        sampler_diffuse_loc = glGetUniformLocation(program_id, "diffuseMap");
        sampler_specular_loc = glGetUniformLocation(program_id, "specularMap");
    }

    GLuint Tree::load_texture(const std::string& path) {
        int w, h, c;
        unsigned char* data = SOIL_load_image(path.c_str(), &w, &h, &c, SOIL_LOAD_RGBA);
        if (!data) return 0;
        GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);

        // Repetimos textura (GL_REPEAT) por si el modelo tiene UVs mayores de 1
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); SOIL_free_image_data(data);
        return id;
    }
}