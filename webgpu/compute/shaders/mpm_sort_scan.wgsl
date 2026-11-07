/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

///use mpm_common

// Particle sort, pass 2 of 3: turns the bucket counts into the first sorted position of each
// bucket (an exclusive prefix sum) and resets the counts for the next sort. One workgroup:
// each thread sums a contiguous range of buckets, the 256 partial sums are scanned in
// workgroup memory, then each thread writes the offsets of its range. 6,401 buckets at 320^2
// columns, 16,385 at the 512^2 maximum - a few dozen per thread.

const SCAN_THREADS: u32 = 256u;

var<workgroup> partial_sums: array<u32, SCAN_THREADS>;

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(local_invocation_index) thread: u32) {
    let buckets = sort_bucket_count();
    let per_thread = (buckets + SCAN_THREADS - 1u) / SCAN_THREADS;
    let begin = min(thread * per_thread, buckets);
    let end = min(begin + per_thread, buckets);

    var sum = 0u;
    for (var b = begin; b < end; b++) {
        sum += atomicLoad(&tile_flags[sort_counts_offset() + b]);
    }
    partial_sums[thread] = sum;
    workgroupBarrier();

    // Inclusive Hillis-Steele scan over the partial sums.
    for (var stride = 1u; stride < SCAN_THREADS; stride *= 2u) {
        var value = partial_sums[thread];
        if thread >= stride {
            value += partial_sums[thread - stride];
        }
        workgroupBarrier();
        partial_sums[thread] = value;
        workgroupBarrier();
    }

    var offset = partial_sums[thread] - sum; // exclusive: the buckets before this thread's range
    for (var b = begin; b < end; b++) {
        let count = atomicLoad(&tile_flags[sort_counts_offset() + b]);
        atomicStore(&tile_flags[sort_cursors_offset() + b], offset);
        atomicStore(&tile_flags[sort_counts_offset() + b], 0u);
        offset += count;
    }
}
