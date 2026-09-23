#include "physics/GravityEngine.hpp"
#include "Vec2d.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void GravityEngine::UpdateOrbits(std::vector<CelestialBody>& bodies, double dt) {
    int n = bodies.size();
    
    std::vector<Vec2d> accelerations(n, Vec2d{0.0, 0.0});
    std::vector<Vec2d> accNew(n, Vec2d{0.0, 0.0});

    for (int i = 0; i < n; ++i) {
        accelerations[i] = ComputeAcceleration(bodies[i].physics.position, bodies);
    }

    for (int i = 0; i < n; ++i) {
        if (bodies[i].isStatic) continue;
        bodies[i].physics.position += bodies[i].physics.velocity * dt + accelerations[i] * (0.5 * dt * dt);
    }

    for (int i = 0; i < n; ++i) {
        accNew[i] = ComputeAcceleration(bodies[i].physics.position, bodies);
    }

    for (int i = 0; i < n; ++i) {
        if (bodies[i].isStatic) continue;
        bodies[i].physics.velocity += (accelerations[i] + accNew[i]) * (0.5 * dt);
    }
}

void GravityEngine::UpdateAll(std::vector<CelestialBody>& bodies, Spaceship& ship, double dt) {
    int n = bodies.size();
    std::vector<Vec2d> accelerations(n, Vec2d{0.0, 0.0});
    std::vector<Vec2d> accNew(n, Vec2d{0.0, 0.0});

    // a0 calculado en el MISMO instante para bodies y nave
    Vec2d shipAcc = ComputeAcceleration(ship.physics.position, bodies);
    for (int i = 0; i < n; ++i) {
        accelerations[i] = ComputeAcceleration(bodies[i].physics.position, bodies);
    }

    // mover todos (bodies no estáticos + nave)
    for (int i = 0; i < n; ++i) {
        if (bodies[i].isStatic) continue;
        bodies[i].physics.position += bodies[i].physics.velocity * dt + accelerations[i] * (0.5 * dt * dt);
    }
    ship.physics.position += ship.physics.velocity * dt + shipAcc * (0.5 * dt * dt);

    // a1 en el mismo instante tras mover
    Vec2d shipAccNew = ComputeAcceleration(ship.physics.position, bodies);
    for (int i = 0; i < n; ++i) {
        accNew[i] = ComputeAcceleration(bodies[i].physics.position, bodies);
    }

    // actualizar velocidades de todos
    for (int i = 0; i < n; ++i) {
        if (bodies[i].isStatic) continue;
        bodies[i].physics.velocity += (accelerations[i] + accNew[i]) * (0.5 * dt);
    }
    ship.physics.velocity += (shipAcc + shipAccNew) * (0.5 * dt);
}

void GravityEngine::UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, double dt) {
    Vec2d acceleration = ComputeAcceleration(ship.physics.position, bodies);
    ship.physics.position += ship.physics.velocity * dt + acceleration * (0.5 * dt * dt);

    Vec2d accNew = ComputeAcceleration(ship.physics.position, bodies);
    ship.physics.velocity += (acceleration + accNew) * (0.5 * dt);
}

std::vector<Vec2d> GravityEngine::PredictConic(
    const Vec2d& rRel, const Vec2d& vRel, double mu, const Vec2d& primaryPos, int numPoints
) {
    std::vector<Vec2d> out;
    const double r  = rRel.length();
    const double v  = vRel.length();
    const double hS = rRel.x * vRel.y - rRel.y * vRel.x;   // signado
    const double h  = std::abs(hS);
    const double eps = 0.5 * v * v - mu / r;
    const double p = (h * h) / mu;
    double eDisc = 1.0 + 2.0 * eps * h * h / (mu * mu);
    if (eDisc < 0.0) eDisc = 0.0;            // ruido numérico en órbitas casi circulares: sin NaN
    const double e = std::sqrt(eDisc);

    if (h < 1e-12 || mu <= 0) return out;

    Vec2d rHat = rRel * (1.0 / r);
    Vec2d tHat = { -rHat.y, rHat.x };

    double cosNu = 0.0, sinNu = 1.0;
    if (e > 1e-3) {   // periapsis bien definida; e<1e-3 la órbita es visualmente circular
        cosNu = (p / r - 1.0) / e;
        sinNu = hS * (rRel.x*vRel.x + rRel.y*vRel.y) / (mu * r * e);
    } else {
        cosNu = 1.0; sinNu = 0.0;
    }
    Vec2d pHat = { rHat.x*cosNu - tHat.x*sinNu, rHat.y*cosNu - tHat.y*sinNu };
    Vec2d qHat = { -pHat.y, pHat.x };              // perpendicular a pHat (dirección de movimiento)

    auto pushPoint = [&](double phi) {
        double rr = p / (1.0 + e * std::cos(phi));
        Vec2d u = { pHat.x*cos(phi) + qHat.x*sin(phi),
                    pHat.y*cos(phi) + qHat.y*sin(phi) };
        out.push_back(primaryPos + u * rr);
    };

    if (e < 1.0) {
        for (int i = 0; i < numPoints; ++i) pushPoint(2.0 * M_PI * i / numPoints);
        if (out.size() > 1) out.push_back(out.front());   // cerrar la elipse
    } else {
        // Hipérbola: arco abierto, |φ| ≤ φmax (asíntota) con margen
        double phiEnd = std::acos(-1.0 / e) - 0.05;
        int half = std::max(16, numPoints / 2);
        for (int i = -half; i <= half; ++i) pushPoint(phiEnd * i / half);
    }
    return out;
}


Vec2d GravityEngine::ComputeAcceleration(const Vec2d& pos, const std::vector<CelestialBody>& bodies) {
    static constexpr double EPSILON2 = 25.0;

    double ax = 0.0;
    double ay = 0.0;

    for (const auto& body : bodies) {
        double rx = body.physics.position.x - pos.x;
        double ry = body.physics.position.y - pos.y;

        double d = rx * rx + ry * ry + EPSILON2;
        double invDenom = 1.0 / (d * std::sqrt(d));

        ax += G * body.physics.mass * rx * invDenom;
        ay += G * body.physics.mass * ry * invDenom;
    }

    return { ax, ay };
}

const CelestialBody* GravityEngine::GetDominantBody(const Vec2d& pos, const std::vector<CelestialBody>& bodies) {
    const CelestialBody* dominant = &bodies[0];
    double minDistance = std::numeric_limits<double>::max();

    for (const auto& body : bodies) {
        double distance = (body.physics.position - pos).length();
        if (distance < body.soiRadius) {
            minDistance = distance;
            dominant = const_cast<CelestialBody*>(&body);
        }
    }

    return dominant;
}

const OrbitInfo GravityEngine::ComputeOrbitInfo(const Vec2d& r_rel, const Vec2d& v_rel, double mu) {
    OrbitInfo info;

    double r = r_rel.length();
    double v = v_rel.length();
    double eps = 0.5 * v * v - mu / r;
    double h = std::abs(r_rel.x * v_rel.y - r_rel.y * v_rel.x);

    info.mu = mu;
    info.r = r;
    info.v = v;
    info.eps = eps;
    info.h = h;

    if (eps < 0) {
        info.hyperbolic = false;
        info.a = -mu / (2.0 * eps);
        double eDisc = 1.0 + (2.0 * eps * h * h) / (mu * mu);
        if (eDisc < 0.0) eDisc = 0.0;                 // mismo ruido numérico que PredictConic
        info.e = std::sqrt(eDisc);
        info.rp = info.a * (1.0 - info.e);
        info.ra = info.a * (1.0 + info.e);
        info.T = 2.0 * M_PI * std::sqrt(info.a * info.a * info.a / mu);
    } else if (eps == 0) {
        info.hyperbolic = false;
        info.a = std::numeric_limits<double>::infinity();
        info.e = 1.0;
        info.rp = h * h / mu;
        info.ra = std::numeric_limits<double>::infinity();
        info.T = std::numeric_limits<double>::infinity();
    } else {
        info.hyperbolic = true;
        info.a = -mu / (2.0 * eps);
        info.e = std::sqrt(1.0 + (2.0 * eps * h * h) / (mu * mu));
        info.rp = info.a * (1.0 - info.e);
        info.ra = std::numeric_limits<double>::infinity();
        info.T = std::numeric_limits<double>::infinity();
    }

    return info;
}