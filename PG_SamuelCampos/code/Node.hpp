#pragma once

#include "Camera.hpp"
#include <vector>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace udit
{
    // Avisamos al compilador de que la clase Light existe (Forward Declaration)
    // para evitar problemas de referencias circulares.
    class Light;

    class Node
    {
    protected:
        // Relación de parentesco: quién es mi padre y quiénes son mis hijos
        Node* parent;
        std::vector<Node*> children;

        // Transformaciones locales (en mi propio espacio)
        glm::vec3 position;
        glm::vec3 rotation; // Ángulos de Euler (X, Y, Z)
        glm::vec3 scale;

        // Matrices para guardar los cálculos
        glm::mat4 local_matrix;  // Mi transformación respecto a mi padre
        glm::mat4 global_matrix; // Mi transformación en el mundo real (World Space)

    public:
        Node();
        virtual ~Node(); // El destructor es virtual para que se llamen los destructores de las clases hijas (Cube, Terrain...)

        // Gestión de la jerarquía
        void add_child(Node* child);
        void remove_child(Node* child);

        // Se llama en cada frame para actualizar lógicas y matrices
        virtual void update();

        // Se llama para dibujar. Recibe la cámara y la luz para pasárselas a los hijos.
        virtual void render(const Camera& camera, Light* light = nullptr);

        // Setters para mover, rotar y escalar el objeto fácilmente
        void set_position(const glm::vec3& pos) { position = pos; }
        void set_rotation(const glm::vec3& rot) { rotation = rot; }
        void set_scale(const glm::vec3& scl) { scale = scl; }

        // Getters para consultar propiedades
        const glm::mat4& get_global_matrix() const { return global_matrix; }
        glm::vec3 get_position() const { return position; }
    };
}