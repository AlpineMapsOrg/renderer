
fn compute_hash(a: u32) -> u32
{
   var b = (a + 2127912214u) + (a << 12u);
   b = (b ^ 3345072700u) ^ (b >> 19u);
   b = (b + 374761393u) + (b << 5u);
   b = (b + 3551683692u) ^ (b << 9u);
   b = (b + 4251993797u) + (b << 3u);
   b = (b ^ 3042660105u) ^ (b >> 16u);
   return b;
}

fn color_from_id_hash(a: u32) -> vec3f {
    var hash = compute_hash(a);
    return vec3f(f32(hash & 255u), f32((hash >> 8u) & 255u), f32((hash >> 16u) & 255u)) / 255.0;
}
