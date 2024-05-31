#include "cuda_kernal.h"
#include "device_launch_parameters.h"
#include "math_functions.h"
#include "helper_math.h"

// Error checking macro
#define CUDA_CHECK(call)                                                    \
    do {                                                                    \
        cudaError_t err = call;                                             \
        if (err != cudaSuccess) {                                           \
            std::cerr << "CUDA error in " << __FILE__ << "(" << __LINE__ << "): " \
                      << cudaGetErrorString(err) << std::endl;              \
            std::exit(EXIT_FAILURE);                                        \
        }                                                                   \
    } while (0)


Body* d_Bodies;
Body_Info* d_Bodies_ext;

inline float flerp(float min, float max, float t) {
    return min * (1 - t) + max * (t);
}
inline float frandom(float min, float max) {
    float frand = (float)rand() / (float)RAND_MAX;
    return flerp(min, max, frand);
}

__device__ uint8_t* d_render_texture;          //device texture ptr
__constant__ int2 d_texture_size;           //device texture size in pixels
__constant__ int d_channels;                //device channels
const int MAX_CHANNELS = 4;

uint8_t* h_render_texture;                 //texture ptr
int texture_mem_size;                   //texture size in bytes
int2 texture_size;                      //texture size in pixels

__device__ int2 get_pixel_coord() {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    return make_int2(x, y);
}

__device__ void set_pixel(uint8_t* texture, float4 color, int2 coord) {
    coord.y -= d_texture_size.y;
    coord.y *= -1;

    int ptr_loc = (coord.x + coord.y * d_texture_size.x) * MAX_CHANNELS;
    
    int red_offset = ptr_loc + min(0, d_channels - 1);
    int green_offset = ptr_loc + min(1, d_channels - 1);
    int blue_offset = ptr_loc + min(2, d_channels - 1);
    int alpha_offset = ptr_loc + min(3, d_channels - 1);
    color.x = min(color.x, 1.f);
    color.y = min(color.y, 1.f);
    color.z = min(color.z, 1.f);
    color.w = min(color.w, 1.f);
    *(texture + alpha_offset) = (uint8_t)(color.w * 255);
    *(texture + blue_offset) = (uint8_t)(color.z * 255);
    *(texture + green_offset) = (uint8_t)(color.y * 255);
    *(texture + red_offset) = (uint8_t)(color.x * 255);
}

__device__ float4 get_pixel(uint8_t* texture, int2 coord) {
    coord.y -= d_texture_size.y;
    coord.y *= -1;

    int ptr_loc = (coord.x + coord.y * d_texture_size.x) * MAX_CHANNELS;

    int red_offset = ptr_loc + min(0, d_channels - 1);
    int green_offset = ptr_loc + min(1, d_channels - 1);
    int blue_offset = ptr_loc + min(2, d_channels - 1);
    int alpha_offset = ptr_loc + min(3, d_channels - 1);
    float4 color = make_float4(0, 0, 0, 0);

    if (d_channels >= 4)
        color.w = (*(texture + alpha_offset)) / 255.0f;
    if (d_channels >= 3)
        color.z = (*(texture + blue_offset)) / 255.0f;
    if (d_channels >= 2)
        color.y = (*(texture + green_offset)) / 255.0f;
    if (d_channels >= 1)
        color.x = ( *(texture + red_offset)   ) / 255.0f;

    return color;
}

__device__ float lum(float4 color) {
    return (0.299 * color.x + 0.587 * color.y + 0.114 * color.z);
}

__global__ void render() {
    int2 coord = get_pixel_coord();
    
    float4 finalColor = make_float4(0, 0, 0, 0);
    float weight = 0;
    int size = 2;
    for (int i = -size; i <= size; i++)
    {
        for (int j = -size; j <= size; j++)
        {
            int2 c = coord + make_int2(i, j);
            if (c.x < 0 || c.x >= d_texture_size.x || c.y < 0 || c.y >= d_texture_size.y) {
                continue;
            }

            float w = sqrt((1 - abs(i) / (float)size) * (1 - abs(j) / (float)size));
            weight += w;
            finalColor += get_pixel(d_render_texture, c) * w;
        }
    }
    if (weight != 0) {
        finalColor /= (float)weight;
        finalColor *= 2.0;
        set_pixel(d_render_texture, finalColor, coord);
    }
    else {
        finalColor += get_pixel(d_render_texture, coord);
        set_pixel(d_render_texture, finalColor, coord);
    }
}

void clear_rt() {
    cudaMemset(h_render_texture, 0, texture_mem_size);
}

void render_to_rt(uint8_t* pixels, bool blur) {
    if (blur) {
        CUDA_CHECK(cudaMemcpyToSymbol(d_render_texture, &h_render_texture, sizeof(void*)));
        dim3 threads_per_block = dim3(8, 8, 1);
        dim3 blocks_per_grid = dim3((float)texture_size.x / threads_per_block.x, (float)texture_size.y / threads_per_block.y, 1);
        render << < blocks_per_grid, threads_per_block >> > ();
    }
    CUDA_CHECK(cudaMemcpy(pixels, h_render_texture, texture_mem_size, cudaMemcpyDeviceToHost));
}

int init_cuda_renderer(int render_width, int render_height, int channels) {
    texture_mem_size = render_height * render_width * sizeof(uint8_t) * MAX_CHANNELS;
    texture_size = make_int2(render_width, render_height);

    CUDA_CHECK(cudaMalloc(&h_render_texture, texture_mem_size));
    CUDA_CHECK(cudaMemcpyToSymbol(d_texture_size, &texture_size, sizeof(float2), 0, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpyToSymbol(d_channels, &channels, sizeof(int), 0, cudaMemcpyHostToDevice));
	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


__constant__ constexpr float GRAV_CONST = 1.0f;
__constant__ constexpr float DIST_SCALE = 0.5f;
__constant__ constexpr float MASS_SCALE = 1.0f;
Body* h_Bodies;
int body_count;

__global__ void calculate_nbody(Body* bodies, Body_Info* bodies_ext, int lowBound, int upBound, int count, float delta) {

    int midx = threadIdx.x + blockDim.x * blockIdx.x;
    if (midx >= count) {
        return;
    }

    Body me = bodies[midx];
    Body_Info me_ext = bodies_ext[midx];

    float2 curVel = me_ext.velocity;
    float2 total_force;

    for (size_t i = lowBound; i < upBound; i++)
    {
        if (i == midx) {
            continue;
        }

        Body t = bodies[i];
        float2 dir = t.position - me.position;
        float dist = length(dir) * DIST_SCALE;
        if (dist == 0.0) {
            continue;
        }


        if (dist <= 1.0) {
            float2 force = GRAV_CONST * me.mass * MASS_SCALE * t.mass * MASS_SCALE * normalize(dir) * powf(dist, 1.0f);
            total_force += force;
            curVel += force * delta / abs(me.mass * MASS_SCALE);
        }
        else {
            float dist2 = dist * dist;
            float2 force = GRAV_CONST * me.mass * MASS_SCALE * t.mass * MASS_SCALE * normalize(dir) / dist2;
            total_force += force;
            curVel += force * delta / abs(me.mass * MASS_SCALE);
        }
    }
    me.position += curVel * delta;

    //needed to avoid flickering
    float total_accel_length = length(total_force) / abs(me.mass * MASS_SCALE);
    float f0 = 1 / (1 + total_accel_length);
    float f1 = 1 / (1 + me_ext.previous_accel);
    if (f0 != 0 || f1 != 0) {
        me_ext.accel = max((total_accel_length * f0 + me_ext.previous_accel * f1) / (f0 + f1), 0.0f);
    }
    me_ext.previous_accel = max(total_accel_length, 0.0f);
    me_ext.velocity = curVel;

    bodies[midx] = me;
    bodies_ext[midx] = me_ext;
}

__device__ float4 get_color_from_value(float value, float scale) {

    float4 red = make_float4(1, 0, 0, 0);
    float4 yellow = make_float4(1, 1, 0, 0);
    float4 green = make_float4(0, 1, 0, 0);
    float4 cyan = make_float4(0, 1, 1, 0);
    float4 blue = make_float4(0, 0, 1, 0);
    float4 black = make_float4(0, 0, 0.5, 0);

    float4 finalCol = red;

    float scaler = scale;
    float inv_ten = 1.0 / (10.0f * scaler);

    if (value < 10 * scaler) {
        finalCol = (lerp(black, blue, value * inv_ten));
    }
    else if (value < 20 * scaler) {
        finalCol = (lerp(blue, cyan, (value - 10.0f * scaler) * inv_ten));
    }
    else if (value < 30 * scaler) {
        finalCol = (lerp(cyan, green, (value - 20.0f * scaler) * inv_ten));
    }
    else if (value < 40 * scaler) {
        finalCol = (lerp(green, yellow, (value - 30.0f * scaler) * inv_ten));
    }
    else if (value < 50 * scaler) {
        finalCol = (lerp(yellow, red, (value - 40.0f * scaler) * inv_ten));
    }
    else {
        finalCol = red;
    }

    return finalCol;
}

__global__ void draw_bodies(Body* bodies, Body_Info* bodies_ext, int count, uint8_t* outTexture, float colorScale, float renderScale, float2 renderOffset) {

    int midx = threadIdx.x + blockDim.x * blockIdx.x;
    if (midx >= count) {
        return;
    }
    Body me = bodies[midx];
    Body_Info me_ext = bodies_ext[midx];

    float2 mid_f = make_float2(d_texture_size.x, d_texture_size.y) * 0.5;
    float2 coord_f = make_float2(me.position.x + renderOffset.x, me.position.y + renderOffset.y);

    coord_f -= mid_f;
    coord_f *= renderScale;
    coord_f += mid_f;

    int2 coord = make_int2(floor(coord_f.x), floor(coord_f.y));
    if (coord.x > 0 && coord.x < d_texture_size.x
        && coord.y > 0 && coord.y < d_texture_size.y) {
        float val = me_ext.accel;
        float4 color = get_color_from_value(val, colorScale);
        set_pixel(outTexture, color, coord);
    }
}

float2 get_random_direction() {
    float x;
    float y;
    float2 dir;

    do
    {
        x = frandom(-1, 1);
        y = frandom(-1, 1);
        dir = make_float2(x, y);
    } while (length(dir) > 1.0);

    return dir;
}

Body* init_nbody(int count, int seed, float minMass, float maxMass, float2 spaceLow, float2 spaceHigh, bool spiral) {
    int memSize = sizeof(Body) * count;
    int memSize_ext = sizeof(Body_Info) * count;
    h_Bodies = (Body*)malloc(memSize);
    auto h_Bodies_ext = (Body_Info*)malloc(memSize_ext);
    memset(h_Bodies, 0, memSize);
    memset(h_Bodies_ext, 0, memSize_ext);
    srand(seed);

    float2 midPoint = (spaceHigh + spaceLow) * 0.5f;
    float extentLength = length(spaceHigh - midPoint) * 0.7;
    float spiral_center_mass = maxMass * 1000000;
    int neg_count = 0;
    int black_hole_count = 0;

    uint32_t total_system_mass = 0;

    for (size_t i = 0; i < count; i++)
    {

        h_Bodies[i].mass = frandom(minMass, maxMass);

        if (!spiral) {
            float randx = frandom(spaceLow.x, spaceHigh.x);
            float randy = frandom(spaceLow.y, spaceHigh.y);
            h_Bodies[i].position = make_float2(randx, randy);
        }
        else {
            h_Bodies[i].position = midPoint + get_random_direction() * extentLength;
        }

        h_Bodies_ext[i].velocity.x = 0;
        h_Bodies_ext[i].velocity.y = 0;

        if (neg_count == 63) {
            h_Bodies[i].mass *= -63.0f;
            neg_count = 0;
        }
        else {
            neg_count++;
        }

        if (spiral) {
            if (black_hole_count == count - 1) {
                h_Bodies[i].mass = frandom(spiral_center_mass * .1, spiral_center_mass * 0.25);
                black_hole_count = 0;
            }
            else {
                black_hole_count++;
            }
        }
        else {
            if (black_hole_count == ceilf((float)count * 0.25)) {
                h_Bodies[i].mass = maxMass * frandom(1000, 10000);
                black_hole_count = 0;
            }
            else {
                black_hole_count++;
            }
        }


        total_system_mass += h_Bodies[i].mass > 0.0 ? h_Bodies[i].mass : 0.0;
    }

    if (spiral) {
        for (size_t i = 0; i < count; i++)
        {
            float2 dir2mid = midPoint - h_Bodies[i].position;
            float dist = length(dir2mid) * DIST_SCALE;
            float3 dir2mid3 = normalize(make_float3(dir2mid.x, dir2mid.y, 0));
            float3 cross_dir = make_float3(0, 0, 1);
            float3 velocity_dir = normalize(cross(dir2mid3, cross_dir));
            float orbit_speed = sqrt(GRAV_CONST * (total_system_mass + spiral_center_mass / DIST_SCALE) / dist);
            float2 vel2 = make_float2(velocity_dir.x, velocity_dir.y) * orbit_speed;
            h_Bodies_ext[i].velocity = vel2;
        }
    }

    if (spiral) {
        h_Bodies[0].mass = spiral_center_mass;
        h_Bodies[0].position = midPoint;
        h_Bodies_ext[0].velocity *= 0;
    }

    body_count = count;
    CUDA_CHECK(cudaMalloc(&d_Bodies, memSize));
    CUDA_CHECK(cudaMalloc(&d_Bodies_ext, memSize_ext));
    CUDA_CHECK(cudaMemset(d_Bodies, 0, memSize));
    CUDA_CHECK(cudaMemset(d_Bodies_ext, 0, memSize_ext));

    CUDA_CHECK(cudaMemcpy(d_Bodies, h_Bodies, memSize, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_Bodies_ext, h_Bodies_ext, memSize, cudaMemcpyHostToDevice));

    CUDA_CHECK(cudaDeviceSynchronize());
    free(h_Bodies_ext);
    return h_Bodies;
}

void tick(float deltaTime, float colorScale, float renderScale, float2 renderOffset) {
    dim3 threads = dim3(64, 1, 1);
    dim3 blocks = dim3((uint)ceilf((float)body_count / threads.x), 1, 1);

    calculate_nbody << <blocks, threads >> > (d_Bodies, d_Bodies_ext, 0, body_count, body_count, deltaTime);
    CUDA_CHECK(cudaDeviceSynchronize());

    draw_bodies << <blocks, threads >> > (d_Bodies, d_Bodies_ext, body_count, h_render_texture, colorScale, renderScale, renderOffset);
    CUDA_CHECK(cudaDeviceSynchronize());
}