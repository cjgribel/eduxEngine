// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef EENG_AABB_h
#define EENG_AABB_h

#include <glm/glm.hpp>
#include <float.h>

namespace eeng
{
    struct AABB
    {
        glm::vec3 min, max;

        AABB()
        {
            reset();
        }

        /// Reset to negative-space state
        inline void reset()
        {
            min[0] = min[1] = min[2] = std::numeric_limits<float>::max();
            max[0] = max[1] = max[2] = std::numeric_limits<float>::lowest();
        }

        /// Grow AABB to include point
        inline void grow(const glm::vec3& p)
        {
            min[0] = std::fminf(min[0], p[0]);
            max[0] = std::fmaxf(max[0], p[0]);

            min[1] = std::fminf(min[1], p[1]);
            max[1] = std::fmaxf(max[1], p[1]);

            min[2] = std::fminf(min[2], p[2]);
            max[2] = std::fmaxf(max[2], p[2]);
        }

        /// Grow AABB to include point
        inline void grow(const float p[3])
        {
            for (int i = 0; i < 3; i++)
            {
                min[i] = std::fminf(min[i], p[i]);
                max[i] = std::fmaxf(max[i], p[i]);
            }
        }

        /// Grow AABB to include another AABB
        inline void grow(const AABB& aabb)
        {
            grow(aabb.min);
            grow(aabb.max);
        }

        /// Mimimal bounding sphere that contains AABB
        inline glm::vec4 getBoundingSphere() const
        {
            glm::vec4 bs;

            for (int i = 0; i < 3; i++)
                bs[i] = (min[i] + max[i]) / 2;

            glm::vec3 rmax;
            for (int i = 0; i < 3; i++)
                rmax[i] = fmaxf(bs[i] - min[i], max[i] - bs[i]);
            bs.w = glm::length(rmax);

            return bs;
        }

        inline AABB operator+(const glm::vec3& v)
        {
            AABB aabb = *this;
            aabb.min += v;
            aabb.max += v;

            return aabb;
        }

        /// AABB resulting from this AABB being rotated and translated.
        /// Based on Ericsson, Real-time Collision Detection, page 86.
        inline AABB post_transform(const glm::vec3& T, const glm::mat3& R) const
        {
            AABB aabb;

            // For all three axes
            for (int i = 0; i < 3; i++)
            {
                aabb.min[i] = aabb.max[i] = T[i];
                for (int j = 0; j < 3; j++)
                {
                    float e = R[j][i] * min[j];
                    float f = R[j][i] * max[j];
                    if (e < f)
                    {
                        aabb.min[i] += e;
                        aabb.max[i] += f;
                    }
                    else
                    {
                        aabb.min[i] += f;
                        aabb.max[i] += e;
                    }
                }
            }

            return aabb;
        }

        inline AABB post_transform(const glm::mat4& M) const
        {
            AABB aabb;

            // For all three axes
            for (int i = 0; i < 3; i++) {
                // Start by adding in translation
                aabb.min[i] = aabb.max[i] = M[3][i];
                // Form extent by summing smaller and larger terms respectively
                for (int j = 0; j < 3; j++) {
                    float e = M[j][i] * min[j];
                    float f = M[j][i] * max[j];
                    if (e < f) {
                        aabb.min[i] += e;
                        aabb.max[i] += f;
                    }
                    else {
                        aabb.min[i] += f;
                        aabb.max[i] += e;
                    }
                }
            }
            return aabb;
        }

        operator bool()
        {
            return max.x > min.x && max.y > min.y && max.z > min.z;
        }

#define AABB_EPS FLT_EPSILON

        inline bool intersect(const AABB& aabb) const
        {
            if (max[0] < aabb.min[0] - AABB_EPS || min[0] > aabb.max[0] + AABB_EPS)
                return false;
            if (max[1] < aabb.min[1] - AABB_EPS || min[1] > aabb.max[1] + AABB_EPS)
                return false;
            if (max[2] < aabb.min[2] - AABB_EPS || min[2] > aabb.max[2] + AABB_EPS)
                return false;
            return true;
        }

    private:
        /// Split AABB at frac ∈ [0,1] in the dim:th dimension
        void split2(int dim, float frac, AABB& aabb_left, AABB& aabb_right)
        {
            aabb_left = *this;
            aabb_right = *this;
            float halfx = min[dim] + (max[dim] - min[dim]) * frac;

            aabb_left.max[dim] = halfx;
            aabb_right.min[dim] = halfx;
        }

    public:
        /// Split AABB into four in the xz-plane
        void split4_xz(AABB aabb[4])
        {
            AABB l, r;
            split2(0, 0.5f, l, r);               // split in x
            l.split2(2, 0.5f, aabb[0], aabb[1]); // split in z
            r.split2(2, 0.5f, aabb[2], aabb[3]); // split in z
        }
    };
} // namespace eeng
#endif
