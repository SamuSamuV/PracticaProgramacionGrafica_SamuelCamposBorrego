#include "Terrain.hpp"
#include <gtc/type_ptr.hpp>
#include <SOIL2.h>
#include <iostream>
#include <half.hpp>

using half_float::half; // Usamos floats de media precisión para ahorrar memoria en vértices

namespace udit
{
    // Leemos el mapa de alturas y subimos el vértice Y.
    static const std::string terrain_v_shader = R"(
        #version 330
        layout (location = 0) in vec2 vertex_xz; // Posición plana (x, z)
        layout (location = 1) in vec2 vertex_uv;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 normal_matrix;
        uniform float max_height;
        uniform sampler2D sampler; // La textura height-map

        uniform float fog_near;
        uniform float fog_far;

        out vec3 Normal;
        out vec3 FragPos;
        out vec2 v_uv;
        out float visibility;

        void main() {
           // 1. Leemos la altura (color rojo 'r') de la textura
           float h_center = texture(sampler, vertex_uv).r * max_height;
           
           // 2. Para calcular la normal (iluminación), necesitamos saber la pendiente.
           // Leemos la altura de los vecinos (derecha y arriba) un poquito más allá (offset).
           float offset = 1.0 / 100.0; 
           float h_right = texture(sampler, vertex_uv + vec2(offset, 0.0)).r * max_height;
           float h_up    = texture(sampler, vertex_uv + vec2(0.0, offset)).r * max_height;
           
           vec3 v_center = vec3(vertex_xz.x, h_center, vertex_xz.y);
           vec3 v_right  = vec3(vertex_xz.x + 0.1, h_right, vertex_xz.y); 
           vec3 v_up     = vec3(vertex_xz.x, h_up, vertex_xz.y + 0.1);
           
           // Producto cruz para obtener la normal perpendicular a la superficie
           vec3 calculated_normal = normalize(cross(v_up - v_center, v_right - v_center));
           
           // Enviamos datos al Fragment Shader
           Normal = mat3(normal_matrix) * calculated_normal;
           FragPos = vec3(model * vec4(vertex_xz.x, h_center, vertex_xz.y, 1.0));
           
           // Cálculo de niebla lineal basada en la distancia Z a la cámara
           vec4 position_view = view * vec4(FragPos, 1.0);
           float distance = length(position_view.xyz); // O usar solo Z
           // En este shader original se usaba .z, pero length es más preciso. 
           // Mantengo la lógica original de clamp sobre Z:
           visibility = clamp((fog_far + position_view.z) / (fog_far - fog_near), 0.0, 1.0);
           
           v_uv = vertex_uv;
           gl_Position = projection * position_view;
        }
    )";

    static const std::string terrain_f_shader = R"(
        #version 330
        in vec3 Normal;
        in vec3 FragPos;
        in vec2 v_uv;
        in float visibility; // Factor de niebla (0 = niebla total, 1 = se ve bien)

        out vec4 fragment_color;

        uniform sampler2D diffuse_sampler; // Textura de la montaña
        uniform vec3 fog_color;
        
        uniform vec3 lightPos;
        uniform vec3 lightColor;
        uniform vec3 viewPos;
        
        void main() {
            vec3 color = texture(diffuse_sampler, v_uv).rgb;

            // Iluminación (Similar al cubo pero ajustada para terreno)
            float ambientStrength = 0.2;
            vec3 ambient = ambientStrength * lightColor;
  
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor;
            
            // Brillo especular muy suave (terreno mate)
            float specularStrength = 0.1; 
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 16.0);
            vec3 specular = specularStrength * spec * lightColor;  
                
            vec3 lighting = (ambient + diffuse + specular) * color;
            
            // Mezclamos el color final con el color de la niebla según la visibilidad
            fragment_color = mix(vec4(fog_color, 1.0), vec4(lighting, 1.0), visibility);
        }
    )";

    Terrain::Terrain(float width, float depth, unsigned x_slices, unsigned z_slices)
    {
        unsigned n_vertices_x = x_slices + 1;
        unsigned n_vertices_z = z_slices + 1;
        number_of_indices = x_slices * z_slices * 6;
        int number_of_vertices = n_vertices_x * n_vertices_z;
        std::vector<half> coordinates(number_of_vertices * 2);
        std::vector<half> texture_uvs(number_of_vertices * 2);
        std::vector<unsigned int> indices(number_of_indices);
        float x_step = width / float(x_slices);
        float z_step = depth / float(z_slices);
        float u_step = 1.f / float(x_slices);
        float v_step = 1.f / float(z_slices);
        int vertex_index = 0;
        for (unsigned z = 0; z < n_vertices_z; ++z) {
            for (unsigned x = 0; x < n_vertices_x; ++x) {
                float pos_x = -width * 0.5f + x * x_step;
                float pos_z = -depth * 0.5f + z * z_step;
                coordinates[vertex_index * 2 + 0] = half(pos_x);
                coordinates[vertex_index * 2 + 1] = half(pos_z);
                texture_uvs[vertex_index * 2 + 0] = half(x * u_step);
                texture_uvs[vertex_index * 2 + 1] = half(z * v_step);
                vertex_index++;
            }
        }
        int index_ptr = 0;
        for (unsigned z = 0; z < z_slices; ++z) {
            for (unsigned x = 0; x < x_slices; ++x) {
                unsigned int tl = (z * n_vertices_x) + x;
                unsigned int tr = tl + 1;
                unsigned int bl = ((z + 1) * n_vertices_x) + x;
                unsigned int br = bl + 1;
                indices[index_ptr++] = tl; indices[index_ptr++] = bl; indices[index_ptr++] = tr;
                indices[index_ptr++] = tr; indices[index_ptr++] = bl; indices[index_ptr++] = br;
            }
        }
        glGenVertexArrays(1, &vao_id);
        glGenBuffers(VBO_COUNT, vbo_ids);
        glBindVertexArray(vao_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[COORDINATES_VBO]);
        glBufferData(GL_ARRAY_BUFFER, coordinates.size() * sizeof(half), coordinates.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_HALF_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[TEXTURE_UVS_VBO]);
        glBufferData(GL_ARRAY_BUFFER, texture_uvs.size() * sizeof(half), texture_uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_HALF_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_ids[INDICES_VBO]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        compile_shaders();
        height_map_id = load_texture("../../../shared/assets/height-map.png");
        diffuse_map_id = load_texture("../../../shared/assets/MountainTexture.png");
    }

    Terrain::~Terrain()
    {
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(VBO_COUNT, vbo_ids);
        glDeleteProgram(shader_program_id);
        glDeleteTextures(1, &height_map_id);
        glDeleteTextures(1, &diffuse_map_id);
    }

    void Terrain::render(const Camera& camera, Light* light)
    {
        glUseProgram(shader_program_id);

        // Obtenemos matrices del grafo
        glm::mat4 model = get_global_matrix();
        glm::mat4 view = camera.get_transform_matrix_inverse();
        glm::mat4 proj = camera.get_projection_matrix();
        glm::mat4 normal_mat = glm::transpose(glm::inverse(model));

        glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(normal_matrix_loc, 1, GL_FALSE, glm::value_ptr(normal_mat));

        // Pasamos datos de luz
        if (light) {
            glm::vec3 lpos = light->get_position();
            glUniform3f(light_pos_loc, lpos.x, lpos.y, lpos.z);
            glm::vec3 lcol = light->get_color();
            glUniform3f(light_color_loc, lcol.x, lcol.y, lcol.z);
        }
        else {
            glUniform3f(light_pos_loc, 0, 10, 0);
            glUniform3f(light_color_loc, 1, 1, 1);
        }

        glm::vec4 cpos = camera.get_location();
        glUniform3f(glGetUniformLocation(shader_program_id, "viewPos"), cpos.x, cpos.y, cpos.z);

        // Configuración atmósfera
        glUniform1f(fog_near_loc, 5.0f);
        glUniform1f(fog_far_loc, 50.0f);
        glUniform3f(fog_color_loc, 0.5f, 0.5f, 0.5f);
        glUniform1f(max_height_loc, 5.0f);

        // Bind Texturas
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, height_map_id);
        glUniform1i(sampler_height_loc, 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, diffuse_map_id);
        glUniform1i(sampler_diffuse_loc, 1);

        glBindVertexArray(vao_id);
        glDrawElements(GL_TRIANGLES, number_of_indices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        Node::render(camera, light);
    }

    void Terrain::compile_shaders()
    {
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        const char* vs = terrain_v_shader.c_str(); const char* fs = terrain_f_shader.c_str();
        glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        shader_program_id = glCreateProgram();
        glAttachShader(shader_program_id, v); glAttachShader(shader_program_id, f);
        glLinkProgram(shader_program_id);
        glDeleteShader(v); glDeleteShader(f);

        model_loc = glGetUniformLocation(shader_program_id, "model");
        view_loc = glGetUniformLocation(shader_program_id, "view");
        proj_loc = glGetUniformLocation(shader_program_id, "projection");
        normal_matrix_loc = glGetUniformLocation(shader_program_id, "normal_matrix");
        max_height_loc = glGetUniformLocation(shader_program_id, "max_height");
        sampler_height_loc = glGetUniformLocation(shader_program_id, "sampler");
        sampler_diffuse_loc = glGetUniformLocation(shader_program_id, "diffuse_sampler");
        fog_near_loc = glGetUniformLocation(shader_program_id, "fog_near");
        fog_far_loc = glGetUniformLocation(shader_program_id, "fog_far");
        fog_color_loc = glGetUniformLocation(shader_program_id, "fog_color");
        light_pos_loc = glGetUniformLocation(shader_program_id, "lightPos");
        light_color_loc = glGetUniformLocation(shader_program_id, "lightColor");
    }

    GLuint Terrain::load_texture(const std::string& path)
    {
        int w, h, c; unsigned char* data = SOIL_load_image(path.c_str(), &w, &h, &c, SOIL_LOAD_RGBA);
        if (!data) return 0;
        GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); SOIL_free_image_data(data);
        return id;
    }
}