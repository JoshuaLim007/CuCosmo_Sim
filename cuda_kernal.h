#pragma once
#include <stdio.h>
#include <iostream>
#include "cuda_runtime.h"

int init_cuda_renderer(int render_height, int render_width, int channels);
void render_to_rt(uint8_t* pixels, bool blur);

void clear_rt();
void init_nbody(int count, int seed, float minMass, float maxMass, float2 spaceLow, float2 spaceHigh, bool spiral);
void tick(float deltaTime, float colorScale, float renderScale, float2 renderOffset);