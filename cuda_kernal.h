#pragma once
#include <stdio.h>
#include <iostream>
#include "cuda_runtime.h"
#include "linear_quadtree.h"

struct Body {
    float2 position;
    float mass;
};
struct Body_Info {
    float2 velocity;
    float accel;
    float previous_accel;
};

int init_cuda_renderer(int render_height, int render_width, int channels);
void render_to_rt(uint8_t* pixels, bool blur);

void clear_rt();
Body* init_nbody(int count, int seed, float minMass, float maxMass, float2 spaceLow, float2 spaceHigh, bool spiral);
void tick(float deltaTime, float colorScale, float renderScale, float2 renderOffset);
Body* fetch_updated_bodies();
void push_bth_gpu(Node* tree, int count);
void init_bth_gpu(int count);