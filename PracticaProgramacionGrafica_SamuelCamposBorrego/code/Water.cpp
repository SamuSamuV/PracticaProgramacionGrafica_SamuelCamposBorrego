// Este código es de dominio público
// samuel.campos@alumnos.udit.es

#include "Water.hpp"
#include <vector>
#include <iostream>
#include <gtc/type_ptr.hpp>
#include <SOIL2.h>

namespace udit
{
    // SHADERS DEL AGUA
    const std::string Water::vertex_shader_source = R"(
        #version 330
        
        layout (location = 0) in vec3 vertex_pos;
        layout (location = 1) in vec2 vertex_uv;
        
        uniform mat4 model_view_matrix;
        uniform mat4 projection_matrix;
        uniform float time;
        
        out vec2 uv;
        
        void main() {
            gl_Position = projection_matrix * model_view_matrix * vec4(vertex_pos, 1.0);
            uv = (vertex_uv * 80.0) + vec2(time * 0.05, time * 0.05);
        }
    )";

    const std::string Water::fragment_shader_source = R"(
        #version 330
        
        uniform sampler2D sampler;
        in vec2 uv;
        out vec4 color;
        
        void main() {
            vec4 tex_color = texture(sampler, uv);
            
            // Agua semi-transparente (Alpha 0.6) y azulada
            color = vec4(tex_color.rgb * vec3(0.8, 0.8, 1.0), 0.6); 
        }
    )";

    Water::Water(float width, float depth)
    {
        float y = -2.0f;

        float w = width / 2.0f;
        float d = depth / 2.0f;

        // Vértices (X, Y, Z)
        std::vector<float> vertices = {
            -w, y, -d, // Top Left
            -w, y,  d, // Bottom Left
             w, y, -d, // Top Right
             w, y, -d, // Top Right
            -w, y,  d, // Bottom Left
             w, y,  d  // Bottom Right
        };

        // UVs
        std::vector<float> uvs = {
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f
        };

        vertex_count = 6;

        glGenVertexArrays(1, &vao_id);
        glGenBuffers(2, vbo_ids);

        glBindVertexArray(vao_id);

        // VBO Posiciones
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[0]);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        // VBO UVs
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[1]);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glBindVertexArray(0);

        // Shaders y Textura
        program_id = compile_shaders(vertex_shader_source, fragment_shader_source);

        model_view_loc = glGetUniformLocation(program_id, "model_view_matrix");
        projection_loc = glGetUniformLocation(program_id, "projection_matrix");
        time_loc = glGetUniformLocation(program_id, "time");
        sampler_loc = glGetUniformLocation(program_id, "sampler");

        texture_id = load_texture("../../../shared/assets/WaterTexture.png");
    }

    Water::~Water()
    {
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(2, vbo_ids);
        glDeleteProgram(program_id);
        glDeleteTextures(1, &texture_id);
    }

    void Water::render(const glm::mat4& view, const glm::mat4& projection, float time)
    {
        glUseProgram(program_id);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glUniformMatrix4fv(model_view_loc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform1f(time_loc, time);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glUniform1i(sampler_loc, 0);

        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glUseProgram(0);
    }

    GLuint Water::load_texture(const std::string& path)
    {
        int w, h, c;
        unsigned char* data = SOIL_load_image(path.c_str(), &w, &h, &c, SOIL_LOAD_RGBA);
        if (!data) {
            std::cerr << "Error cargando textura agua: " << path << std::endl;
            return 0;
        }
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        SOIL_free_image_data(data);
        return id;
    }

    GLuint Water::compile_shaders(const std::string& vs, const std::string& fs)
    {
        GLuint p = glCreateProgram();
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        const char* v_c = vs.c_str();
        const char* f_c = fs.c_str();
        glShaderSource(v, 1, &v_c, NULL);
        glShaderSource(f, 1, &f_c, NULL);
        glCompileShader(v);
        glCompileShader(f);
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    }
}