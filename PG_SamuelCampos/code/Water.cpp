#include "Water.hpp"
#include "Light.hpp"
#include <vector>
#include <gtc/type_ptr.hpp>
#include <SOIL2.h>
#include <iostream>

namespace udit
{
    static const std::string water_vs = R"(
        #version 330
        layout (location = 0) in vec3 vertex_pos;
        layout (location = 1) in vec2 vertex_uv;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform float time; // Tiempo acumulado para la animación
        
        out vec2 uv;
        out vec3 FragPos;
        out vec3 Normal; 

        void main() {
            vec4 worldPos = model * vec4(vertex_pos, 1.0);
            FragPos = vec3(worldPos);
            
            // Asumimos que el agua es plana y apunta hacia arriba (0,1,0),
            // pero la rotamos según la matriz del modelo.
            Normal = mat3(model) * vec3(0.0, 1.0, 0.0);

            gl_Position = projection * view * worldPos;
            
            // TRUCO DE ANIMACIÓN:
            // Desplazamos las coordenadas UV con el tiempo para que la textura "se mueva".
            uv = (vertex_uv * 80.0) + vec2(time * 0.05, time * 0.05);
        }
    )";

    static const std::string water_fs = R"(
        #version 330
        uniform sampler2D sampler;
        
        in vec2 uv;
        in vec3 FragPos;
        in vec3 Normal;

        uniform vec3 lightPos;
        uniform vec3 lightColor;
        uniform vec3 viewPos;

        out vec4 color;
        
        void main() {
            vec4 tex_color = texture(sampler, uv);
            
            // Ambient
            float ambientStrength = 0.3;
            vec3 ambient = ambientStrength * lightColor;
            
            // Diffuse
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor;
            
            // Specular (Blinn-Phong) - Fuerte (0.8) porque el agua brilla mucho
            float specularStrength = 0.8; 
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
            vec3 specular = specularStrength * spec * lightColor;  

            vec3 lighting = (ambient + diffuse + specular);

            // Resultado final con transparencia (Alpha 0.6) y un tinte azulado
            color = vec4(lighting * tex_color.rgb * vec3(0.8, 0.8, 1.0), 0.6); 
        }
    )";

    Water::Water(float width, float depth) : time_accum(0.0f)
    {
        // Creamos un cuadrado simple (2 triángulos)
        float y = 0.0f;
        float w = width / 2.0f;
        float d = depth / 2.0f;

        std::vector<float> vertices = { -w, y, -d, -w, y, d, w, y, -d, w, y, -d, -w, y, d, w, y, d };
        std::vector<float> uvs = { 0,1, 0,0, 1,1, 1,1, 0,0, 1,0 };
        vertex_count = 6;

        glGenVertexArrays(1, &vao_id);
        glGenBuffers(2, vbo_ids);
        glBindVertexArray(vao_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[0]);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[1]);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glBindVertexArray(0);

        program_id = compile_shaders();
        texture_id = load_texture("../../../shared/assets/WaterTexture.png");
    }

    Water::~Water()
    {
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(2, vbo_ids);
        glDeleteProgram(program_id);
        glDeleteTextures(1, &texture_id);
    }

    void Water::update()
    {
        time_accum += 0.01f; // Aumentamos el contador de tiempo
        Node::update();      // No olvidar llamar al padre
    }

    void Water::render(const Camera& camera, Light* light)
    {
        glUseProgram(program_id);

        // Activamos mezcla alpha para transparencia
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // El agua transparente no debe ocultar lo que hay detrás en el Z-buffer

        glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(get_global_matrix()));
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(camera.get_transform_matrix_inverse()));
        glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(camera.get_projection_matrix()));
        glUniform1f(time_loc, time_accum);

        if (light) {
            glm::vec3 lpos = light->get_position();
            glUniform3f(glGetUniformLocation(program_id, "lightPos"), lpos.x, lpos.y, lpos.z);
            glm::vec3 lcol = light->get_color();
            glUniform3f(glGetUniformLocation(program_id, "lightColor"), lcol.x, lcol.y, lcol.z);
        }
        else {
            glUniform3f(glGetUniformLocation(program_id, "lightPos"), 0, 10, 0);
            glUniform3f(glGetUniformLocation(program_id, "lightColor"), 1, 1, 1);
        }
        glm::vec4 cpos = camera.get_location();
        glUniform3f(glGetUniformLocation(program_id, "viewPos"), cpos.x, cpos.y, cpos.z);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glUniform1i(sampler_loc, 0);

        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE); // Restauramos Z-buffer
        glDisable(GL_BLEND);

        Node::render(camera, light);
    }

    GLuint Water::compile_shaders()
    {
        GLuint p = glCreateProgram();
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        const char* vs = water_vs.c_str();
        const char* fs = water_fs.c_str();
        glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
        glDeleteShader(v); glDeleteShader(f);

        model_loc = glGetUniformLocation(p, "model");
        view_loc = glGetUniformLocation(p, "view");
        projection_loc = glGetUniformLocation(p, "projection");
        time_loc = glGetUniformLocation(p, "time");
        sampler_loc = glGetUniformLocation(p, "sampler");
        return p;
    }

    GLuint Water::load_texture(const std::string& path) {
        int w, h, c;
        unsigned char* data = SOIL_load_image(path.c_str(), &w, &h, &c, SOIL_LOAD_RGBA);
        if (!data) return 0;
        GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); SOIL_free_image_data(data);
        return id;
    }
}