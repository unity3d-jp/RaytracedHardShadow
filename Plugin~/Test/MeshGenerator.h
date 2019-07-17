#pragma once

#include "rths/rths.h"
using namespace rths;

void GenerateIcoSphereMesh(
    std::vector<int>& counts,
    std::vector<int>& indices,
    std::vector<float3>& points,
    float radius,
    int iteration);

void GenerateWaveMesh(
    std::vector<int>& counts,
    std::vector<int>& indices,
    std::vector<float3> &points,
    float size, float height,
    const int resolution,
    float angle,
    bool triangulate = false);

