// Copyright 2019, 2020 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Foundation/polynomial.hpp"
#include "TTauri/Foundation/vec.hpp"
#include "TTauri/Foundation/mat.hpp"
#include <array>

namespace TTauri {

// B(t)=(P_{2}-P_{1})t+P_{1}
template<typename T>
inline std::array<T,2> bezierToPolynomial(T P1, T P2) noexcept
{
    return {
        P2 - P1,
        P1
    };
}

// B(t)=(P_{1}-2C+P_{2})t^{2}+2(C-P_{1})t+P_{1}
template<typename T>
inline std::array<T,3> bezierToPolynomial(T P1, T C, T P2) noexcept
{
    return {
        P1 - C * 2.0f + P2,
        (C - P1) * 2.0f,
        P1
    };
}

// B(t)=(-P_{1}+3C_{1}-3C_{2}+P_{2})t^{3}+(3P_{1}-6_{1}+3C_{2})t^{2}+(-3P_{1}+3C_{1})t+P_{1}
template<typename T>
inline std::array<T,4> bezierToPolynomial(T P1, T C1, T C2, T P2) noexcept
{
    return {
        -P1 + C1 * 3.0f - C2 * 3.0f + P2,
        P1 * 3.0f - C1 * 6.0f + C2 * 3.0f,
        P1 * -3.0f + C1 * 3.0f,
        P1
    };
}

inline vec bezierPointAt(vec P1, vec P2, float t) noexcept {
    ttauri_assume(P1.w() == 1.0f && P2.w() == 1.0f);

    let [a, b] = bezierToPolynomial(P1, P2);
    return a*t + b;
}

inline vec bezierPointAt(vec P1, vec C, vec P2, float t) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C.w() == 1.0f && P2.w() == 1.0f);

    let [a, b, c] = bezierToPolynomial(P1, C, P2);
    return a*t*t + b*t + c;
}

inline vec bezierPointAt(vec P1, vec C1, vec C2, vec P2, float t) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C1.w() == 1.0f && C2.w() && P2.w() == 1.0f);

    let [a, b, c, d] = bezierToPolynomial(P1, C1, C2, P2);
    return a*t*t*t + b*t*t + c*t + d;
}

inline vec bezierTangentAt(vec P1, vec P2, float t) noexcept
{
    ttauri_assume(P1.w() == 1.0f && P2.w() == 1.0f);

    return P2 - P1;
}

inline vec bezierTangentAt(vec P1, vec C, vec P2, float t) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C.w() == 1.0f && P2.w() == 1.0f);

    return 2.0f * t * (P2 - 2.0f * C + P1) + 2.0f * (C - P1);
} 

inline vec bezierTangentAt(vec P1, vec C1, vec C2, vec P2, float t) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C1.w() == 1.0f && C2.w() && P2.w() == 1.0f);

    return 
        3.0f * t * t * (P2 - 3.0f * C2 + 3.0f * C1 - P1) +
        6.0f * t * (C2 - 2.0f * C1 + P1) +
        3.0f * (C1 - P1);
}

inline results<float,1> bezierFindT(float P1, float P2, float x) noexcept
{
    let [a, b] = bezierToPolynomial(P1, P2);
    return solvePolynomial(a, b - x);
}

inline results<float,2> bezierFindT(float P1, float C, float P2, float x) noexcept
{
    let [a, b, c] = bezierToPolynomial(P1, C, P2);
    return solvePolynomial(a, b, c - x);
}

inline results<float,3> bezierFindT(float P1, float C1, float C2, float P2, float x) noexcept
{
    let [a, b, c, d] = bezierToPolynomial(P1, C1, C2, P2);
    return solvePolynomial(a, b, c, d - x);
}

/** Find t on the line P1->P2 which is closest to P.
 * Used for finding the shortest distance from a point to a curve.
 * The shortest vector from a curve to a point is a normal.
 */
inline results<float,1> bezierFindTForNormalsIntersectingPoint(vec P1, vec P2, vec P) noexcept
{
    ttauri_assume(P1.w() == 1.0f && P2.w() == 1.0f && P.w() == 1.0f);

    auto t_above = dot(P - P1, P2 - P1);
    auto t_below = dot(P2 - P1, P2 - P1);
    if (ttauri_unlikely(t_below == 0.0)) {
        return {};
    } else {
        return {t_above / t_below};
    }
}

/** Find t on the curve P1->C->P2 which is closest to P.
* Used for finding the shortest distance from a point to a curve.
* The shortest vector from a curve to a point is a normal.
*/
inline results<float,3> bezierFindTForNormalsIntersectingPoint(vec P1, vec C, vec P2, vec P) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C.w() == 1.0f && P2.w() == 1.0f && P.w() == 1.0f);

    auto p = P - P1;
    auto p1 = C - P1;
    auto p2 = P2 - (2.0f * C) + P1;

    auto a = dot(p2, p2);
    auto b = 3.0f * dot(p1, p2);
    auto c = dot(2.0f * p1, p1) - dot(p2, p);
    auto d = -dot(p1, p);
    return solvePolynomial(a, b, c, d);
}

/*! Find x for y on a bezier curve.
 * In a contour, multiple bezier curves are attached to each other
 * on the anchor point. We don't want duplicate results when
 * passing `y` that is at the same height as an anchor point.
 * So we compare with less than to the end-anchor point to remove
 * it from the result.
 */
inline results<float,1> bezierFindX(vec P1, vec P2, float y) noexcept
{
    ttauri_assume(P1.w() == 1.0f && P2.w() == 1.0f);

    if (y < std::min({P1.y(), P2.y()}) || y > std::max({P1.y(), P2.y()})) {
        return {};
    }

    results<float,1> r;
    for (let t: bezierFindT(P1.y(), P2.y(), y)) {
        if (t >= 0.0f && t < 1.0f) {
            r.add(bezierPointAt(P1, P2, t).x());
        }
    }

    return r;
}

/*! Find x for y on a bezier curve.
* In a contour, multiple bezier curves are attached to each other
* on the anchor point. We don't want duplicate results when
* passing `y` that is at the same height as an anchor point.
* So we compare with less than to the end-anchor point to remove
* it from the result.
*/
inline results<float,2> bezierFindX(vec P1, vec C, vec P2, float y) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C.w() == 1.0f && P2.w() == 1.0f);

    results<float,2> r{};

    if (y < std::min({P1.y(), C.y(), P2.y()}) || y > std::max({P1.y(), C.y(), P2.y()})) {
        return r;
    }

    for (let t: bezierFindT(P1.y(), C.y(), P2.y(), y)) {
        if (t >= 0.0f && t <= 1.0f) {
            r.add(bezierPointAt(P1, C, P2, t).x());
        }
    }

    return r;
}

/*! Find x for y on a bezier curve.
* In a contour, multiple bezier curves are attached to each other
* on the anchor point. We don't want duplicate results when
* passing `y` that is at the same height as an anchor point.
* So we compare with less than to the end-anchor point to remove
* it from the result.
*/
inline results<float,3> bezierFindX(vec P1, vec C1, vec C2, vec P2, float y) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C1.w() == 1.0f && C2.w() == 1.0f && P2.w() == 1.0f);

    results<float,3> r{};

    if (y < std::min({ P1.y(), C1.y(), C2.y(), P2.y() }) || y > std::max({ P1.y(), C1.y(), C2.y(), P2.y() })) {
        return r;
    }

    for (let t: bezierFindT(P1.y(), C1.y(), C2.y(), P2.y(), y)) {
        if (t >= 0.0f && t <= 1.0f) {
            r.add(bezierPointAt(P1, C1, C2, P2, t).x());
        }
    }

    return r;
}

/*! Return the flatness of a curve.
* \return 1.0 when completely flat, < 1.0 when curved.
*/
inline float bezierFlatness(vec P1, vec P2) noexcept
{
    ttauri_assume(P1.w() == 1.0f && P2.w() == 1.0f);

    return 1.0f;
}

/*! Return the flatness of a curve.
* \return 1.0 when completely flat, < 1.0 when curved.
*/

inline float bezierFlatness(vec P1, vec C, vec P2) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C.w() == 1.0f && P2.w() == 1.0f);

    let P1P2 = length(P2 - P1);
    if (P1P2 == 0.0f) {
        return 1.0;
    }

    let P1C1 = length(C - P1);
    let C1P2 = length(P2 - C);
    return P1P2 / (P1C1 + C1P2);
}

/*! Return the flatness of a curve.
* \return 1.0 when completely flat, < 1.0 when curved.
*/

inline float bezierFlatness(vec P1, vec C1, vec C2, vec P2) noexcept
{
    ttauri_assume(P1.w() == 1.0f && C1.w() == 1.0f && C2.w() == 1.0f && P2.w() == 1.0f);

    let P1P2 = length(P2 - P1);
    if (P1P2 == 0.0f) {
        return 1.0;
    }

    let P1C1 = length(C1 - P1);
    let C1C2 = length(C2 - C1);
    let C2P2 = length(P2 - C2);
    return P1P2 / (P1C1 + C1C2 + C2P2);
}

inline std::pair<vec, vec> parrallelLine(vec P1, vec P2, float distance) noexcept
{
    ttauri_assume(P1.w() == 1.0f && P2.w() == 1.0f);

    let v = P2 - P1;
    let n = normal(v);
    return {
        P1 + n * distance,
        P2 + n * distance
    };
}

/*! Find the intersect points between two line segments.
*/
inline std::optional<vec> getIntersectionPoint(vec A1, vec A2, vec B1, vec B2) noexcept
{
    ttauri_assume(A1.w() == 1.0f && A2.w() == 1.0f && B1.w() == 1.0f && B2.w() == 1.0f);

    // convert points to vectors.
    let p = A1;
    let r = A2 - A1;
    let q = B1;
    let s = B2 - B1;

    // find t and u in:
    // p + t*r == q + us
    let crossRS = viktor_cross(r, s);
    if (crossRS == 0.0f) {
        // Parallel, other non, or a range of points intersect.
        return {};

    } else {
        let q_min_p = q - p;
        let t = viktor_cross(q_min_p, s) / crossRS;
        let u = viktor_cross(q_min_p, r) / crossRS;

        if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
            return bezierPointAt(A1, A2, t);
        } else {
            // The lines intersect outside of one or both of the segments.
            return {};
        }
    }
}

/*! Find the intersect points between two line segments.
*/
inline std::optional<vec> getExtrapolatedIntersectionPoint(vec A1, vec A2, vec B1, vec B2) noexcept
{
    ttauri_assume(A1.w() == 1.0f && A2.w() == 1.0f && B1.w() == 1.0f && B2.w() == 1.0f);

    // convert points to vectors.
    let p = A1;
    let r = A2 - A1;
    let q = B1;
    let s = B2 - B1;

    // find t and u in:
    // p + t*r == q + us
    let crossRS = viktor_cross(r, s);
    if (crossRS == 0.0f) {
        // Parallel, other non, or a range of points intersect.
        return {};

    } else {
        let q_min_p = q - p;
        let t = viktor_cross(q_min_p, s) / crossRS;

        return bezierPointAt(A1, A2, t);
    }
}

}
