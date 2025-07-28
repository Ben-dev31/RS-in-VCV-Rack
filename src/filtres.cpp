#include <vector>
#include <cmath>
#include "filtres.hpp"

// Diode simple
float diode(float x, float th) {
    return (x >= th) ? (x - th) : 0.f;
}

// Diode symétrique (type "rubber")
float rubber(float x, float th) {
    if (x >= th) return x - th;
    else if (x <= -th) return x + th;
    return 0.f;
}

// Filtre bistable
float bistableFilter(float xi, float si, float ni, float dt, float tau, float Xb) {
    return xi + dt / tau * (xi - 1.f / (Xb * Xb) * xi * xi * xi + si + ni);
}

// Filtre multi-puits
float multiWellFilter(float xi, float si, float ni, float dt, float tau, int N, float Xb) {
    float dx = si + ni - multi_well_grad(xi, N, Xb);
    return xi + dt / tau * dx;
}

// Potentiel bistable
float bistablePotential(float x, float th) {
    return -0.5f * x * x + (1.f / (4.f * th * th)) * x * x * x * x;
}

// Potentiel multi-puits
float multi_well_potential(float x, int N, float Xb) {
    float L = 2.0f * Xb;
    std::vector<float> x0_list(N);
    for (int i = 0; i < N; ++i)
        x0_list[i] = (i - (N - 1) / 2.0f) * L;

    for (int i = 0; i < N; ++i) {
        float x0 = x0_list[i];
        float dx = x - x0;
        // Bord gauche
        if (i == 0 && x <= x0 + Xb)
            return -0.5f * dx * dx + (1.f / (4.f * Xb * Xb)) * std::pow(dx, 4);
        // Bord droit
        else if (i == N - 1 && x >= x0 - Xb)
            return -0.5f * dx * dx + (1.f / (4.f * Xb * Xb)) * std::pow(dx, 4);
        // Puits centraux
        else if (i > 0 && i < N - 1 && x >= x0 - Xb && x <= x0 + Xb)
            return -0.5f * dx * dx + (1.f / (4.f * Xb * Xb)) * std::pow(dx, 4);
    }
    // En dehors de tout puits
    return 0.f;
}

// Gradient du potentiel multi-puits
float multi_well_grad(float x, int N, float Xb) {
    float L = 2.0f * Xb;
    std::vector<float> x0_list(N);
    for (int i = 0; i < N; ++i)
        x0_list[i] = (i - (N - 1) / 2.0f) * L;

    for (int i = 0; i < N; ++i) {
        float x0 = x0_list[i];
        float dx = x - x0;
        // Bord gauche
        if (i == 0 && x <= x0 + Xb)
            return -dx + (1.f / (Xb * Xb)) * std::pow(dx, 3);
        // Bord droit
        else if (i == N - 1 && x >= x0 - Xb)
            return -dx + (1.f / (Xb * Xb)) * std::pow(dx, 3);
        // Puits centraux
        else if (i > 0 && i < N - 1 && x >= x0 - Xb && x <= x0 + Xb)
            return -dx + (1.f / (Xb * Xb)) * std::pow(dx, 3);
    }
    // En dehors de tout puits
    return 0.f;
}
