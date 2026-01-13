// Este c?digo es de dominio p?blico
// samuel.campos@alumnos.udit.es

#ifndef GROUND_HEADER
#define GROUND_HEADER

#include <glad/gl.h>

namespace udit
{

    class Terrain
    {
    private:

        // ?ndices para indexar el array vbo_ids:
        enum
        {
            COORDINATES_VBO,
            TEXTURE_UVS_VBO,
            INDICES_VBO,
            VBO_COUNT
        };

    private:

        GLuint  vao_id;
        GLuint  vbo_ids[VBO_COUNT];

        GLsizei number_of_vertices;
        GLsizei number_of_indices;

    public:

        Terrain(float width, float depth, unsigned x_slices, unsigned z_slices);
        ~Terrain();

    public:

        void render();

    };

}

#endif