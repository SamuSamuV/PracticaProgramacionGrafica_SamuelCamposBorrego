#include "Node.hpp"

namespace udit
{
    Node::Node()
        : parent(nullptr),
        position(0.0f),
        rotation(0.0f),
        scale(1.0f),
        local_matrix(1.0f),
        global_matrix(1.0f)
    {
    }

    Node::~Node()
    {
        // Cuando un nodo muere, se lleva a sus hijos con él.
        // Esto facilita mucho la gestión de memoria del grafo.
        for (auto child : children) {
            delete child;
        }
        children.clear();
    }

    void Node::add_child(Node* child)
    {
        // Establecemos la relación familiar y guardamos al hijo
        child->parent = this;
        children.push_back(child);
    }

    void Node::remove_child(Node* child)
    {
        // Aquí podríamos implementar lógica para quitar un hijo sin borrarlo, si fuera necesario.
    }

    void Node::update()
    {
        // 1. Calculamos la matriz LOCAL basándonos en posición, rotación y escala
        local_matrix = glm::mat4(1.0f);
        local_matrix = glm::translate(local_matrix, position);
        // Rotamos en orden X -> Y -> Z (esto puede causar Gimbal Lock, pero es simple y funciona)
        local_matrix = glm::rotate(local_matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        local_matrix = glm::rotate(local_matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        local_matrix = glm::rotate(local_matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        local_matrix = glm::scale(local_matrix, scale);

        // 2. Calculamos la matriz GLOBAL
        // Si tengo padre, mi posición en el mundo depende de la suya.
        // Multiplicamos Padre * Yo (el orden importa mucho en matrices).
        if (parent) {
            global_matrix = parent->global_matrix * local_matrix;
        }
        else {
            // Si soy huérfano (o la raíz), mi local es mi global.
            global_matrix = local_matrix;
        }

        // 3. Propagamos la actualización a todos los hijos (Recursividad)
        for (auto child : children) {
            child->update();
        }
    }

    void Node::render(const Camera& camera, Light* light)
    {
        // Por defecto, un Nodo genérico no dibuja nada, solo le dice a sus hijos que se dibujen.
        for (auto child : children) {
            child->render(camera, light);
        }
    }
}