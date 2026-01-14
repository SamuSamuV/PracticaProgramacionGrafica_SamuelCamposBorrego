#pragma once
#include "Node.hpp"
#include <glm.hpp>

namespace udit
{
    // La Luz es un Nodo más. Esto significa que puede tener posición,
    // rotación y puede ser hija de otros objetos
    class Light : public Node
    {
    private:
        glm::vec3 color;
        float ambient_intensity; // Luz base que siempre está presente
        float diffuse_intensity; // Luz directa que depende del ángulo

    public:
        Light()
            : color(1.0f), ambient_intensity(0.2f), diffuse_intensity(1.0f)
        {
        }

        void set_color(const glm::vec3& c) { color = c; }
        void set_ambient(float a) { ambient_intensity = a; }
        void set_diffuse(float d) { diffuse_intensity = d; }

        glm::vec3 get_color() const { return color; }
        float get_ambient() const { return ambient_intensity; }
        float get_diffuse() const { return diffuse_intensity; }

        // La luz es invisible (no tiene malla), así que su render solo propaga la llamada.
        virtual void render(const Camera& camera, Light* light = nullptr) override
        {
            Node::render(camera, light);
        }
    };
}