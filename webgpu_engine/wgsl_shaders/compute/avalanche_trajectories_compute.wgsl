/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2025 Markus Rampp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "util/filtering.wgsl"
#include "util/tile_util.wgsl"
#include "util/normals_util.wgsl"
#include "util/random.wgsl"
#include "util/encoder.wgsl"


// weights need to match the texels that are chosen by textureGather - this does NOT align perfectly, and introduces some artifacts
// adding offset fixes this issue, see https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/
const TEXTURE_GATHER_OFFSET = 1.0f / 512.0f;
const g: f32 = 9.81;
const density: f32 = 200.0;
const slab_thickness: f32 = 1;
const cfl: f32 = 0.5;

const mass_per_area = density * slab_thickness;
const acceleration_gravity = vec3f(0.0, 0.0, -g);
const velocity_threshold: f32 = 0.01f;

struct AvalancheTrajectoriesSettings {
    output_resolution: vec2u,
    region_size: vec2f, // world space width and height of the the region we operate on

    num_steps: u32, // maximum number of steps (along gradient)
    step_length: f32, // length of one simulation step in world space
//    num_paths_per_release_cell: u32,

    max_perturbation: f32, // randomness contribution on normal in [0,1], 0 means no randomness, 1 means only randomness
    persistence_contribution: f32, // persistence contribution on normal in [0,1], 0 means only local normal, 1 means only last normal

    model_type: u32, //0 is simple, 1 is more complex
    model1_linear_drag_coeff: f32,
    model1_downward_acceleration_coeff: f32,
    model2_gravity: f32,
    model2_mass: f32,
    model2_friction_coeff: f32,
    model2_drag_coeff: f32,

    //model5_weights: array<f32, 8>, // wgsl compiler does not allow f32 arrays in uniforms because of padding requirements, altough it would work here
    model5_center_height_offset: f32,
    model5_weights: array<vec4f, 2>,

    runout_model_type: u32, //actually: friction model: 0 coulomb, 1 voellmy, 2 voellmy minshear, 3 samosAt

    runout_perla_my: f32, // sliding friction coeff
    runout_perla_md: f32, // M/D mass-to-drag ratio
    runout_perla_l: f32, // distance between grid cells (in m)
    runout_perla_g: f32, // acceleration due to gravity (in m/s^2)

    runout_flowpy_alpha: f32, // alpha runout angle in radians

    layer1_zdelta_enabled: u32, // 0 = disabled, 1 = enabled
    layer2_cellCounts_enabled: u32,
    layer3_travelLength_enabled: u32,
    layer4_travelAngle_enabled: u32,
    layer5_altitudeDifference_enabled: u32,

    random_seed: u32,
}

// input
@group(0) @binding(0) var<uniform> settings: AvalancheTrajectoriesSettings;
@group(0) @binding(1) var input_normal_texture: texture_2d<f32>;
@group(0) @binding(2) var input_height_texture: texture_2d<f32>;
@group(0) @binding(3) var input_release_point_texture: texture_2d<f32>;
@group(0) @binding(4) var input_normal_sampler: sampler;
@group(0) @binding(5) var input_height_sampler: sampler;

// output
@group(0) @binding(6) var<storage, read_write> output_storage_buffer: array<atomic<u32>>; // trajectory texture

// output layers
@group(0) @binding(7) var<storage, read_write> output_layer1_zdelta: array<atomic<u32>>;
@group(0) @binding(8) var<storage, read_write> output_layer2_cellCounts: array<atomic<u32>>;
@group(0) @binding(9) var<storage, read_write> output_layer3_travelLength: array<atomic<u32>>;
@group(0) @binding(10) var<storage, read_write> output_layer4_travelAngle: array<atomic<u32>>;
@group(0) @binding(11) var<storage, read_write> output_layer5_altitudeDifference: array<atomic<u32>>;

// note: as of writing this, wgsl only supports atomic access for storage buffers and only for u32 and i32
//       therefore, we first write the risk value (along the trajectory as raster) into a buffer,
//       then write its contents into a texture in a subsequent step (avalanche_trajectories_buffer_to_texture.wgsl)


// Samples normal texture with bilinear filtering.
fn sample_normal_texture(uv: vec2f) -> vec3f {
    return textureSampleLevel(input_normal_texture, input_normal_sampler, uv, 0).xyz * 2 - 1;
}

// Samples height texture with bilinear filtering.
fn sample_height_texture(uv: vec2f) -> f32 {
    let texture_dimensions = textureDimensions(input_height_texture);
    let weights: vec2f = fract(uv * vec2f(texture_dimensions) - 0.5f + TEXTURE_GATHER_OFFSET); // -0.5 to make relative to texel center
    let texel_values: vec4f = textureGather(0, input_height_texture, input_height_sampler, uv);
    return dot(vec4f((1.0 - weights.x) * weights.y, weights.x * weights.y, weights.x * (1.0 - weights.y), (1.0 - weights.x) * (1.0 - weights.y)), texel_values);
}

// Samples release point texture with nearest neighbor.
// Returns true if alpha component is greater than 0, false otherwise.
fn sample_release_point_texture(uv: vec2f) -> bool {
    let texture_dimensions = textureDimensions(input_release_point_texture);
    let pos = vec2u(uv * vec2f(texture_dimensions));
    let mask = textureLoad(input_release_point_texture, pos, 0).rgba;
    return mask.a > 0;
}

fn draw_line_uv(start_uv: vec2f, end_uv: vec2f, value: f32, z_delta: f32, travel_length: f32, travel_angle: f32, altitude_difference: f32) {
    let start_pos = vec2u(floor(start_uv * vec2f(settings.output_resolution)));
    let end_pos = vec2u(floor(end_uv * vec2f(settings.output_resolution)));
    draw_line_pos(start_pos, end_pos, value, z_delta, travel_length, travel_angle, altitude_difference);
}

// implementation of bresenham's line algorithm
// adapted from https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm#All_cases (used last one, with error) 
fn draw_line_pos(start_pos: vec2u, end_pos: vec2u, value: f32, z_delta: f32, travel_length: f32, travel_angle: f32, altitude_difference: f32) {
    let dx = abs(i32(end_pos.x) - i32(start_pos.x));
    let sx = select(-1, 1, start_pos.x < end_pos.x);
    let dy = -abs(i32(end_pos.y) - i32(start_pos.y));
    let sy = select(-1, 1, start_pos.y < end_pos.y);
    var error = dx + dy;
    var x = i32(start_pos.x);
    var y = i32(start_pos.y);

    const layer_scaling = 1.0;//1000.0; // scaling factor for layer values, to avoid precision loss (also change in buffer_to_texture)

    while (true) {
        let buffer_index = u32(y) * settings.output_resolution.x + u32(x);
        // Commented out for a fair comparison, (other implementations also just write the other layers)
        //atomicMax(&output_storage_buffer[buffer_index], range_to_u32(value, U32_ENCODING_RANGE_NORM)); // map value from [0,1] angle to [0, 2^32 - 1]
        if (settings.layer1_zdelta_enabled != 0) {
            atomicMax(&output_layer1_zdelta[buffer_index], u32(z_delta * layer_scaling)); // zdelta in m (we lose some precision here)
        }
        if (settings.layer2_cellCounts_enabled != 0) {
            atomicAdd(&output_layer2_cellCounts[buffer_index], 1 * u32(layer_scaling)); // count number of steps in this layer
        }
        if (settings.layer3_travelLength_enabled != 0) {
            atomicMax(&output_layer3_travelLength[buffer_index], u32(travel_length * layer_scaling)); // travel length in m (we lose some precision here)
        }
        if (settings.layer4_travelAngle_enabled != 0) {
            let travel_angle_value: f32 = degrees(travel_angle); // travel angle in deg (we lose some precision here)
            atomicMax(&output_layer4_travelAngle[buffer_index], u32(travel_angle_value * layer_scaling));
        }
        if (settings.layer5_altitudeDifference_enabled != 0) {
            atomicMax(&output_layer5_altitudeDifference[buffer_index], u32(altitude_difference * layer_scaling));
        }

        if (x == i32(end_pos.x) && y == i32(end_pos.y)) {
            break;
        }

        let e2 = 2 * error;
        if (e2 >= dy) {
            error = error + dy;
            x += sx;
        }
        if (e2 <= dx) {
            error = error + dx;
            y += sy;
        }
    }
}

// ***** MODELS *****



// returns UV coordinates for the trajectory starting point for a thread id
fn get_starting_point_uv(id: vec3<u32>) -> vec2f {
    let input_texture_size = textureDimensions(input_normal_texture);
    let texel_size_uv = 1.0 / vec2f(input_texture_size);
    //let uv = vec2f(f32(id.x), f32(id.y)) * texel_size_uv + texel_size_uv / 2.0; // texel center
    let uv = vec2f(f32(id.x), f32(id.y)) * texel_size_uv + rand2() * texel_size_uv; // random within texel
    //TODO try regular grid (?)
    return uv;
}

fn trajectory_overlay(id: vec3<u32>) {
    seed(vec4u(id, settings.random_seed)); //seed PRNG with thread id
    let pixel_size = vec2f(settings.region_size) / vec2f(textureDimensions(input_normal_texture));
    let dx = min(pixel_size.x, pixel_size.y);

    let uv = get_starting_point_uv(id);
//    let uv = vec2f(f32(id.x), f32(id.y)) * texel_size_uv;

    if (!sample_release_point_texture(uv)) {
        return;
    }

    // get slope angle at start
    let start_normal = sample_normal_texture(uv);
    var velocity = vec3f(0, 0, 0);
    var last_velocity = vec3f(0, 0, 0);

    let start_slope_angle = get_slope_angle(start_normal);
    let trajectory_value = start_slope_angle / (PI / 2);

    // alpha-beta model state
    let start_point_height: f32 = sample_height_texture(uv);
    var world_space_travel_distance: f32 = 0.0;

    var last_dir_index: i32 = -1;  // used for d8 with weights
    var last_uv = uv;
    var world_space_offset = vec2f(0, 0); // offset from original world position

    var normal_t = vec3f(0, 0, 1);

    var acceleration_tangential =  acceleration_gravity - g * start_normal.z * start_normal;
    var acceleration_friction = vec3f(0, 0, 0);
    var dt = sqrt(2 * dx / length(acceleration_tangential));
    var last_direction = vec2f(0, 0);

    var z_delta = 0f;
    
    var velocity_magnitude = 0f;

    for (var i: u32 = 0; i < settings.num_steps; i++) {
        // compute uv coordinates for current position
        let current_uv = uv + vec2f(world_space_offset.x, -world_space_offset.y) / settings.region_size;

        // quit if moved out of bounds
        if (current_uv.x < 0 || current_uv.x > 1 || current_uv.y < 0 || current_uv.y > 1) {
            break;
        }


        let current_height = sample_height_texture(current_uv);

        // check if we are still on the terrain
        // avaframe examples have non rectangular terrain
        // missing values are noramlly -9999
        // webigeo sets them to 0
        if (current_height < 10) {
            break;
        }

        let normal = sample_normal_texture(current_uv);

        if (i > 0) {
            // calculate physical quantities
            //  - altitude difference from starting point
            //  - z_delta (kinetic energy ~ velocity)
            //  - delta (local runout angle)
            //  - distance we have already

            let height_difference = start_point_height - current_height;
            let z_alpha = tan(settings.runout_flowpy_alpha) * world_space_travel_distance;
            let z_gamma = height_difference;
            z_delta = z_gamma - z_alpha;
            let gamma = atan(height_difference / world_space_travel_distance); // will always be positive -> [ 0 , PI/2 ]
//            let delta = gamma - settings.runout_flowpy_alpha;
            // evaluate runout model, terminate, if necessary
            if (settings.model_type == 0) {
                // more info: https://docs.avaframe.org/en/latest/theoryCom4FlowPy.html
                if (z_delta <= 0) {
                    break;
                }
            }
            if (settings.model_type == 1) {
                z_delta = velocity_magnitude * velocity_magnitude / (2*g);
            }

            // draw line from last to current position
            draw_line_uv(last_uv, current_uv, trajectory_value, z_delta, world_space_travel_distance, gamma, height_difference);
        }
        last_uv = current_uv;

        // sample normal and get new world space offset based on chosen model
        if (settings.model_type == 0) {
            let perturbed_normal: vec3f = perturb(normal);
            let perturbed_normal_2d: vec2f = perturbed_normal.xy;
            var velocity_magnitude = sqrt(z_delta*2*g);
            if (velocity_magnitude < 1) { //check potential 0-division before normalization
                velocity_magnitude = 1;
            }
            let current_direction = last_direction * settings.persistence_contribution + perturbed_normal_2d / velocity_magnitude  * (1.0 - settings.persistence_contribution);

            let dir_magnitude = length(current_direction);
            if (dir_magnitude < 0.001) { //check potential 0-division before normalization
                break;
            }
            let normalized_current_direction = current_direction / dir_magnitude;
            last_direction = normalized_current_direction;

            //TODO 2 is step length here, use uniform, put into ui
            let relative_trajectory = normalized_current_direction.xy * 2 * settings.step_length;

            world_space_offset = world_space_offset + relative_trajectory;
            world_space_travel_distance += length(relative_trajectory);
        } else if (settings.model_type == 1) {
            let acceleration_normal = g * normal.z * normal;
            let acceleration_tangential = acceleration_gravity + acceleration_normal;
            // estimate optimal timestep
            dt = cfl * dx / length(velocity + acceleration_tangential * dt);
            let acceleration_friction_magnitude = acceleration_by_friction(acceleration_normal, mass_per_area, velocity);
            velocity = velocity + acceleration_tangential * dt;
            // friction stop criterion, it has to use the new timestep
            if(length(velocity) < acceleration_friction_magnitude * dt){
                dt = length(velocity) / acceleration_friction_magnitude;
//                let relative_trajectory = dt * 0.5 * (last_velocity + velocity);
                let relative_trajectory = velocity * dt;
                world_space_offset = world_space_offset + relative_trajectory.xy;
                world_space_travel_distance += length(relative_trajectory.xy);
                break;
            }
            last_velocity = velocity;
            velocity = velocity + acceleration_tangential * dt;
            // explicit
            velocity = velocity - acceleration_friction_magnitude * normalize(velocity) * dt;
            // implicit
//            velocity_magnitude = length(velocity);
//            velocity = velocity / (1.0 - acceleration_friction /
//                select(velocity_magnitude, velocity_threshold, velocity_magnitude < velocity_threshold) * dt);
//            let relative_trajectory = dt * 0.5 * (last_velocity + velocity);
            let relative_trajectory = velocity * dt;
            world_space_offset = world_space_offset + relative_trajectory.xy;
            world_space_travel_distance += length(relative_trajectory.xy);
            velocity_magnitude = length(velocity);
            if (velocity_magnitude < velocity_threshold ){
                break;
            }
            last_velocity = velocity;
        }
    }
}


// Generates a random unit vector in a cone around the given vector
fn perturb(v: vec3<f32>) -> vec3<f32> {
    let cos_max_angle_rad = cos(settings.max_perturbation * PI / 180.0); // max angle in radians
    let r = rand2();
    let u1 = r.x;
    let u2 = r.y;

    // Convert from uniform random to spherical coordinates within cone
    let cos_theta = cos_max_angle_rad + (1 - cos_max_angle_rad) * u1;  // this is uniform
    //let cos_theta = cos_max_angle_rad + (1 - cos_max_angle_rad) * sqrt(u1); // this i think should be cosine weighted
    let sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    let phi = 2.0 * PI * u2;

    // If the vector is close to the z-axis, use a different up vector
    var up = select(vec3<f32>(0.0, 0.0, 1.0), vec3<f32>(1.0, 0.0, 0.0), abs(v.z) >= 0.999);

    let tangent = normalize(cross(up, v));
    let bitangent = cross(v, tangent);

    // Compute perturbed direction
    return
        v * cos_theta +
        tangent * sin_theta * cos(phi) +
        bitangent * sin_theta * sin(phi);
}

fn acceleration_by_friction(acceleration_normal: vec3f, mass_per_area: f32, velocity: vec3f) -> f32 {
    let velocity_magnitude = length(velocity);
    if (velocity_magnitude < velocity_threshold || settings.runout_model_type == 4) {
        return 0.0f;
    }
    let model = settings.runout_model_type;
    // standard 0.155, samos: standard 0.155, small 0.22, medium 0.17
    let friction_coefficient = settings.model2_friction_coeff;
    let drag_coefficient = settings.model2_drag_coeff; // only used for voellmy, standard 4000.
    let normal_stress = length(acceleration_normal * mass_per_area);
    const min_shear_stress = 70f;
    var shear_stress = 0.0f;
    //actually: friction model: 0 coulomb, 1 voellmy, 2 voellmy minshear, 3 samosAt
    // Coulomb friction model
    if (model == 0){
        shear_stress = friction_coefficient * normal_stress;
    }
    // Voellmy friction model
    else if (model == 1){
        shear_stress = friction_coefficient * normal_stress + density * g * velocity_magnitude * velocity_magnitude / drag_coefficient;
    }
    // Voellmy min shear friction model
    else if (model == 2){
        shear_stress = min_shear_stress + friction_coefficient * normal_stress + density * g * velocity_magnitude * velocity_magnitude / drag_coefficient;
    }
    // samosAT friction model
    else if (model == 3){
        let min_shear_stress = 0f;
        let rs0 = 0.222;
        let kappa = 0.43;
        let r = 0.05;
        let b = 4.13;
        let rs = density * velocity_magnitude * velocity_magnitude / (normal_stress + 0.001);
        var div = slab_thickness / r;
        if (div < 1.0){
            div = 1.0;
        }
        div = log(div) / kappa + b;
        shear_stress = min_shear_stress + normal_stress * friction_coefficient * (1.0 + rs0 / (rs0 + rs)) + density * velocity_magnitude * velocity_magnitude / (div * div);
    }
    let acceleration_magnitude = shear_stress / mass_per_area;
    return acceleration_magnitude;
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(texture_dimensions(output_tiles).xy / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    let texture_dimensions = textureDimensions(input_release_point_texture);
    if (id.x >= texture_dimensions.x || id.y >= texture_dimensions.y) {
        return;
    }
    // id.xy in [0, texture_dimensions(output_tiles) - 1]
    // id.z contains sample index
    trajectory_overlay(id);
}