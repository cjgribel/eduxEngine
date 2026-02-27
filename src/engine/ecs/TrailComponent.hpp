// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TrailComponent_hpp
#define TrailComponent_hpp

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eeng::ecs
{
    struct TrailComponent
    {
        constexpr static unsigned max_trails = 8;

        struct Trail
        {
            /*
                using VertexType = ImPrimitiveRendererNS::LineVertex; // Avoid this dependecy
                static const unsigned max_vertices = 16;
                uint color = BaseColors::White;

                // Multiple trails
                // * MotionTrailComponent contains 4 trails. 0-4 of them active.
                // * Each trail has an offset (m4f)
                // * Supply nbr of trails & offset during construction of the component
                //      Do construction via MotionTrailSystem::spawn.
                //      Do it like this for ALL components?
                //          Well - I did one for MouseForce, but that one also spawned *entities*
                //          Should System::spawn function spawn entities & stuff OR just a component?

        //        struct TrailVertex
        //        {
        ////            VertexType vertex;
        //            v3f point;
        //            uint color;
        //            float age;
        //        };

                VertexType vertices[max_vertices];
                float vertex_ages[max_vertices];
                int start_index = 0;
                int nbr_vertices = 0;

                // "suggest_vertex"
                void add_vertex(const v3f& vertex)
                {
                    const float min_dist = 0.2f;
                    int prev_index = (start_index + nbr_vertices - 1) % max_vertices;
                    if (length_squared(vertices[prev_index].p - vertex) < min_dist*min_dist)
                        return;

                    unsigned index = (start_index + nbr_vertices) % max_vertices;
                    vertices[index] = VertexType {vertex, color};
                    vertex_ages[index] = 0.0f;

                    if (nbr_vertices < max_vertices)    ++nbr_vertices;
                    else                                ++start_index;
                }
                void update(float dt)
                {
                    // System {MotionTrailComponent, Handle<Transform>}
                    // Add vertex using Handle<Transform> * offset

                    const float max_age = 1.0f;
                    for (int i = 0; i < nbr_vertices; i++)
                    {
                        unsigned index = (start_index + i) % max_vertices;
                        // Step age forward & discard vertex if it has reached max age
                        vertex_ages[index] += dt;
                        if (vertex_ages[index] > max_age)
                        {
                            ++start_index;
                            --nbr_vertices;
                            --i;
                            continue;
                        }
                        //vertex_ages[index] = std::min(vertex_ages[index] + dt, max_age);
                        // Blend alpha based on age
                        unsigned char alpha = (unsigned char)((1.0f - vertex_ages[index]/max_age) * 255);
                        vertices[index].color = ((vertices[index].color & 0x00ffffff) | (alpha << 24));
                    }
        //            std::cout << nbr_vertices << std::endl;
                }
                void dump()
                {
                    std::cout << "start_index " << start_index << ", nbr " << nbr_vertices << ": " << std::endl;
                    for (int i = 0; i < nbr_vertices; i++)
                    {
                        unsigned index = (start_index + i) % max_vertices;
                        std::cout
                        << vertices[index].p << ", "
                        << vertices[index].color << ", "
                        << vertex_ages[index] << std::endl;
                    }
                }
                */
            } trails[max_trails];

    };

    std::string to_string(const TrailComponent& t);

    template<typename Visitor>
    void visit_asset_refs(TrailComponent& t, Visitor&& visitor) {}

    template<typename Visitor>
    void visit_entity_refs(TrailComponent& t, Visitor&& visitor) {}


}

#endif // TrailComponent_hpp
