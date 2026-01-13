// Este c?digo es de dominio p?blico
// angel.rodriguez@udit.es
//samuel.campos@alumnos.udit.es

#include "Terrain.hpp"
#include <glm.hpp>
#include <half.hpp>
#include <vector>

using glm::vec3;
using std::vector;
using half_float::half;

namespace udit
{

    Terrain::Terrain(float width, float depth, unsigned x_slices, unsigned z_slices)
    {
        unsigned n_vertices_x = x_slices + 1;
        unsigned n_vertices_z = z_slices + 1;

        number_of_vertices = n_vertices_x * n_vertices_z;
        number_of_indices = x_slices * z_slices * 6;

        vector< half > coordinates(number_of_vertices * 2);
        vector< half > texture_uvs(number_of_vertices * 2);
        vector< unsigned int > indices(number_of_indices);

        float x_step = width / float(x_slices);
        float z_step = depth / float(z_slices);
        float u_step = 1.f / float(x_slices);
        float v_step = 1.f / float(z_slices);

        int vertex_index = 0;

        for (unsigned z = 0; z < n_vertices_z; ++z)
        {
            for (unsigned x = 0; x < n_vertices_x; ++x)
            {
                // Posici?n (centrada en 0,0)
                float pos_x = -width * 0.5f + x * x_step;
                float pos_z = -depth * 0.5f + z * z_step;

                // Coordenadas de textura
                float tex_u = x * u_step;
                float tex_v = z * v_step;

                // Se guardan en los vectores
                coordinates[vertex_index * 2 + 0] = half(pos_x);
                coordinates[vertex_index * 2 + 1] = half(pos_z);

                texture_uvs[vertex_index * 2 + 0] = half(tex_u);
                texture_uvs[vertex_index * 2 + 1] = half(tex_v);

                vertex_index++;
            }
        }

        int index_ptr = 0;
        for (unsigned z = 0; z < z_slices; ++z)
        {
            for (unsigned x = 0; x < x_slices; ++x)
            {
                // Indices de los 4 vertices de un cuadrado (Quad)
                unsigned int top_left = (z * n_vertices_x) + x;
                unsigned int top_right = top_left + 1;
                unsigned int bottom_left = ((z + 1) * n_vertices_x) + x;
                unsigned int bottom_right = bottom_left + 1;

                // Triangulo 1 (Top-Left -> Bottom-Left -> Top-Right)
                indices[index_ptr++] = top_left;
                indices[index_ptr++] = bottom_left;
                indices[index_ptr++] = top_right;

                // Triangulo 2 (Top-Right -> Bottom-Left -> Bottom-Right)
                indices[index_ptr++] = top_right;
                indices[index_ptr++] = bottom_left;
                indices[index_ptr++] = bottom_right;
            }
        }

        glGenVertexArrays(1, &vao_id);
        glGenBuffers(VBO_COUNT, vbo_ids);

        glBindVertexArray(vao_id);

        // VBO de Coordenadas
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[COORDINATES_VBO]);
        glBufferData(GL_ARRAY_BUFFER, coordinates.size() * sizeof(half), coordinates.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_HALF_FLOAT, GL_FALSE, 0, 0);

        // VBO de UVs
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[TEXTURE_UVS_VBO]);
        glBufferData(GL_ARRAY_BUFFER, texture_uvs.size() * sizeof(half), texture_uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_HALF_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_ids[INDICES_VBO]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    }

    Terrain::~Terrain()
    {
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(VBO_COUNT, vbo_ids);
    }

    void Terrain::render()
    {
        glBindVertexArray(vao_id);
        glDrawElements(GL_TRIANGLES, number_of_indices, GL_UNSIGNED_INT, 0);
    }

}