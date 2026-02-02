// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#include <stdio.h>
#include <cstddef>
#include <array>
#include <cmath>

#include <glm/gtc/type_ptr.hpp>

#include "config.h"
#include "glcommon.h"
#include "ShaderLoader.h"
#include "ShapeRenderer.hpp"

namespace ShapeRendering {

#define BUFOFS(offset) ( (GLvoid*)(offset) )

    const Color4u Color4u::Black = Color4u(0xff000000u);
    const Color4u Color4u::White = Color4u(0xffffffffu);
    const Color4u Color4u::Red = Color4u(0xff0000ffu);
    const Color4u Color4u::Lime = Color4u(0xff00ff00u);
    const Color4u Color4u::Blue = Color4u(0xffff0000u);
    const Color4u Color4u::Yellow = Color4u(0xff00ffffu);
    const Color4u Color4u::Cyan = Color4u(0xffffff00u);
    const Color4u Color4u::Magenta = Color4u(0xffff00ffu);
    const Color4u Color4u::Gray = Color4u(0xff808080u);
    const Color4u Color4u::Maroon = Color4u(0xff000080u);
    const Color4u Color4u::Olive = Color4u(0xff808000u);
    const Color4u Color4u::Green = Color4u(0xff008000u);
    const Color4u Color4u::Purple = Color4u(0xff800080u);
    const Color4u Color4u::Teal = Color4u(0xff808000u);
    const Color4u Color4u::Navy = Color4u(0xff800000u);
    const Color4u Color4u::Orange = Color4u(0xffffa500u);
    const Color4u Color4u::Pink = Color4u(0xffffc0cbu);
    const Color4u Color4u::Brown = Color4u(0xffa52a2au);
    const Color4u Color4u::Silver = Color4u(0xffc0c0c0u);
    const Color4u Color4u::Gold = Color4u(0xffffd700u);
    const Color4u Color4u::Turquoise = Color4u(0xff40e0d0u);

    namespace {

        inline glm::vec3 transform_vec(const glm::mat4& mat, const glm::vec3& vec)
        {
            return glm::vec3(mat * glm::vec4(vec, 0.0f));
        }

        inline glm::vec3 transform_pos(const glm::mat4& mat, const glm::vec3& pos)
        {
            return glm::vec3(mat * glm::vec4(pos, 1.0f));
        }

        inline glm::vec3 transform_normal(const glm::mat4& mat, const glm::vec3& normal)
        {
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(mat)));
            return normalMatrix * normal;
        }

        inline glm::mat3 normal_transform(const glm::mat4& mat)
        {
            return glm::transpose(glm::inverse(glm::mat3(mat)));
        }

        inline void append_thick_line_segment(
            std::vector<ThickLineVertex>& vertices,
            std::vector<unsigned>& indices,
            const glm::vec3& start,
            const glm::vec3& end,
            uint color_start,
            uint color_end)
        {
            const glm::vec3 delta = end - start;
            if (glm::dot(delta, delta) < 1.0e-10f)
                return;

            unsigned base = (unsigned)vertices.size();
            vertices.push_back(ThickLineVertex{ start, end, glm::vec2(-1.0f, 0.0f), color_start });
            vertices.push_back(ThickLineVertex{ start, end, glm::vec2(1.0f, 0.0f), color_start });
            vertices.push_back(ThickLineVertex{ start, end, glm::vec2(-1.0f, 1.0f), color_end });
            vertices.push_back(ThickLineVertex{ start, end, glm::vec2(1.0f, 1.0f), color_end });

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 1);
            indices.push_back(base + 3);
            indices.push_back(base + 2);
        }

        glm::mat3 create_basis_from_vector(const glm::vec3& direction)
        {
            // Ensure the direction is normalized
            glm::vec3 forward = glm::normalize(direction);

            // Create a temporary vector for perpendicular calculation
            glm::vec3 temp = (glm::abs(forward.x) > 0.9f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);

            // Calculate the right vector (perpendicular to forward and temp)
            glm::vec3 right = glm::normalize(glm::cross(temp, forward));

            // Calculate the up vector (perpendicular to forward and right)
            glm::vec3 up = glm::cross(forward, right);

            // Create the basis matrix
            glm::mat3 basis(right, up, forward);

            return basis;
        }

        void create_grid_wireframe_indices(
            unsigned hres,
            unsigned rres,
            std::vector<unsigned>& indices)
        {
            if (hres < 2 || rres < 2)
                return;

            const unsigned rmax = rres - 1;
            for (unsigned hi = 0; hi < hres; ++hi)
            {
                for (unsigned ri = 0; ri < rmax; ++ri)
                {
                    unsigned i = hi * rres + ri;
                    indices.push_back(i);
                    indices.push_back(i + 1);

                    if (hi + 1 < hres)
                    {
                        indices.push_back(i);
                        indices.push_back(i + rres);
                    }
                }
            }
        }

        struct unitcube_t
        {
            //static const int face_stride = 4;

            const std::array<glm::vec3, 8> vertices =
            {
                glm::vec3 {0.5f,-0.5f, 0.5f},
                glm::vec3 {0.5f, 0.5f, 0.5f},
                glm::vec3 {-0.5f, 0.5f, 0.5f},
                glm::vec3 {-0.5f,-0.5f, 0.5f},

                glm::vec3 {0.5f,-0.5f,-0.5f},
                glm::vec3 {0.5f, 0.5f,-0.5f},
                glm::vec3 {-0.5f, 0.5f,-0.5f},
                glm::vec3 {-0.5f,-0.5f,-0.5f}
            };

            const std::array<glm::vec3, 6> tri_normals =
            {
                glm::vec3 {0.0f, 0.0f, 1.0f},
                glm::vec3 {0.0f, 0.0f, -1.0f},
                glm::vec3 {1.0f, 0.0f, 0.0f},
                glm::vec3 {-1.0f, 0.0f, 0.0f},
                glm::vec3 {0.0f, 1.0f, 0.0f},
                glm::vec3 {0.0f, -1.0f, 0.0f}
            };

            const std::array<unsigned, 36> tris =
            {
                0,1,2,
                0,2,3,

                4,7,6,
                4,6,5,

                0,4,5,
                0,5,1,

                3,2,6,
                3,6,7,

                1,5,6,
                1,6,2,

                0,3,7,
                0,7,4
            };

            //    const std::vector<unsigned> quad_faces =
            //    {
            //        0,1,2,3,
            //        4,7,6,5,
            //        0,4,5,1,
            //        3,2,6,7,
            //        1,5,6,2,
            //        0,3,7,4
            //    };

            const std::array<unsigned, 24> edges =
            {
                0,1,
                0,3,
                2,1,
                2,3,
                4,5,
                4,7,
                6,5,
                6,7,
                0,4,
                1,5,
                2,6,
                3,7
            };

        } unitcube;

        struct UnitQuad
        {
            //static const int face_stride = 4;

            const std::vector<glm::vec3> vertices =
            {
                glm::vec3(0.5f,     -0.5f,  0.0f),
                glm::vec3(0.5f,     0.5f,   0.0f),
                glm::vec3(-0.5f,    0.5f,   0.0f),
                glm::vec3(-0.5f,    -0.5f,  0.0f)
            };

            //        const std::vector<unsigned> tri_faces =
            //        {
            //            0,1,2,3,
            //            4,7,6,5,
            //            0,4,5,1,
            //            3,2,6,7,
            //            1,5,6,2,
            //            0,3,7,4
            //        };

            //    const std::vector<unsigned> quad_faces =
            //    {
            //        0,1,2,3,
            //        4,7,6,5,
            //        0,4,5,1,
            //        3,2,6,7,
            //        1,5,6,2,
            //        0,3,7,4
            //    };

            const std::vector<unsigned> edges =
            {
                0,1,
                0,3,
                2,1,
                2,3
            };

        } unitquad;

    } // anon namespace

    //
    // CONE
    //
    // height = h
    // radius = r
    //
    // Parametric equation (along z with tip in {0,0,h}):
    //
    //                   |(1-u/h) r cos(theta)  |
    //      P(u,theta) = | (1-u/h) r sin(theta) |
    //                   | u                    |
    // where
    //      0 <= u <= h
    //      0 <= theta <= 2 pi
    //
    // Normal direction = dP/dtheta x dP/du
    //                  = [ simplifying while preserving directions ]
    //                  = (-sin(theta), cos(theta), 0) x (-cos(theta), -sin(theta), h/r)
    //
    //                    | h/r cos(theta) |
    //                  = | h/r sin(theta) |
    //                    | 1              |
    //
    // http://mathworld.wolfram.com/Cone.html
    //
    //
    void create_cone(
        float h,
        float r,
        unsigned hres,
        unsigned rres,
        std::vector<PolyVertex>& vertices,
        std::vector<unsigned>& indices)
    {
        // Vertices
        //
        for (int hi = 0; hi < hres; hi++)
        {
            float z = h * (float)hi / (hres - 1);
            float d = (1.0f - (float)hi / (hres - 1)) * r;

            for (int ri = 0; ri < rres; ri++)
            {
                float theta = 2.0f * 3.14159f * (float)ri / (rres - 1);
                glm::vec3 v = { d * cos(theta), d * sin(theta), z };
                glm::vec3 n = { h / r * cos(theta), h / r * sin(theta), 1 };

                vertices.push_back({ v, normalize(n) });
            }
        }

        // Indices (GL_TRIANGLES)
        //
        for (int hi = 0; hi < hres - 1; hi++) /* once for hi=0 */
            for (int ri = 0; ri < rres - 1; ri++)
            {
                unsigned i = hi * rres + ri;
                unsigned j = hi * rres + ri + 1;
                indices.push_back(i);
                indices.push_back(j);
                indices.push_back(i + rres);

                // Degenerate for top triangle strip
                indices.push_back(j);
                indices.push_back(j + rres);
                indices.push_back(i + rres);
            }
    }

    //
    // CYLINDER
    //
    // Note:
    // GL_TRIANGLE_STRIP works for a cylinder if the indices are generated from a wrapping grid,
    // since triangles straddling each strip then automatically become degenerate (and are discarded
    // automatically).
    // For a flat mesh or something non-wrapping, additional degenerate triangles has to be added in order to
    // separate the strips correctly. This is done by duplicating indices at the start and end of each strip.
    // http://www.learnopengles.com/android-lesson-eight-an-introduction-to-index-buffer-objects-ibos/
    //
    void create_cylinder(
        float h,
        float r,
        unsigned hres,
        unsigned rres,
        std::vector<PolyVertex>& vertices,
        std::vector<unsigned>& indices)
    {
        // Vertices
        //
        for (int hi = 0;hi < hres;hi++)
        {
            float z = h * hi / (hres - 1);
            for (int ri = 0; ri < rres; ri++)
            {
                float alpha = 2.0f * 3.14159f * ri / (rres - 1);
                glm::vec3 v = { r * cos(alpha), r * sin(alpha), z };
                glm::vec3 n = { r * cos(alpha), r * sin(alpha), 0 };

                vertices.emplace_back(v, normalize(n));
            }
        }

#if 0
        // Indices (GL_TRIANGLE_STRIP)
        //
        for (int hi = 0;hi < hres - 1;hi++)
            for (int ri = 0; ri <= rres; ri++)
            {
                unsigned i = hi * rres + ri % rres;
                cylinder_i.push_back(i + rres);
                cylinder_i.push_back(i);
            }
#endif

        // Indices (GL_TRIANGLES)
        //
        for (int hi = 0; hi < hres - 1; hi++)
            for (int ri = 0; ri < rres - 1; ri++)
            {
                unsigned i = hi * rres + ri % rres;
                unsigned j = hi * rres + (ri + 1) % rres;
                indices.push_back(i);
                indices.push_back(j);
                indices.push_back(i + rres);

                indices.push_back(j);
                indices.push_back(j + rres);
                indices.push_back(i + rres);
            }
    }

    //
    // SPHERE
    //
    // radius = r
    //
    // Parametric equation:
    //
    //                      | r sin(theta)sin(phi)  |
    //      P(theta, phi) = | -r cos(phi)           |
    //                      | -r sin(theta)sin(phi) |
    // where
    //      0 <= theta <= 2pi
    //      0 <= phi <= pi
    //
    // Normal direction = t x b
    //                  = dP/dtheta x dP/dphi
    //                  = [ simplifying while preserving directions ]
    //                    | sin(theta)sin(phi) |
    //                  = | -cos(phi)          |
    //                    | cos(theta)sin(phi) |
    //
    void create_sphere(
        float r,
        unsigned thetares,
        unsigned phires,
        std::vector<PolyVertex>& vertices,
        std::vector<unsigned>& indices,
        bool wireframe = false)
    {
        // Vertices
        //
        float theta = 0.0f;			            // steps 0 - 2PI
        float dtheta = 2.0f * 3.14159f / (thetares - 1);	// theta step size, depending on the resolution
        float phi = 0.0f;						// steps 0 - 2PI
        float dphi = 3.14159f / (phires - 1);	// phi step size, depending on the resolution

        for (int phii = 0; phii < phires; phii++)
        {
            float cos_phi = cos(phi);
            float sin_phi = sin(phi);
            theta = 0;

            for (int thetai = 0; thetai < thetares; thetai++)
            {
                float cos_theta = cos(theta);
                float sin_theta = sin(theta);

                glm::vec3 v = { r * sin_theta * sin_phi, -r * cos_phi, r * cos_theta * sin_phi };
                glm::vec3 n = { sin_theta * sin_phi, -cos_phi, cos_theta * sin_phi };
                vertices.push_back({ v, glm::normalize(n) });
                theta += dtheta;
            }
            phi += dphi;
        }

        if (wireframe)
        {
            // Indices (GL_LINES)
            //
            for (int phii = 0; phii < phires - 1; phii++)
                for (int thetai = 0; thetai < thetares - 1; thetai++)
                {
                    unsigned i = thetares * phii + thetai;
                    indices.push_back(i);
                    indices.push_back(i + 1);

                    indices.push_back(i);
                    indices.push_back(i + thetares);
                }
        }
        else
        {
            // Indices (GL_TRIANGLES)
            //
            for (int phii = 0; phii < phires - 1; phii++)
                for (int thetai = 0; thetai < thetares - 1; thetai++)
                {
                    unsigned i = thetares * phii + thetai;
                    indices.push_back(i);
                    indices.push_back(i + 1);
                    indices.push_back(i + thetares);

                    indices.push_back(i + 1);
                    indices.push_back(i + 1 + thetares);
                    indices.push_back(i + thetares);
                }
        }
    }

    void generate_normals(
        std::vector<PolyVertex>& vertices,
        std::vector<unsigned>& indices)
    {
        std::vector<glm::vec3>* v_bin = new std::vector<glm::vec3>[vertices.size()];

        /* Compute triangle normals and bin to vertices */
        for (int i = 0; i < indices.size(); i += 3)
        {
            glm::vec3 v0 = vertices[indices[i + 0]].p;
            glm::vec3 v1 = vertices[indices[i + 1]].p;
            glm::vec3 v2 = vertices[indices[i + 2]].p;
            glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));

            v_bin[indices[i + 0]].push_back(n);
            v_bin[indices[i + 1]].push_back(n);
            v_bin[indices[i + 2]].push_back(n);
        }

        /* Average binned normals */
        for (int i = 0; i < vertices.size(); i++)
        {
            glm::vec3 n(0, 0, 0);
            for (int j = 0; j < v_bin[i].size(); j++)
            {
                n += v_bin[i][j];
            }
            n = glm::normalize(n);

            vertices[i].normal = n;
        }

        delete[] v_bin;
    }

    void ShapeRenderer::init()
    {
        // Create default primitives
        const unsigned cone_hres = 2;
        const unsigned cone_rres = 8;
        const unsigned cylinder_hres = 2;
        const unsigned cylinder_rres = 8;
        const unsigned sphere_thetares = 12;
        const unsigned sphere_phires = 8;

        create_cone(1, 1, cone_hres, cone_rres, unitcone_buffers.vertices, unitcone_buffers.indices);
        create_cylinder(1, 1, cylinder_hres, cylinder_rres, unitcylinder_buffers.vertices, unitcylinder_buffers.indices);
        create_sphere(1, sphere_thetares, sphere_phires, unitsphere_buffers.vertices, unitsphere_buffers.indices, false);
        create_sphere(1, sphere_thetares, sphere_phires, unitspherewireframe_buffers.vertices, unitspherewireframe_buffers.indices, true);

        unitcone_line_buffers.vertices.reserve(unitcone_buffers.vertices.size());
        for (const auto& v : unitcone_buffers.vertices)
            unitcone_line_buffers.vertices.push_back(v.p);
        create_grid_wireframe_indices(cone_hres, cone_rres, unitcone_line_buffers.indices);

        unitcylinder_line_buffers.vertices.reserve(unitcylinder_buffers.vertices.size());
        for (const auto& v : unitcylinder_buffers.vertices)
            unitcylinder_line_buffers.vertices.push_back(v.p);
        create_grid_wireframe_indices(cylinder_hres, cylinder_rres, unitcylinder_line_buffers.indices);

        unitsphere_line_buffers.vertices.reserve(unitspherewireframe_buffers.vertices.size());
        for (const auto& v : unitspherewireframe_buffers.vertices)
            unitsphere_line_buffers.vertices.push_back(v.p);
        unitsphere_line_buffers.indices = unitspherewireframe_buffers.indices;

        // Default state
        push_states(BackfaceCull::True,
            DepthTest::True,
            XRayMode::Off,
            XRayAlpha{},
            LineType::Simple,
            LineStyle{ 1.0f },
            Color4u::White,
            glm::mat4{ 1.0f });

        const GLchar* poly_vshader =
            "#version 410 core\n"
            "layout (location = 0) in vec3 attr_Pos;"
            "layout (location = 1) in vec3 attr_Normal;"
            "layout (location = 2) in vec4 attr_Color;"
            "uniform mat4 PROJ_VIEW;"
            "out vec3 pos, normal;"
            "out vec4 color;"
            ""
            "void main()"
            "{"
            "   pos = attr_Pos;"
            "   normal = attr_Normal;"
            "   color = attr_Color;"
            "   gl_Position = PROJ_VIEW * vec4(attr_Pos, 1);"
            "}";

        const GLchar* poly_fshader =
            "#version 410 core\n"
            "uniform float ambient_ratio;"
            "uniform float alpha_multiplier;"
            ""
            "in vec3 pos, normal;"
            "in vec4 color;"
            "out vec4 fragcolor;"
            "void main()"
            "{"
            "   vec3 l = vec3(1000,1000,1000);"
            "   float lambert = ambient_ratio + (1.0-ambient_ratio) * dot(normal, normalize(l-pos));"
            "   fragcolor = vec4(color.xyz*lambert, color.w * alpha_multiplier);"
            "}";

        const GLchar* simple_line_vshader =
            "#version 410 core\n"
            "layout (location = 0) in vec3 attr_Pos;"
            "layout (location = 1) in vec4 attr_Color;"
            "uniform mat4 PROJ_VIEW;"
            "out vec4 color;"
            ""
            "void main()"
            "{"
            "   color = attr_Color;"
            "   gl_Position = PROJ_VIEW * vec4(attr_Pos, 1);"
            "}";

        const GLchar* simple_line_fshader =
            "#version 410 core\n"
            "uniform float alpha_multiplier;"
            "in vec4 color;"
            "out vec4 fragcolor;"
            "void main()"
            "{"
            "   fragcolor = vec4(color.xyz, color.w * alpha_multiplier);"
            "}";

        const GLchar* thick_line_vshader =
            "#version 410 core\n"
            "layout (location = 0) in vec3 attr_Start;"
            "layout (location = 1) in vec3 attr_End;"
            "layout (location = 2) in vec2 attr_Expand;"
            "layout (location = 3) in vec4 attr_Color;"
            "uniform mat4 PROJ_VIEW;"
            "uniform vec2 viewport;"
            "uniform float line_thickness;"
            "out vec4 color;"
            "out float line_u;"
            ""
            "void main()"
            "{"
            "   vec4 clip0 = PROJ_VIEW * vec4(attr_Start, 1.0);"
            "   vec4 clip1 = PROJ_VIEW * vec4(attr_End, 1.0);"
            "   vec2 ndc0 = clip0.xy / clip0.w;"
            "   vec2 ndc1 = clip1.xy / clip1.w;"
            "   vec2 delta = ndc1 - ndc0;"
            "   float len = length(delta);"
            "   vec2 dir = (len > 0.00001) ? (delta / len) : vec2(0.0, 0.0);"
            "   vec2 normal = vec2(-dir.y, dir.x);"
            "   float half_thickness = line_thickness * 0.5;"
            "   vec2 offset = normal * attr_Expand.x * half_thickness * (vec2(2.0) / viewport);"
            "   float len_px = length(delta * viewport * 0.5);"
            "   vec4 clip = mix(clip0, clip1, attr_Expand.y);"
            "   vec2 ndc = (clip.xy / clip.w) + offset;"
            "   gl_Position = vec4(ndc * clip.w, clip.z, clip.w);"
            "   color = attr_Color;"
            "   line_u = attr_Expand.y * len_px;"
            "}";

        const GLchar* thick_line_fshader =
            "#version 410 core\n"
            "uniform float dash_period;"
            "uniform float dash_ratio;"
            "uniform float dash_offset;"
            "uniform float alpha_multiplier;"
            "in vec4 color;"
            "in float line_u;"
            "out vec4 fragcolor;"
            "void main()"
            "{"
            "   if (dash_period > 0.0)"
            "   {"
            "       float t = (line_u + dash_offset) / dash_period;"
            "       if (fract(t) > dash_ratio) discard;"
            "   }"
            "   fragcolor = vec4(color.xyz, color.w * alpha_multiplier);"
            "}";

        const GLchar* point_vshader =
            "#version 410 core\n"
            "layout (location = 0) in vec3 attr_Pos;"
            "layout (location = 1) in vec4 attr_Color;"
            "uniform mat4 PROJ_VIEW;"
            "out vec4 color;"
            ""
            "void main()"
            "{"
            "   color = attr_Color;"
            "   gl_Position = PROJ_VIEW * vec4(attr_Pos, 1);"
            "}";

        const GLchar* point_fshader =
            "#version 410 core\n"
            "uniform float alpha_multiplier;"
            "in vec4 color;"
            "out vec4 fragcolor;"
            ""
            "void main()"
            "{"
            "   fragcolor = vec4(color.xyz, color.w * alpha_multiplier);"
            "}";

        const GLuint poly_pos_location = 0;
        const GLuint poly_normal_location = 1;
        const GLuint poly_color_location = 2;

        const GLuint simple_line_pos_location = 0;
        const GLuint simple_line_color_location = 1;

        const GLuint thick_line_start_location = 0;
        const GLuint thick_line_end_location = 1;
        const GLuint thick_line_expand_location = 2;
        const GLuint thick_line_color_location = 3;

        const GLuint point_pos_location = 0;
        const GLuint point_color_location = 1;

        lambert_shader = createShaderProgram(poly_vshader, poly_fshader);
        simple_line_shader = createShaderProgram(simple_line_vshader, simple_line_fshader);
        thick_line_shader = createShaderProgram(thick_line_vshader, thick_line_fshader);
        point_shader = createShaderProgram(point_vshader, point_fshader);

        //
        // Init polygon buffers
        //

        glGenVertexArrays(1, &polygon_vao);
        glGenBuffers(1, &polygon_vbo);
        glGenBuffers(1, &polygon_ibo);

        glBindVertexArray(polygon_vao);
        glBindBuffer(GL_ARRAY_BUFFER, polygon_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, polygon_ibo);

        assert(std::is_trivial_v<PolyVertex> || std::is_standard_layout_v<PolyVertex>);
        glEnableVertexAttribArray(poly_pos_location);
        glEnableVertexAttribArray(poly_normal_location);
        glEnableVertexAttribArray(poly_color_location);
        glVertexAttribPointer(poly_pos_location,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(PolyVertex),
            (GLvoid*)offsetof(PolyVertex, p));
        glVertexAttribPointer(poly_normal_location,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(PolyVertex),
            (GLvoid*)offsetof(PolyVertex, normal));
        glVertexAttribPointer(poly_color_location,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            sizeof(PolyVertex),
            (GLvoid*)offsetof(PolyVertex, color));

        glBindVertexArray(0);

        //
        // Init simple line buffers
        //

        glGenVertexArrays(1, &simple_lines_VAO);
        glGenBuffers(1, &simple_lines_VBO);
        glGenBuffers(1, &simple_lines_IBO);

        glBindVertexArray(simple_lines_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, simple_lines_VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, simple_lines_IBO);

        assert(std::is_trivial_v<LineVertex> || std::is_standard_layout_v<LineVertex>);
        glEnableVertexAttribArray(simple_line_pos_location);
        glEnableVertexAttribArray(simple_line_color_location);
        glVertexAttribPointer(simple_line_pos_location,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(LineVertex),
            (GLvoid*)offsetof(LineVertex, p));
        glVertexAttribPointer(simple_line_color_location,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            sizeof(LineVertex),
            (GLvoid*)offsetof(LineVertex, color));

        glBindVertexArray(0);

        //
        // Init thick line buffers
        //

        glGenVertexArrays(1, &lines_VAO);
        glGenBuffers(1, &lines_VBO);
        glGenBuffers(1, &lines_IBO);

        glBindVertexArray(lines_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, lines_VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lines_IBO);

        assert(std::is_trivial_v<ThickLineVertex> || std::is_standard_layout_v<ThickLineVertex>);
        glEnableVertexAttribArray(thick_line_start_location);
        glEnableVertexAttribArray(thick_line_end_location);
        glEnableVertexAttribArray(thick_line_expand_location);
        glEnableVertexAttribArray(thick_line_color_location);
        glVertexAttribPointer(thick_line_start_location,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(ThickLineVertex),
            (GLvoid*)offsetof(ThickLineVertex, start));
        glVertexAttribPointer(thick_line_end_location,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(ThickLineVertex),
            (GLvoid*)offsetof(ThickLineVertex, end));
        glVertexAttribPointer(thick_line_expand_location,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(ThickLineVertex),
            (GLvoid*)offsetof(ThickLineVertex, expand));
        glVertexAttribPointer(thick_line_color_location,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            sizeof(ThickLineVertex),
            (GLvoid*)offsetof(ThickLineVertex, color));

        glBindVertexArray(0);

        //
        // Init point buffers
        //

        glGenVertexArrays(1, &point_vao);
        glGenBuffers(1, &point_vbo);

        glBindVertexArray(point_vao);
        glBindBuffer(GL_ARRAY_BUFFER, point_vbo);

        assert(std::is_trivial_v<PointVertex> || std::is_standard_layout_v<PointVertex>);
        glEnableVertexAttribArray(point_pos_location);
        glEnableVertexAttribArray(point_color_location);
        glVertexAttribPointer(point_pos_location,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(PointVertex),
            (GLvoid*)offsetof(PointVertex, p));
        glVertexAttribPointer(point_color_location,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            sizeof(PointVertex),
            (GLvoid*)offsetof(PointVertex, color));

        glBindVertexArray(0);
        CheckAndThrowGLErrors();

        initialized = true;
    }

    void ShapeRenderer::push_quad(
        const glm::vec3 points[4],
        const glm::vec3& n)
    {
        const auto [color, depth_test, cull_face, M] = get_states<Color4u, DepthTest, BackfaceCull, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const auto Mn = normal_transform(M);
        static const unsigned tri_indices[] = { 0, 1, 2, 0, 2, 3 };

        unsigned vertex_ofs = (unsigned)polygon_vertices.size();
        GLsizei index_ofs = (GLsizei)polygon_indices.size();

        for (int i = 0; i < 4; i++)
            polygon_vertices.emplace_back(transform_pos(M, points[i]), transform_vec(Mn, n), color);

        polygon_indices.insert(
            polygon_indices.end(),
            tri_indices,
            tri_indices + 6);

        polygon_hash.insert({
            PolygonBatch {GL_TRIANGLES, depth_test, cull_face, xray_mode, xray_alpha},
            IndexRange {index_ofs, 6, (GLint)vertex_ofs}
            });
    }

    void ShapeRenderer::push_quad_wireframe()
    {
        push_lines(unitquad.vertices, unitquad.edges);
    }

    void ShapeRenderer::push_cube()
    {
        const auto [color, depth_test, cull_face, M] = get_states<Color4u, DepthTest, BackfaceCull, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        unsigned vertex_ofs = (unsigned)polygon_vertices.size();
        GLsizei index_ofs = (GLsizei)polygon_indices.size();

        for (auto& v : unitcube.vertices)
        {
            const glm::vec3& vm = glm::vec3(M * glm::vec4(v, 1.0f));
            const glm::vec3 nm = glm::normalize(glm::vec3(M * glm::vec4(v, 0.0f)));
            polygon_vertices.push_back(PolyVertex{ vm, nm, color });
        }

        polygon_indices.insert(polygon_indices.end(),
            unitcube.tris.begin(),
            unitcube.tris.end());

        polygon_hash.insert({
            PolygonBatch {GL_TRIANGLES, depth_test, cull_face, xray_mode, xray_alpha},
            IndexRange {index_ofs, unitcube.tris.size(), (GLint)vertex_ofs}
            });
    }

    void ShapeRenderer::push_cube_wireframe()
    {
        const auto& transform = get_states<glm::mat4>();

        auto vertices = unitcube.vertices;
        // std::for_each(vertices.begin(),
        //     vertices.end(),
        //     [&transform](glm::vec3& v) { v = glm::vec3((transform * glm::vec4(v, 1.0f))); });

        push_lines(vertices.data(),
            vertices.size(),
            unitcube.edges.data(),
            unitcube.edges.size());
    }

    void ShapeRenderer::push_AABB(
        const glm::vec3& min,
        const glm::vec3& max)
    {
        // Bottom face
        push_line(glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z));
        push_line(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, min.y, max.z));
        push_line(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z));
        push_line(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, min.y, min.z));

        // Top face
        push_line(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z));
        push_line(glm::vec3(max.x, max.y, min.z), glm::vec3(max.x, max.y, max.z));
        push_line(glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z));
        push_line(glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, max.y, min.z));

        // Vertical edges connecting top and bottom faces
        push_line(glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, max.y, min.z));
        push_line(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z));
        push_line(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z));
        push_line(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z));
    }

    void ShapeRenderer::push_line(const glm::vec3& pos0, const glm::vec3& pos1)
    {
        const auto& line_type = get_states<LineType>();
        if (line_type == LineType::Thick)
        {
            const auto [color, depth_test, M, style] = get_states<Color4u, DepthTest, glm::mat4, LineStyle>();
            const auto xray_mode = get_states<XRayMode>();
            const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
            const LineBatch batch{ depth_test, style, xray_mode, xray_alpha };
            auto& index_batch = line_hash[batch];
            append_thick_line_segment(line_vertices,
                index_batch,
                transform_pos(M, pos0),
                transform_pos(M, pos1),
                color,
                color);
        }
        else
        {
            push_simple_line(pos0, pos1);
        }
    }

    void ShapeRenderer::push_simple_line(const glm::vec3& pos0, const glm::vec3& pos1)
    {
        const auto [color, depth_test, M] = get_states<Color4u, DepthTest, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;

        const SimpleLineBatch ldc{ GL_LINES, depth_test, xray_mode, xray_alpha };
        unsigned vertex_ofs = (unsigned)simple_line_vertices.size();

        simple_line_vertices.emplace_back(transform_pos(M, pos0), color);
        simple_line_vertices.emplace_back(transform_pos(M, pos1), color);

        simple_line_hash[ldc].push_back(vertex_ofs + 0);
        simple_line_hash[ldc].push_back(vertex_ofs + 1);
    }

    void ShapeRenderer::push_lines_from_cyclic_source(const LineVertex* vertices,
        int start_index,
        int nbr_vertices,
        int max_vertices)
    {
        const auto& line_type = get_states<LineType>();
        if (line_type == LineType::Thick)
        {
            const auto [depth_test, style] = get_states<DepthTest, LineStyle>();
            const auto xray_mode = get_states<XRayMode>();
            const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
            const LineBatch batch{ depth_test, style, xray_mode, xray_alpha };
            auto& index_batch = line_hash[batch];

            for (int i = 0; i < nbr_vertices - 1; i++)
            {
                unsigned idx0 = (start_index + i) % max_vertices;
                unsigned idx1 = (start_index + i + 1) % max_vertices;
                const LineVertex& v0 = vertices[idx0];
                const LineVertex& v1 = vertices[idx1];
                append_thick_line_segment(line_vertices,
                    index_batch,
                    v0.p,
                    v1.p,
                    v0.color,
                    v1.color);
            }
        }
        else
        {
            push_simple_lines_from_cyclic_source(vertices, start_index, nbr_vertices, max_vertices);
        }
    }

    void ShapeRenderer::push_simple_lines_from_cyclic_source(const LineVertex* vertices,
        int start_index,
        int nbr_vertices,
        int max_vertices)
    {
        const auto& depth_test = get_states<DepthTest>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;

        const SimpleLineBatch ldc{ GL_LINES, depth_test, xray_mode, xray_alpha };
        unsigned vertex_ofs = (unsigned)simple_line_vertices.size();

        for (int i = 0; i < nbr_vertices; i++)
        {
            unsigned index = (start_index + i) % max_vertices;
            simple_line_vertices.push_back(vertices[index]);
        }

        for (int i = 0; i < nbr_vertices - 1; i++)
        {
            simple_line_hash[ldc].push_back(vertex_ofs + i);
            simple_line_hash[ldc].push_back(vertex_ofs + i + 1);
        }
        // TODO: Wrapping should be optional
    //    simple_line_hash[0].push_back(vertex_ofs + (unsigned)nbr_vertices - 1);
    //    simple_line_hash[0].push_back(vertex_ofs);
    }

    void ShapeRenderer::push_lines(const std::vector<glm::vec3>& vertices,
        const std::vector<unsigned>& indices)
    {
        const auto& line_type = get_states<LineType>();
        if (line_type == LineType::Thick)
        {
            push_lines(vertices.data(),
                vertices.size(),
                indices.data(),
                indices.size());
        }
        else
        {
            push_simple_lines(vertices, indices);
        }
    }

    void ShapeRenderer::push_simple_lines(const std::vector<glm::vec3>& vertices,
        const std::vector<unsigned>& indices)
    {
        push_simple_lines(vertices.data(),
            vertices.size(),
            indices.data(),
            indices.size());
    }

    void ShapeRenderer::push_lines(const glm::vec3* vertices,
        size_t nbr_vertices,
        const unsigned* indices,
        size_t nbr_indices)
    {
        const auto& line_type = get_states<LineType>();
        if (line_type == LineType::Thick)
        {
            const auto [color, depth_test, M, style] = get_states<Color4u, DepthTest, glm::mat4, LineStyle>();
            const auto xray_mode = get_states<XRayMode>();
            const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
            const LineBatch batch{ depth_test, style, xray_mode, xray_alpha };
            auto& index_batch = line_hash[batch];

            for (int i = 0; i + 1 < nbr_indices; i += 2)
            {
                const glm::vec3 p0 = transform_pos(M, vertices[indices[i]]);
                const glm::vec3 p1 = transform_pos(M, vertices[indices[i + 1]]);
                append_thick_line_segment(line_vertices, index_batch, p0, p1, color, color);
            }
        }
        else
        {
            push_simple_lines(vertices, nbr_vertices, indices, nbr_indices);
        }
    }

    void ShapeRenderer::push_simple_lines(const glm::vec3* vertices,
        size_t nbr_vertices,
        const unsigned* indices,
        size_t nbr_indices)
    {
        const auto [color, depth_test, M] = get_states<Color4u, DepthTest, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;

        const SimpleLineBatch ldc{ GL_LINES, depth_test, xray_mode, xray_alpha };
        unsigned vertex_ofs = (unsigned)simple_line_vertices.size();

        for (int i = 0; i < nbr_vertices; i++)
            simple_line_vertices.emplace_back(transform_pos(M, vertices[i]), color);

        for (int i = 0; i < nbr_indices; i++)
            simple_line_hash[ldc].push_back(vertex_ofs + indices[i]);
    }

    void ShapeRenderer::push_lines(const glm::vec3* vertices,
        size_t nbr_vertices)
    {
        const auto& line_type = get_states<LineType>();
        if (line_type == LineType::Thick)
        {
            const auto [transform, color, depth_test, style] = get_states<glm::mat4, Color4u, DepthTest, LineStyle>();
            const auto xray_mode = get_states<XRayMode>();
            const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
            const LineBatch batch{ depth_test, style, xray_mode, xray_alpha };
            auto& index_batch = line_hash[batch];

            assert(vertices);
            assert(nbr_vertices > 0);

            for (int i = 0; i < nbr_vertices - 1; i++)
            {
                const glm::vec3 p0 = glm::vec3(transform * glm::vec4(vertices[i], 1.0f));
                const glm::vec3 p1 = glm::vec3(transform * glm::vec4(vertices[i + 1], 1.0f));
                append_thick_line_segment(line_vertices, index_batch, p0, p1, color, color);
            }

            // TODO: Wrapping should be optional
            const glm::vec3 p0 = glm::vec3(transform * glm::vec4(vertices[nbr_vertices - 1], 1.0f));
            const glm::vec3 p1 = glm::vec3(transform * glm::vec4(vertices[0], 1.0f));
            append_thick_line_segment(line_vertices, index_batch, p0, p1, color, color);
        }
        else
        {
            push_simple_lines(vertices, nbr_vertices);
        }
    }

    void ShapeRenderer::push_simple_lines(const glm::vec3* vertices,
        size_t nbr_vertices)
    {
        const auto [transform, color, depth_test] = get_states<glm::mat4, Color4u, DepthTest>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;

        assert(vertices);
        assert(nbr_vertices > 0);
        const SimpleLineBatch ldc{ GL_LINES, depth_test, xray_mode, xray_alpha };
        unsigned vertex_ofs = (unsigned)simple_line_vertices.size();

        for (int i = 0; i < nbr_vertices; i++)
            simple_line_vertices.emplace_back(glm::vec3(transform * glm::vec4(vertices[i], 1.0f)), color);

        for (int i = 0; i < nbr_vertices - 1; i++)
        {
            simple_line_hash[ldc].push_back(vertex_ofs + i);
            simple_line_hash[ldc].push_back(vertex_ofs + i + 1);
        }
        // TODO: Wrapping should be optional
        simple_line_hash[ldc].push_back(vertex_ofs + (unsigned)nbr_vertices - 1);
        simple_line_hash[ldc].push_back(vertex_ofs);
    }

    void ShapeRenderer::push_grid(const glm::vec3& pos,
        unsigned size,
        unsigned resolution)
    {
        float sizef = float(size);

        for (int i = 0; i < resolution; i++)
        {
            push_line(
                glm::vec3{ pos.x - sizef / 2 + i * sizef / (resolution - 1), pos.y, pos.z - sizef / 2 },
                glm::vec3{ pos.x - sizef / 2 + i * sizef / (resolution - 1), pos.y, pos.z + sizef / 2 });
            push_line(
                glm::vec3{ pos.x - sizef / 2, pos.y, pos.z - sizef / 2 + i * sizef / (resolution - 1) },
                glm::vec3{ pos.x + sizef / 2, pos.y, pos.z - sizef / 2 + i * sizef / (resolution - 1) });
        }
    }

    void ShapeRenderer::push_cone(const glm::vec3& from,
        const glm::vec3 to,
        float r)
    {
        const glm::vec3 conev = (to - from);
        const float conel = glm::length(conev);

        // Main transform
        const glm::mat4 R = glm::mat4(create_basis_from_vector(conev));
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), from);
        const glm::mat4 M = T * R;

        push_states(M);
        push_cone(conel, r);
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_cone_wireframe(const glm::vec3& from,
        const glm::vec3& to,
        float r)
    {
        const glm::vec3 conev = (to - from);
        const float conel = glm::length(conev);

        const glm::mat4 R = glm::mat4(create_basis_from_vector(conev));
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), from);
        const glm::mat4 M = T * R;

        push_states(M);
        push_cone_wireframe(conel, r);
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_cone(float h,
        float r,
        bool flip_normals)
    {
        const auto [color, depth_test, cull_face, M] = get_states<Color4u, DepthTest, BackfaceCull, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const auto vertex_ofs = polygon_vertices.size();
        GLsizei index_ofs = (GLsizei)polygon_indices.size();

        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(r, r, h));
        glm::mat4 N = M * S;
        glm::mat4 Nit = glm::transpose(glm::inverse(N));

        for (auto& v : unitcone_buffers.vertices)
        {
            glm::vec3 vw = glm::vec3(N * glm::vec4(v.p, 1.0f));
            glm::vec3 nw = glm::vec3(Nit * glm::vec4(v.normal, 0.0f)) * (flip_normals ? -1.0f : 1.0f);
            polygon_vertices.push_back({ vw, glm::normalize(nw), color });
        }

        polygon_indices.insert(polygon_indices.end(),
            unitcone_buffers.indices.begin(),
            unitcone_buffers.indices.end());

        // Hacky way to flip the cone inside out
        if (flip_normals)
            for (int i = index_ofs; i < polygon_indices.size(); i += 3)
                std::swap(polygon_indices[i], polygon_indices[i + 1]);

        polygon_hash.insert({
            PolygonBatch {GL_TRIANGLES, depth_test, cull_face, xray_mode, xray_alpha},
            IndexRange {index_ofs, (GLsizei)unitcone_buffers.indices.size(), (GLint)vertex_ofs}
            });

#if 0
        // Add normals (last [vertex_ofs] added normals)
        for (auto it = std::next(varray.begin(), vertex_ofs); it < varray.end(); it++)
        {
            add_line2(it->p, it->p + it->normal * 0.2f, { 0,1,0 });
        }
#endif
    }

    void ShapeRenderer::push_cone_wireframe(float h,
        float r)
    {
        if (unitcone_line_buffers.vertices.empty() || unitcone_line_buffers.indices.empty())
            return;

        push_states(glm_aux::S(glm::vec3(r, r, h)));
        push_lines(unitcone_line_buffers.vertices.data(),
            unitcone_line_buffers.vertices.size(),
            unitcone_line_buffers.indices.data(),
            unitcone_line_buffers.indices.size());
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_cylinder(float h,
        float r)
    {
        const auto [color, depth_test, cull_face, M] = get_states<Color4u, DepthTest, BackfaceCull, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const auto vertex_ofs = polygon_vertices.size();
        const auto index_ofs = polygon_indices.size();

        glm::mat4 N = M * glm::scale(glm::mat4(1.0f), glm::vec3(r, r, h));
        glm::mat4 Nit = N;

        for (auto& v : unitcylinder_buffers.vertices)
        {
            glm::vec3 vw = glm::vec3(N * glm::vec4(v.p, 1.0f));
            glm::vec3 nw = glm::vec3(Nit * glm::vec4(v.normal, 0.0f));
            polygon_vertices.push_back({ vw, glm::normalize(nw), color });
        }
        polygon_indices.insert(polygon_indices.end(),
            unitcylinder_buffers.indices.begin(),
            unitcylinder_buffers.indices.end());

        polygon_hash.insert({
            PolygonBatch {GL_TRIANGLES, depth_test, cull_face, xray_mode, xray_alpha},
            IndexRange {(GLsizei)index_ofs, (GLsizei)unitcylinder_buffers.indices.size(), (GLint)vertex_ofs}
            });

        // if (ray)
        //     for (int i = 0; i < unitcylinder_buffers.indices.size(); i += 3) {
        //         const v3f& v0 = polygon_vertices[vertex_ofs + unitcylinder_buffers.indices[i + 0]].p;
        //         const v3f& v1 = polygon_vertices[vertex_ofs + unitcylinder_buffers.indices[i + 1]].p;
        //         const v3f& v2 = polygon_vertices[vertex_ofs + unitcylinder_buffers.indices[i + 2]].p;
        //         RayTriangleIntersection(*ray, v0, v1, v2);
        //     }

#if 0
        // Add normals (last [vertex_ofs] added normals)
        for (auto it = std::next(varray.begin(), vertex_ofs); it < varray.end(); it++)
        {
            add_line2(it->p, it->p + it->normal * 0.2f, { 0,1,0 });
        }
#endif
    }

    void ShapeRenderer::push_cylinder_wireframe(float h,
        float r)
    {
        if (unitcylinder_line_buffers.vertices.empty() || unitcylinder_line_buffers.indices.empty())
            return;

        push_states(glm_aux::S(glm::vec3(r, r, h)));
        push_lines(unitcylinder_line_buffers.vertices.data(),
            unitcylinder_line_buffers.vertices.size(),
            unitcylinder_line_buffers.indices.data(),
            unitcylinder_line_buffers.indices.size());
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_arrow(
        const glm::vec3& from,
        const glm::vec3& to,
        ArrowDescriptor arrow_desc)
    {
        glm::vec3 arrowv = (to - from);
        float arrowl = glm::length(arrowv);

        // Main arrow transform
        const glm::mat4 R = glm::mat4(create_basis_from_vector(arrowv));
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), from);
        const glm::mat4 M = T * R;

        // Cone
        float conel = arrowl * arrow_desc.cone_fraction;
        // Skip cone part if arrow is very short
        if (conel > 0.0f)
        {
            const glm::mat4 Mcone = M * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, arrowl - conel));
            push_states(Mcone);
            push_cone(conel, arrow_desc.cone_radius);
            pop_states<glm::mat4>();
        }

        // Cylinder
        float cyll = arrowl - conel;

        push_states(M);
        push_cylinder(cyll, arrow_desc.cylinder_radius);
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_arrow_wireframe(
        const glm::vec3& from,
        const glm::vec3& to,
        ArrowDescriptor arrow_desc)
    {
        glm::vec3 arrowv = (to - from);
        float arrowl = glm::length(arrowv);

        const glm::mat4 R = glm::mat4(create_basis_from_vector(arrowv));
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), from);
        const glm::mat4 M = T * R;

        float conel = arrowl * arrow_desc.cone_fraction;
        if (conel > 0.0f)
        {
            const glm::mat4 Mcone = M * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, arrowl - conel));
            push_states(Mcone);
            push_cone_wireframe(conel, arrow_desc.cone_radius);
            pop_states<glm::mat4>();
        }

        float cyll = arrowl - conel;
        push_states(M);
        push_cylinder_wireframe(cyll, arrow_desc.cylinder_radius);
        pop_states<glm::mat4>();
    }


    void ShapeRenderer::push_sphere(float h, float r)
    {
        const auto [color, depth_test, cull_face, M] = get_states<Color4u, DepthTest, BackfaceCull, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const auto vertex_ofs = polygon_vertices.size();
        const auto index_ofs = polygon_indices.size();

        auto N = M * glm_aux::S(glm::vec3{ r, h, r });
        glm::mat4 Nit;
        if (h == r) Nit = N;
        else { Nit = glm::transpose(glm::inverse(N)); }

        for (auto& v : unitsphere_buffers.vertices)
        {
            glm::vec3 vw = glm::vec3(N * glm::vec4(v.p, 1.0f));
            glm::vec3 nw = glm::vec3(Nit * glm::vec4(v.normal, 0.0f));
            polygon_vertices.push_back({ vw, glm::normalize(nw), color });
        }

        polygon_indices.insert(polygon_indices.end(),
            unitsphere_buffers.indices.begin(),
            unitsphere_buffers.indices.end());

        polygon_hash.insert({
            PolygonBatch {GL_TRIANGLES, depth_test, cull_face, xray_mode, xray_alpha},
            IndexRange {(GLsizei)index_ofs, (GLsizei)unitsphere_buffers.indices.size(), (GLint)vertex_ofs}
            });

#if 0
        // Add normals (last [vertex_ofs] added normals)
        for (auto it = std::next(varray.begin(), vertex_ofs); it < varray.end(); it++)
        {
            add_line2(it->p, it->p + it->normal * 0.2f, { 0,1,0 });
        }
#endif
    }

    void ShapeRenderer::push_sphere_wireframe(float h, float r)
    {
        if (unitsphere_line_buffers.vertices.empty() || unitsphere_line_buffers.indices.empty())
            return;

        push_states(glm_aux::S(glm::vec3{ r, h, r }));
        push_lines(unitsphere_line_buffers.vertices.data(),
            unitsphere_line_buffers.vertices.size(),
            unitsphere_line_buffers.indices.data(),
            unitsphere_line_buffers.indices.size());
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_helix(
        const glm::vec3& from,
        const glm::vec3& to,
        float r_outer,
        float r_inner,
        float revs)
    {
        glm::vec3 helixv = (to - from);
        float helixl = glm::length(helixv);
        if (helixl <= 0.0f || revs <= 0.0f)
            return;

        const glm::mat4 R = glm::mat4(create_basis_from_vector(helixv));
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), from);
        const glm::mat4 M = T * R;

        push_states(M);
        push_helix(helixl, r_outer, r_inner, revs);
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_helix(
        float length,
        float r_outer,
        float r_inner,
        float revs)
    {
        (void)r_inner;
        if (length <= 0.0f || revs <= 0.0f)
            return;

        const int res_per_rev = 24;
        int segments = (int)std::ceil(revs * res_per_rev);
        if (segments < 4)
            segments = 4;

        std::vector<glm::vec3> points;
        std::vector<unsigned> indices;
        points.reserve(segments + 1);
        indices.reserve(segments * 2);

        for (int i = 0; i <= segments; i++)
        {
            float t = (float)i / (float)segments;
            float theta = t * 2.0f * 3.14159f * revs;
            points.emplace_back(std::cos(theta) * r_outer,
                std::sin(theta) * r_outer,
                t * length);
        }

        for (int i = 0; i < segments; i++)
        {
            indices.push_back(i);
            indices.push_back(i + 1);
        }

        push_lines(points.data(), points.size(), indices.data(), indices.size());
    }

    void ShapeRenderer::push_helix_solid(
        const glm::vec3& from,
        const glm::vec3& to,
        float r_outer,
        float r_inner,
        float revs)
    {
        glm::vec3 helixv = (to - from);
        float helixl = glm::length(helixv);
        if (helixl <= 0.0f || revs <= 0.0f || r_inner <= 0.0f)
            return;

        const glm::mat4 R = glm::mat4(create_basis_from_vector(helixv));
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), from);
        const glm::mat4 M = T * R;

        push_states(M);
        push_helix_solid(helixl, r_outer, r_inner, revs);
        pop_states<glm::mat4>();
    }

    void ShapeRenderer::push_helix_solid(
        float length,
        float r_outer,
        float r_inner,
        float revs)
    {
        if (length <= 0.0f || revs <= 0.0f || r_inner <= 0.0f)
            return;

        const auto [color, depth_test, cull_face, Mpos] = get_states<Color4u, DepthTest, BackfaceCull, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        std::vector<PolyVertex> vertices;
        std::vector<unsigned> indices;

        const int res_per_rev = 12;
        int hres = (int)std::ceil(revs * res_per_rev) + 1;
        if (hres < 2)
            hres = 2;
        const int rres = 8;

        create_cylinder(1.0f, r_inner, hres, rres, vertices, indices);

        const glm::mat4 Minit =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, r_outer, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), -0.5f * 3.14159f, glm::vec3(0.0f, 1.0f, 0.0f));

        for (auto& v : vertices)
        {
            float zparam = v.p.z;
            v.p.z = 0.0f;

            float theta = zparam * 2.0f * 3.14159f * revs;
            glm::mat4 Mspin =
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, length * zparam)) *
                glm::rotate(glm::mat4(1.0f), theta, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 M = Mpos * Mspin * Minit;

            v.p = glm::vec3(M * glm::vec4(v.p, 1.0f));
            glm::mat3 N = glm::transpose(glm::inverse(glm::mat3(M)));
            v.normal = glm::normalize(N * v.normal);
            v.color = color;
        }

        unsigned vertex_ofs = (unsigned)polygon_vertices.size();
        GLsizei index_ofs = (GLsizei)polygon_indices.size();

        polygon_vertices.insert(polygon_vertices.end(), vertices.begin(), vertices.end());
        polygon_indices.insert(polygon_indices.end(), indices.begin(), indices.end());

        polygon_hash.insert({
            PolygonBatch {GL_TRIANGLES, depth_test, cull_face, xray_mode, xray_alpha},
            IndexRange {index_ofs, (GLsizei)indices.size(), (GLint)vertex_ofs}
            });
    }

    void ShapeRenderer::push_frustum(
        const glm::mat4& inv_proj_view)
    {
        // Frustum corners in 3D Clip space
        const glm::vec4 points_clip[8] =
        {
            // near
            { -1.0f, -1.0f, -1.0f, 1.0f },
            { 1.0f, -1.0f, -1.0f, 1.0f },
            { 1.0f, 1.0f, -1.0f, 1.0f },
            { -1.0f, 1.0f, -1.0f, 1.0f },
            // far
            { -1.0f, -1.0f, 1.0f, 1.0f },
            { 1.0f, -1.0f, 1.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { -1.0f, 1.0f, 1.0f, 1.0f }
        };

        glm::vec3 points_world[8];
        for (int i = 0; i < 8; i++)
        {
            // Unproject from clip space (NDC in [-1,1]) back to homogeneous world space.
            glm::vec4 p = inv_proj_view * points_clip[i];
            float winv = (std::abs(p.w) > 1.0e-6f) ? (1.0f / p.w) : 1.0f;
            // Perspective divide: convert homogeneous world (x,y,z,w) to 3D world (x/w,y/w,z/w).
            // This mirrors what the GPU does after the vertex shader.
            points_world[i] = glm::vec3(p) * winv;
        }

        for (int i = 0; i < 4; i++)
        {
            push_line(points_world[i], points_world[(i + 1) % 4]);
            push_line(points_world[4 + i], points_world[4 + (i + 1) % 4]);
            push_line(points_world[i], points_world[i + 4]);
        }
    }

    void ShapeRenderer::push_basis_basic(const glm::mat4& basis, float arrlen)
    {
        const glm::vec3 wpos = glm::vec3(basis[3]);

        push_states(Color4u::Red);
        push_line(wpos, wpos + glm::normalize(glm::vec3(basis[0])) * arrlen);
        pop_states<Color4u>();

        push_states(Color4u::Lime);
        push_line(wpos, wpos + glm::normalize(glm::vec3(basis[1])) * arrlen);
        pop_states<Color4u>();

        push_states(Color4u::Blue);
        push_line(wpos, wpos + glm::normalize(glm::vec3(basis[2])) * arrlen);
        pop_states<Color4u>();
    }

    void ShapeRenderer::push_basis_basic2d(const glm::mat4& basis, float arrlen)
    {
        const glm::vec3 wpos = glm::vec3(basis[3]);

        push_states(Color4u::Red);
        push_line(wpos, wpos + glm::normalize(glm::vec3(basis[0])) * arrlen);
        pop_states<Color4u>();

        push_states(Color4u::Lime);
        push_line(wpos, wpos + glm::normalize(glm::vec3(basis[1])) * arrlen);
        pop_states<Color4u>();
    }

    void ShapeRenderer::push_basis(
        const ArrowDescriptor& arrow_desc,
        const glm::vec3 arrow_lengths)
    {
        push_states(Color4u::Red);
        push_arrow(glm_aux::vec3_000, glm_aux::vec3_100 * arrow_lengths.x, arrow_desc);
        pop_states<Color4u>();

        push_states(Color4u::Lime);
        push_arrow(glm_aux::vec3_000, glm_aux::vec3_010 * arrow_lengths.y, arrow_desc);
        pop_states<Color4u>();

        push_states(Color4u::Blue);
        push_arrow(glm_aux::vec3_000, glm_aux::vec3_001 * arrow_lengths.z, arrow_desc);
        pop_states<Color4u>();
    }

    void ShapeRenderer::push_point(const glm::vec3& p, unsigned size)
    {
        const auto [color, depth_test, M] = get_states<Color4u, DepthTest, glm::mat4>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const PointBatch dc{ size, depth_test, xray_mode, xray_alpha };
        point_hash[dc].push_back(PointVertex{ transform_pos(M, p), color });
    }

    void ShapeRenderer::push_point_direct(const glm::vec3& p, unsigned size)
    {
        const auto [color, depth_test] = get_states<Color4u, DepthTest>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const PointBatch dc{ size, depth_test, xray_mode, xray_alpha };
        point_hash[dc].push_back(PointVertex{ p, color });
    }

    void ShapeRenderer::push_points_direct(const PointVertex* points,
        unsigned nbr_points,
        unsigned size)
    {
        const auto& depth_test = get_states<DepthTest>();
        const auto xray_mode = get_states<XRayMode>();
        const float xray_alpha = (xray_mode == XRayMode::On) ? get_states<XRayAlpha>().value : 1.0f;
        const PointBatch dc{ size, depth_test, xray_mode, xray_alpha };

        auto& point_vector = point_hash[dc];
        point_hash[dc].insert(point_vector.end(),
            points,
            points + nbr_points);
    }

    void ShapeRenderer::render(const glm::mat4& PROJ_VIEW)
    {
        assert(initialized);
        framenbr++;

#if 0
        bool wireframe = false;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            //glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
        }
        else {
            glPolygonMode(GL_FRONT, GL_FILL);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE); //glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }
#endif
        //        glPolygonOffset(-1, -1);

#if 1
    // Store state
#ifdef GL_POLYGON_MODE
//    GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
        GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
        GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
        GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
        GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
        GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
        GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
        GLfloat last_poin_size; glGetFloatv(GL_POINT_SIZE, &last_poin_size);
        GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
        GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        GLint last_depth_func = GL_LESS; glGetIntegerv(GL_DEPTH_FUNC, &last_depth_func);
        GLboolean last_depth_mask = GL_TRUE; glGetBooleanv(GL_DEPTH_WRITEMASK, &last_depth_mask);
        //    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glCullFace(GL_BACK);
        //    glEnable(GL_SCISSOR_TEST);
#ifdef GL_POLYGON_MODE
//    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    //
#endif


#if 1
    // Render polygon batches

        if (polygon_hash.size())
        {
            glUseProgram(lambert_shader);
            glBindVertexArray(polygon_vao);

#if 1
            // Reallocate buffers every frame

            glBindBuffer(GL_ARRAY_BUFFER, polygon_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(PolyVertex) * (int)polygon_vertices.size(), &polygon_vertices[0], GL_STREAM_DRAW);
            CheckAndThrowGLErrors();

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, polygon_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * (int)polygon_indices.size(), &polygon_indices[0], GL_STREAM_DRAW);
            CheckAndThrowGLErrors();
#else

            // Reallocate buffers only when needed + stream draw & orphaning
            // This should be a lot faster in theory, at least for large buffers

            static size_t allocated_nbr_poly_vertices = 0;
            static size_t allocated_nbr_poly_indices = 0;

            // Resize only if needed
            if (polygon_vertices.size() > allocated_nbr_poly_vertices)
            {
                glBindBuffer(GL_ARRAY_BUFFER, polygon_vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(PolyVertex) * (int)polygon_vertices.size(), &polygon_vertices[0], GL_STREAM_DRAW);
                allocated_nbr_poly_vertices = polygon_vertices.size();
            }
            if (polygon_indices.size() > allocated_nbr_poly_indices)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, polygon_ibo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * (int)polygon_indices.size(), &polygon_indices[0], GL_STREAM_DRAW);
                allocated_nbr_poly_indices = polygon_vertices.size();
            }

            // == VBO ==
            {
                GLintptr ofs = 0;
                GLsizeiptr size = polygon_vertices.size() * sizeof(PolyVertex);
                const void* data = polygon_vertices.data();

                // Orphan
                glBindBuffer(GL_ARRAY_BUFFER, polygon_vbo);
                glMapBufferRange(GL_ARRAY_BUFFER,
                    0,
                    size,
                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                glUnmapBuffer(GL_ARRAY_BUFFER);

                // Map data
                glBindBuffer(GL_ARRAY_BUFFER, polygon_vbo);
                void* destptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
                memcpy((char*)destptr + ofs, data, size);
                glUnmapBuffer(GL_ARRAY_BUFFER);
            }

            // == IBO ==
            {
                GLintptr ofs = 0;
                GLsizeiptr size = polygon_indices.size() * sizeof(unsigned int);
                const void* data = polygon_indices.data();

                // Orphan
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, polygon_ibo);
                glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER,
                    0,
                    size,
                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

                // Map data
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, polygon_ibo);
                void* destptr = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
                memcpy((char*)destptr + ofs, data, size);
                glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            }
#endif

            // Uniforms
            glUniformMatrix4fv(
                glGetUniformLocation(lambert_shader, "PROJ_VIEW"),
                1,
                0,
                glm::value_ptr(PROJ_VIEW));
            glUniform1f(
                glGetUniformLocation(lambert_shader, "ambient_ratio"),
                0.6);
            const GLint alpha_loc = glGetUniformLocation(lambert_shader, "alpha_multiplier");

            CheckAndThrowGLErrors();

            // geometry v2 (iterate over unique keys)
            //
            // Note: Elements with equal keys (color, topology) are isolated by manual stepping of the
            // iterator. Another way is to use 'count', which returns the number of elements with the same key,
            // but this call has linear time complexity worst case (nbr of found keys on average; which is
            // actually the cost of manual stepping...)

            for (auto it = polygon_hash.begin(); it != polygon_hash.end(); )
            {
                const PolygonBatch& dcgroup = it->first;

                // TODO: keep track of state and only actually change GL state when needed

                const bool depth_enabled = to_integral(dcgroup.depth_test);
                const bool use_xray = depth_enabled && (dcgroup.xray_mode == XRayMode::On);

                // Face culling
                if (to_integral(dcgroup.cull_face))
                    glEnable(GL_CULL_FACE);
                else
                    glDisable(GL_CULL_FACE);

                static std::vector<void*> start;
                static std::vector<GLsizei> size;
                static std::vector<GLint> ofs;
                start.clear();
                size.clear();
                ofs.clear();

                while (it != polygon_hash.end() && dcgroup == it->first)
                {
                    start.push_back(BUFOFS(it->second.start * sizeof(GLuint)));
                    size.push_back(it->second.size);
                    ofs.push_back(it->second.ofs);

                    ++it;
                }
                int count = (int)start.size();

                auto draw_batch = [&](GLenum depth_func, GLboolean depth_mask, float alpha_multiplier)
                {
                    if (depth_enabled)
                    {
                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(depth_func);
                        glDepthMask(depth_mask);
                    }
                    else
                    {
                        glDisable(GL_DEPTH_TEST);
                        glDepthMask(depth_mask);
                    }

                    glUniform1f(alpha_loc, alpha_multiplier);
                    glMultiDrawElementsBaseVertex(dcgroup.topology, // GLenum mode
                        &size[0],         // const GLsizei *count
                        GL_UNSIGNED_INT,  // GLenum type
                        &start[0],        // const void * const *indices
                        count,            // GLsizei drawcount
                        &ofs[0]);         // const GLint *basevertex
                };

                draw_batch(last_depth_func, last_depth_mask, 1.0f);
                if (use_xray)
                    draw_batch(GL_GREATER, GL_FALSE, dcgroup.xray_alpha);
            }

            glBindVertexArray(0);
            glUseProgram(0);
            CheckAndThrowGLErrors();
        }
#endif

#if 1
        // Render thick line batches

        if (line_hash.size())
        {
            glUseProgram(thick_line_shader);
            glBindVertexArray(lines_VAO);
            glDisable(GL_CULL_FACE);

            glBindBuffer(GL_ARRAY_BUFFER, lines_VBO);
            glBufferData(GL_ARRAY_BUFFER,
                sizeof(ThickLineVertex) * (int)line_vertices.size(),
                line_vertices.data(),
                GL_STREAM_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lines_IBO);

            glUniformMatrix4fv(
                glGetUniformLocation(thick_line_shader, "PROJ_VIEW"),
                1,
                0,
                glm::value_ptr(PROJ_VIEW));

            GLint viewport[4] = {};
            glGetIntegerv(GL_VIEWPORT, viewport);
            glUniform2f(
                glGetUniformLocation(thick_line_shader, "viewport"),
                (float)viewport[2],
                (float)viewport[3]);
            const GLint alpha_loc = glGetUniformLocation(thick_line_shader, "alpha_multiplier");

            for (auto& it : line_hash)
            {
                int size = (int)it.second.size();
                if (!size) continue;

                const LineBatch& batch = it.first;
                const bool depth_enabled = to_integral(batch.depth_test);
                const bool use_xray = depth_enabled && (batch.xray_mode == XRayMode::On);

                glUniform1f(
                    glGetUniformLocation(thick_line_shader, "line_thickness"),
                    batch.style.thickness);
                glUniform1f(
                    glGetUniformLocation(thick_line_shader, "dash_period"),
                    batch.style.dash_period_px);
                glUniform1f(
                    glGetUniformLocation(thick_line_shader, "dash_ratio"),
                    batch.style.dash_ratio);
                glUniform1f(
                    glGetUniformLocation(thick_line_shader, "dash_offset"),
                    batch.style.dash_offset_px);

                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * size, &it.second[0], GL_STREAM_DRAW);

                auto draw_batch = [&](GLenum depth_func, GLboolean depth_mask, float alpha_multiplier)
                {
                    if (depth_enabled)
                    {
                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(depth_func);
                        glDepthMask(depth_mask);
                    }
                    else
                    {
                        glDisable(GL_DEPTH_TEST);
                        glDepthMask(depth_mask);
                    }

                    glUniform1f(alpha_loc, alpha_multiplier);
                    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, BUFOFS(0));
                };

                draw_batch(last_depth_func, last_depth_mask, 1.0f);
                if (use_xray)
                    draw_batch(GL_GREATER, GL_FALSE, batch.xray_alpha);
            }
            glBindVertexArray(0);
            glUseProgram(0);
            CheckAndThrowGLErrors();
        }
#endif

#if 1
        // Render simple line batches

        if (simple_line_hash.size())
        {
            glUseProgram(simple_line_shader);
            glBindVertexArray(simple_lines_VAO);

            glLineWidth(1);

            glBindBuffer(GL_ARRAY_BUFFER, simple_lines_VBO);
            glBufferData(GL_ARRAY_BUFFER,
                sizeof(LineVertex) * (int)simple_line_vertices.size(),
                simple_line_vertices.data(),
                GL_STREAM_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, simple_lines_IBO);

            glUniformMatrix4fv(
                glGetUniformLocation(simple_line_shader, "PROJ_VIEW"),
                1,
                0,
                glm::value_ptr(PROJ_VIEW));
            const GLint alpha_loc = glGetUniformLocation(simple_line_shader, "alpha_multiplier");

            // lines v3 (unorderded_map with vectors)

            for (auto& it : simple_line_hash)
            {
                int size = (int)it.second.size();
                if (!size) continue;

                const SimpleLineBatch& dc = it.first;
                const bool depth_enabled = to_integral(dc.depth_test);
                const bool use_xray = depth_enabled && (dc.xray_mode == XRayMode::On);

                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * size, &it.second[0], GL_STREAM_DRAW);

                auto draw_batch = [&](GLenum depth_func, GLboolean depth_mask, float alpha_multiplier)
                {
                    if (depth_enabled)
                    {
                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(depth_func);
                        glDepthMask(depth_mask);
                    }
                    else
                    {
                        glDisable(GL_DEPTH_TEST);
                        glDepthMask(depth_mask);
                    }

                    glUniform1f(alpha_loc, alpha_multiplier);
                    glDrawElements(dc.topology, size, GL_UNSIGNED_INT, BUFOFS(0));
                };

                draw_batch(last_depth_func, last_depth_mask, 1.0f);
                if (use_xray)
                    draw_batch(GL_GREATER, GL_FALSE, dc.xray_alpha);
            }
            glLineWidth(1);
            glBindVertexArray(0);
            glUseProgram(0);
            CheckAndThrowGLErrors();
        }
#endif

#if 1
        // Render point batches

        if (point_hash.size())
        {
            glUseProgram(point_shader);
            glBindVertexArray(point_vao);

            glBindBuffer(GL_ARRAY_BUFFER, point_vbo);

            glUniformMatrix4fv(
                glGetUniformLocation(point_shader, "PROJ_VIEW"),
                1,
                0,
                glm::value_ptr(PROJ_VIEW));
            const GLint alpha_loc = glGetUniformLocation(point_shader, "alpha_multiplier");

            for (auto& it : point_hash)
            {
                int size = (int)it.second.size();
                if (!size) continue;

                const PointBatch& dc = it.first;
                const bool depth_enabled = to_integral(dc.depth_test);
                const bool use_xray = depth_enabled && (dc.xray_mode == XRayMode::On);

                glBufferData(GL_ARRAY_BUFFER, sizeof(PointVertex) * size, &it.second[0], GL_STREAM_DRAW);
                glPointSize(dc.size);

                auto draw_batch = [&](GLenum depth_func, GLboolean depth_mask, float alpha_multiplier)
                {
                    if (depth_enabled)
                    {
                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(depth_func);
                        glDepthMask(depth_mask);
                    }
                    else
                    {
                        glDisable(GL_DEPTH_TEST);
                        glDepthMask(depth_mask);
                    }

                    glUniform1f(alpha_loc, alpha_multiplier);
                    glDrawArrays(GL_POINTS, 0, size);
                };

                draw_batch(last_depth_func, last_depth_mask, 1.0f);
                if (use_xray)
                    draw_batch(GL_GREATER, GL_FALSE, dc.xray_alpha);
            }
            CheckAndThrowGLErrors();
        }
#endif
        glUseProgram(0);


        // Restore state
        glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
        glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
        if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthFunc(last_depth_func);
        glDepthMask(last_depth_mask);
        //    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        glPointSize(last_poin_size);

#ifdef GL_POLYGON_MODE
        //    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
#endif
    }

    void ShapeRenderer::post_render()
    {
        polygon_vertices.clear();
        polygon_indices.clear();
        polygon_hash.clear();

        simple_line_vertices.clear();
        for (auto& it : simple_line_hash)
            it.second.clear();

        line_vertices.clear();
        for (auto& it : line_hash)
            it.second.clear();

        for (auto& it : point_hash)
            it.second.clear();
    }

    void DemoDraw(ShapeRendererPtr renderer)
    {
        const float row_base_y = 1.0f;
        const float row_tri_y = row_base_y + 0.0f;
        const float row_simple_y = row_base_y + 2.5f;
        const float row_thick_y = row_base_y + 5.0f;
        const float col_step = 2.5f;

        float x = 0.0f;
        const float col_aabb = x; x += col_step;
        const float col_lines = x; x += col_step;
        const float col_quad = x; x += col_step;
        const float col_cube = x; x += col_step;
        const float col_cone = x; x += col_step;
        const float col_cylinder = x; x += col_step;
        const float col_arrow = x; x += col_step;
        const float col_sphere = x; x += col_step;
        const float col_circle = x; x += col_step;
        const float col_helix = x; x += col_step;
        const float col_frustum = x; x += col_step;
        const float col_grid = x; x += col_step;

        const auto arrowdesc = ShapeRendering::ArrowDescriptor
        {
            .cone_fraction = 0.25f,
            .cone_radius = 0.2f,
            .cylinder_radius = 0.08f
        };
        const glm::mat4 frustum_view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 frustum_proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.3f, 3.0f);
        const glm::mat4 frustum_inv_proj_view = glm::inverse(frustum_proj * frustum_view);

        // Bottom row: triangulated
        {
            glm::vec3 points[4]{ {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f} };

            renderer->push_states(ShapeRendering::Color4u{ 0x8000ffff });
            renderer->push_states(glm_aux::TS(glm::vec3(col_quad, row_tri_y, 0.0f), glm::vec3(1.0f, 1.5f, 1.0f)));
            renderer->push_quad(points, glm_aux::vec3_001); // Quad (solid)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u{ 0x800000ff });
            renderer->push_states(glm_aux::TS(glm::vec3(col_cube, row_tri_y, 0.0f), glm::vec3(1.0f, 1.5f, 1.0f)));
            renderer->push_cube(); // Cube (solid)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Orange);
            renderer->push_states(glm_aux::T(glm::vec3(col_cone, row_tri_y, 0.0f)));
            renderer->push_cone(1.5f, 0.5f); // Cone (solid)
            renderer->pop_states<glm::mat4>();
            renderer->push_states(glm_aux::T(glm::vec3(col_cylinder, row_tri_y, 0.0f)));
            renderer->push_cylinder(1.5f, 0.5f); // Cylinder (solid)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::T(glm::vec3(col_arrow, row_tri_y, 0.0f)));
            renderer->push_arrow(glm_aux::vec3_000, glm::vec3(0.0f, 0.0f, 1.5f), arrowdesc); // Arrow (solid)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Purple);
            renderer->push_states(glm_aux::TS(glm::vec3(col_sphere, row_tri_y, 0.0f), glm::vec3(1.0f)));
            renderer->push_sphere(1.0f, 1.0f); // Sphere (solid)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Turquoise);
            renderer->push_states(glm_aux::T(glm::vec3(col_helix, row_tri_y, 0.0f)));
            renderer->push_helix_solid(glm_aux::vec3_000, glm::vec3(0.0f, 0.0f, 2.0f), 0.4f, 0.12f, 3.0f); // Helix (solid)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();
        }

        // Middle row: simple lines
        {
            renderer->push_states(ShapeRendering::Color4u::Cyan, glm_aux::T(glm::vec3(col_aabb, row_simple_y, 0.0f)));
            renderer->push_AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); // AABB (wireframe)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Cyan, glm_aux::T(glm::vec3(col_lines, row_simple_y, 0.0f)));
            renderer->push_line(glm::vec3(-0.35f, -0.35f, 0.0f), glm::vec3(0.35f, 0.35f, 0.0f)); // Line segment
            renderer->push_line(glm::vec3(-0.35f, 0.35f, 0.0f), glm::vec3(0.35f, -0.35f, 0.0f)); // Line segment
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::TS(glm::vec3(col_quad, row_simple_y, 0.0f), glm::vec3(1.0f, 1.5f, 1.0f)));
            renderer->push_quad_wireframe(); // Quad (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::TS(glm::vec3(col_cube, row_simple_y, 0.0f), glm::vec3(1.0f, 1.5f, 1.0f)));
            renderer->push_cube_wireframe(); // Cube (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Orange);
            renderer->push_states(glm_aux::T(glm::vec3(col_cone, row_simple_y, 0.0f)));
            renderer->push_cone_wireframe(1.5f, 0.5f); // Cone (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->push_states(glm_aux::T(glm::vec3(col_cylinder, row_simple_y, 0.0f)));
            renderer->push_cylinder_wireframe(1.5f, 0.5f); // Cylinder (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::T(glm::vec3(col_arrow, row_simple_y, 0.0f)));
            renderer->push_arrow_wireframe(glm_aux::vec3_000, glm::vec3(0.0f, 0.0f, 1.5f), arrowdesc); // Arrow (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Purple);
            renderer->push_states(glm_aux::TS(glm::vec3(col_sphere, row_simple_y, 0.0f), glm::vec3(1.0f)));
            renderer->push_sphere_wireframe(1.0f, 1.0f); // Sphere (wireframe)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Cyan);
            renderer->push_states(glm_aux::T(glm::vec3(col_circle, row_simple_y, 0.0f)));
            renderer->push_circle_ring<16>(); // Circle ring
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Turquoise);
            renderer->push_states(glm_aux::T(glm::vec3(col_helix, row_simple_y, 0.0f)));
            renderer->push_helix(glm_aux::vec3_000, glm::vec3(0.0f, 0.0f, 2.0f), 0.4f, 0.1f, 3.0f); // Helix (line)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Silver);
            renderer->push_states(glm_aux::TS(glm::vec3(col_frustum, row_simple_y, 0.0f), glm::vec3(0.6f)));
            renderer->push_frustum(frustum_inv_proj_view); // Frustum
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u{ 0xff808080 });
            renderer->push_states(glm_aux::T(glm::vec3(col_grid, row_simple_y, 0.0f)));
            renderer->push_grid(glm::vec3(0.0f), 2.0f, 5); // Grid
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();
        }

        // Top row: thick lines
        renderer->push_states(ShapeRendering::LineType::Thick, ShapeRendering::LineStyle{ 3.0f });
        {
            renderer->push_states(ShapeRendering::Color4u::Cyan, glm_aux::T(glm::vec3(col_aabb, row_thick_y, 0.0f)));
            renderer->push_AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); // AABB (wireframe)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Cyan, glm_aux::T(glm::vec3(col_lines, row_thick_y, 0.0f)));
            renderer->push_line(glm::vec3(-0.35f, -0.35f, 0.0f), glm::vec3(0.35f, 0.35f, 0.0f)); // Line segment
            renderer->push_line(glm::vec3(-0.35f, 0.35f, 0.0f), glm::vec3(0.35f, -0.35f, 0.0f)); // Line segment
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::LineStyle{ 3.0f, 10.0f, 0.5f, 0.0f });
            renderer->push_states(ShapeRendering::Color4u::Yellow, glm_aux::T(glm::vec3(col_lines, row_thick_y + 0.6f, 0.0f)));
            renderer->push_line(glm::vec3(-0.45f, 0.0f, 0.0f), glm::vec3(0.45f, 0.0f, 0.0f)); // Dashed line segment
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();
            renderer->pop_states<ShapeRendering::LineStyle>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::TS(glm::vec3(col_quad, row_thick_y, 0.0f), glm::vec3(1.0f, 1.5f, 1.0f)));
            renderer->push_quad_wireframe(); // Quad (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::TS(glm::vec3(col_cube, row_thick_y, 0.0f), glm::vec3(1.0f, 1.5f, 1.0f)));
            renderer->push_cube_wireframe(); // Cube (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Orange);
            renderer->push_states(glm_aux::T(glm::vec3(col_cone, row_thick_y, 0.0f)));
            renderer->push_cone_wireframe(1.5f, 0.5f); // Cone (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->push_states(glm_aux::T(glm::vec3(col_cylinder, row_thick_y, 0.0f)));
            renderer->push_cylinder_wireframe(1.5f, 0.5f); // Cylinder (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Yellow);
            renderer->push_states(glm_aux::T(glm::vec3(col_arrow, row_thick_y, 0.0f)));
            renderer->push_arrow_wireframe(glm_aux::vec3_000, glm::vec3(0.0f, 0.0f, 1.5f), arrowdesc); // Arrow (wireframe)
            renderer->pop_states<glm::mat4>();
            renderer->pop_states<ShapeRendering::Color4u>();

            renderer->push_states(ShapeRendering::Color4u::Purple);
            renderer->push_states(glm_aux::TS(glm::vec3(col_sphere, row_thick_y, 0.0f), glm::vec3(1.0f)));
            renderer->push_sphere_wireframe(1.0f, 1.0f); // Sphere (wireframe)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Cyan);
            renderer->push_states(glm_aux::T(glm::vec3(col_circle, row_thick_y, 0.0f)));
            renderer->push_circle_ring<16>(); // Circle ring
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Turquoise);
            renderer->push_states(glm_aux::T(glm::vec3(col_helix, row_thick_y, 0.0f)));
            renderer->push_helix(glm_aux::vec3_000, glm::vec3(0.0f, 0.0f, 2.0f), 0.4f, 0.1f, 3.0f); // Helix (line)
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::Color4u::Silver);
            renderer->push_states(glm_aux::TS(glm::vec3(col_frustum, row_thick_y, 0.0f), glm::vec3(0.6f)));
            renderer->push_frustum(frustum_inv_proj_view); // Frustum
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();

            renderer->push_states(ShapeRendering::LineStyle{ 2.0f });
            renderer->push_states(ShapeRendering::Color4u{ 0xffa0a0a0 });
            renderer->push_states(glm_aux::T(glm::vec3(col_grid, row_thick_y, 0.0f)));
            renderer->push_grid(glm::vec3(0.0f), 2.0f, 5); // Grid
            renderer->pop_states<ShapeRendering::Color4u, glm::mat4>();
            renderer->pop_states<ShapeRendering::LineStyle>();
        }
        renderer->pop_states<ShapeRendering::LineType, ShapeRendering::LineStyle>();
    }

} // namespace ShapeRendering
