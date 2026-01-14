#include "Cube.hpp"
#include <gtc/type_ptr.hpp>
#include <SOIL2.h>
#include <iostream>

namespace udit
{
    const GLfloat Cube::coordinates[] = {
          -1.f, +1.f, +1.f, +1.f, +1.f, +1.f, +1.f, -1.f, +1.f, -1.f, -1.f, +1.f,
          -1.f, +1.f, -1.f, -1.f, +1.f, +1.f, -1.f, -1.f, +1.f, -1.f, -1.f, -1.f,
          +1.f, +1.f, -1.f, -1.f, +1.f, -1.f, -1.f, -1.f, -1.f, +1.f, -1.f, -1.f,
          +1.f, +1.f, +1.f, +1.f, +1.f, -1.f, +1.f, -1.f, -1.f, +1.f, -1.f, +1.f,
          -1.f, -1.f, +1.f, +1.f, -1.f, +1.f, +1.f, -1.f, -1.f, -1.f, -1.f, -1.f,
          -1.f, +1.f, -1.f, +1.f, +1.f, -1.f, +1.f, +1.f, +1.f, -1.f, +1.f, +1.f,
    };

    const GLfloat Cube::normals[] = {
         0,0,1, 0,0,1, 0,0,1, 0,0,1,     // Frontal
        -1,0,0, -1,0,0, -1,0,0, -1,0,0,  // Izquierda
         0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, // Trasera
         1,0,0, 1,0,0, 1,0,0, 1,0,0,     // Derecha
         0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, // Abajo
         0,1,0, 0,1,0, 0,1,0, 0,1,0      // Arriba
    };

    const GLfloat Cube::texture_uvs[] = {
          0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
          0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
          0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
          0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
          0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
          0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
    };

    const GLubyte Cube::indices[] = {
           1,  0,  3, 1,  3,  2, 5,  4,  7, 5,  7,  6, 9,  8, 11, 9, 11, 10,
          13, 12, 15, 13, 15, 14, 17, 16, 19, 17, 19, 18, 20, 23, 22, 20, 22, 21,
    };

    const std::string cube_vs = R"(
        #version 330
        layout (location = 0) in vec3 aPos;    // Posición del vértice
        layout (location = 1) in vec2 aTex;    // Coordenada de textura
        layout (location = 2) in vec3 aNormal; // Normal (hacia dónde mira)

        uniform mat4 model;         // Matriz del objeto
        uniform mat4 view;          // Matriz de la cámara
        uniform mat4 projection;    // Matriz de perspectiva
        uniform mat4 normal_matrix; // Matriz para rotar correctamente las normales

        out vec2 TexCoords;
        out vec3 Normal;
        out vec3 FragPos; // Posición del fragmento en el mundo real

        void main() {
           // Calculamos la posición real en el mundo
           FragPos = vec3(model * vec4(aPos, 1.0));
           
           // Rotamos la normal (usamos normal_matrix para evitar deformaciones si hay escalado)
           Normal = mat3(normal_matrix) * aNormal; 
           
           TexCoords = aTex;
           
           // Posición final en pantalla
           gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";

    const std::string cube_fs = R"(
        #version 330
        out vec4 FragColor;

        in vec2 TexCoords;
        in vec3 Normal;
        in vec3 FragPos;

        uniform sampler2D samplers[2]; 
        uniform float alpha_value;
        
        // Datos de la luz y cámara
        uniform vec3 lightPos;
        uniform vec3 lightColor;
        uniform float ambientStrength;
        uniform float diffuseStrength;
        uniform vec3 viewPos; 

        void main() {
            // Mezclamos las dos texturas (damero y madera) al 50%
            vec4 tex1 = texture(samplers[0], TexCoords);
            vec4 tex2 = texture(samplers[1], TexCoords);
            vec4 objectColor = mix(tex1, tex2, 0.5);

            // 1. Luz Ambiental (siempre ilumina un poco)
            vec3 ambient = ambientStrength * lightColor;

            // 2. Luz Difusa (depende de si la cara mira a la luz)
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0); // Si es negativo (luz por detrás), es 0
            vec3 diffuse = diffuseStrength * diff * lightColor;

            // 3. Luz Especular (Brillos - Blinn Phong)
            float specularStrength = 0.5;
            vec3 viewDir = normalize(viewPos - FragPos);
            // El 'halfway vector' es el truco de Blinn-Phong para brillos más suaves
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 64.0); // 64.0 define lo pequeño/concentrado que es el brillo
            vec3 specular = specularStrength * spec * lightColor;

            // Sumamos todo y multiplicamos por el color del objeto
            vec3 result = (ambient + diffuse + specular) * objectColor.rgb;
            FragColor = vec4(result, alpha_value);
        }
    )";

    Cube::Cube() : opacity(1.0f)
    {
        // Generamos los buffers (VBOs) y el VAO
        glGenBuffers(VBO_COUNT, vbo_ids);
        glGenVertexArrays(1, &vao_id);
        glBindVertexArray(vao_id);

        // Subimos coordenadas
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[COORDINATES_VBO]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(coordinates), coordinates, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        // Subimos UVs
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[TEXTURE_UVS_VBO]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(texture_uvs), texture_uvs, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

        // Subimos Normales (Crucial para la luz)
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[NORMALS_VBO]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

        // Subimos Índices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_ids[INDICES_IBO]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glBindVertexArray(0);

        compile_shaders();

        // Cargamos texturas
        texture1_id = load_texture("../../../shared/assets/uv-checker.png");
        texture2_id = load_texture("../../../shared/assets/wood.png");
    }

    Cube::~Cube() {
        // Limpiamos memoria de la GPU
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(VBO_COUNT, vbo_ids);
        glDeleteProgram(program_id);
        glDeleteTextures(1, &texture1_id);
        glDeleteTextures(1, &texture2_id);
    }

    void Cube::render(const Camera& camera, Light* light)
    {
        glUseProgram(program_id);

        // Si el cubo es transparente, activamos el Blend
        if (opacity < 1.0f) {
            glDepthMask(GL_FALSE); // No escribimos en el Z-Buffer para ver a través
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        // Calculamos matrices
        glm::mat4 model = get_global_matrix(); // Matriz del Grafo de Escena
        glm::mat4 view = camera.get_transform_matrix_inverse();
        glm::mat4 normal_mat = glm::transpose(glm::inverse(model)); // Inversa transpuesta para normales

        // Enviamos matrices
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, glm::value_ptr(camera.get_projection_matrix()));
        glUniformMatrix4fv(normal_matrix_loc, 1, GL_FALSE, glm::value_ptr(normal_mat));
        glUniform1f(alpha_loc, opacity);

        // Enviamos datos de la luz (si existe)
        if (light) {
            glm::vec3 l_pos = light->get_position();
            glUniform3f(light_pos_loc, l_pos.x, l_pos.y, l_pos.z);
            glm::vec3 l_col = light->get_color();
            glUniform3f(light_color_loc, l_col.x, l_col.y, l_col.z);
            glUniform1f(light_ambient_loc, light->get_ambient());
            glUniform1f(light_diffuse_loc, light->get_diffuse());
        }
        else {
            // Valores por defecto si no hay luz
            glUniform3f(light_pos_loc, 0, 10, 0);
            glUniform3f(light_color_loc, 1, 1, 1);
            glUniform1f(light_ambient_loc, 0.2f);
            glUniform1f(light_diffuse_loc, 1.0f);
        }

        // Enviamos posición de cámara para el brillo especular
        glm::vec4 cam_pos = camera.get_location();
        glUniform3f(view_pos_loc, cam_pos.x, cam_pos.y, cam_pos.z);

        // Activamos texturas
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture1_id);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture2_id);
        glUniform1i(glGetUniformLocation(program_id, "samplers[0]"), 0);
        glUniform1i(glGetUniformLocation(program_id, "samplers[1]"), 1);

        // Dibujamos
        glBindVertexArray(vao_id);
        glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_BYTE, 0);
        glBindVertexArray(0);

        // Restauramos estado si era transparente
        if (opacity < 1.0f) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        }

        Node::render(camera, light);
    }

    void Cube::compile_shaders() {
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        const char* vs = cube_vs.c_str(); const char* fs = cube_fs.c_str();
        glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        program_id = glCreateProgram();
        glAttachShader(program_id, v); glAttachShader(program_id, f); glLinkProgram(program_id);
        glDeleteShader(v); glDeleteShader(f);

        // Buscamos las 'locations' de los uniforms para usarlas luego
        model_loc = glGetUniformLocation(program_id, "model");
        view_loc = glGetUniformLocation(program_id, "view");
        proj_loc = glGetUniformLocation(program_id, "projection");
        normal_matrix_loc = glGetUniformLocation(program_id, "normal_matrix");
        alpha_loc = glGetUniformLocation(program_id, "alpha_value");
        light_pos_loc = glGetUniformLocation(program_id, "lightPos");
        light_color_loc = glGetUniformLocation(program_id, "lightColor");
        light_ambient_loc = glGetUniformLocation(program_id, "ambientStrength");
        light_diffuse_loc = glGetUniformLocation(program_id, "diffuseStrength");
        view_pos_loc = glGetUniformLocation(program_id, "viewPos");
    }

    GLuint Cube::load_texture(const std::string& path) {
        // Carga de textura usando la librería SOIL
        int w, h, c; unsigned char* data = SOIL_load_image(path.c_str(), &w, &h, &c, SOIL_LOAD_RGBA);
        if (!data) return 0;
        GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); SOIL_free_image_data(data);
        return id;
    }
}