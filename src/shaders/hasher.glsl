#version 450 core
#extension GL_ARB_gpu_shader_int64 : enable

#define LOCAL_WORKGROUP_SIZE 256

#define MAGIC 0x6C078965

layout(local_size_x = LOCAL_WORKGROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform input_data {
    uint target_hi;
    uint target_lo;
    uint y_offset;
    uint x_offset;
    uint state_in[16];
};

layout(binding = 0) buffer output_data {
    int found;
    uint x_result;
};

uint state[16];

uint sum(uint a0, uint a1, uint a2) {
    uint v1 = a1;

    if (a1 == 0) {
        v1 = a2;
    }

    uint64_t prod = uint64_t(a0) * uint64_t(v1);
    uint hi = uint(prod >> 32);
    uint lo = uint(prod);
    uint diff = hi - lo;

    if (diff == 0) {
        return a0;
    }

    return diff;
}

void finalize_checksum(uint y, uint x) {
    uint yts = y >> 27;
    uint ytc = 32 - yts;

    uint ybs = y & 0x1F;
    uint ybc = 32 - ybs;

    uint xbs = x & 0x1F;
    uint xbc = 32 - xbs;

    state[0] += sum(0xFFFFFFFF, x, 1008);
    state[1] = sum(state[1], x, 1008);
    state[2] ^= x;
    state[3] += sum(x + 5, MAGIC, 1008);
    state[4] += (x >> ybs) | (x << ybc);
    state[5] += (x << yts) | (x >> ytc);
    if (x < state[6]) {
        state[6] = (x + 1008) ^ (state[3] + state[6]);
    } else {
        state[6] ^= (state[4] + x);
    }
    state[7] = sum(state[7], (x << ybs) | (x >> ybc), 1008);
    state[8] = sum(state[8], (x >> yts) | (x << ytc), 1008);
    if (y < x) {
        state[9] = sum(state[9], x, 1008);
    } else {
        state[9] += x;
    }
    state[10] = sum(state[10], x, 1007);
    state[11] = sum(state[11], x, 1007);
    state[13] += (x >> xbs) | (x << xbc);
    state[14] = sum(state[14], (x >> ybs) | (x << ybc), 1007);
    state[15] = sum(state[15], (x << yts) | (x >> ytc), 1007);
}

uint finalize_hi(void) {
    uint buf[2];

    for (int i = 0; i < 2; i++) {
        buf[i] = state[0];
    }

    for (uint i = 0; i < 16; i++) {
        uint data = state[i];
        uint shift = data & 0x1F;

        buf[0] += (data >> shift) | (data << (32 - shift));

        if (data < buf[0]) {
            buf[1] += data;
        } else {
            buf[1] = sum(buf[1], data, i);
        }
    }

    return sum(buf[0], buf[1], 16) & 0xFFFF;
}

uint finalize_lo(void) {
    uint buf[2] = {state[0], state[0]};

    for (uint i = 0; i < 16; i++) {
        uint data = state[i];
        uint tmp = (data & (1 << 1)) >> 1;
        uint tmp2 = data & (1 << 0);

        if (tmp == tmp2) {
            buf[0] += data;
        } else {
            buf[0] = sum(buf[0], data, i);
        }

        if (tmp2 == 1) {
            buf[1] ^= data;
        } else {
            buf[1] = sum(buf[1], data, i);
        }
    }

    return buf[0] ^ buf[1];
}

void main(void) {
    state = state_in;
    uint y = y_offset;
    uint x =
        (gl_GlobalInvocationID.z * gl_NumWorkGroups.y * gl_NumWorkGroups.x * LOCAL_WORKGROUP_SIZE) +
        (gl_GlobalInvocationID.y * gl_NumWorkGroups.x * LOCAL_WORKGROUP_SIZE) +
        (gl_GlobalInvocationID.x) +
        x_offset;

    finalize_checksum(y, x);
    if (finalize_hi() == target_hi) {
        if (finalize_lo() == target_lo) {
            if (atomicOr(found, 1) == 0) {
                x_result = x;
            }
        }
    }
}
