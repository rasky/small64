override LOCAL_WORKGROUP_SIZE: u32 = 256;

const MAGIC: u32 = 0x6C078965;

struct Input {
    target_hi: u32,
    target_lo: u32,
    y_offset: u32,
    x_offset: u32,
    state: array<u32, 16>,
}

struct Output {
    found: atomic<i32>,
    x_result: u32,
}

var<push_constant> input: Input;

@group(0) @binding(0) var<storage, read_write> output: Output;

var<private> state: array<u32, 16>;

fn sum(a0: u32, a1: u32, a2: u32) -> u32 {
    var v1: u32 = a1;

    if a1 == 0 {
        v1 = a2;
    }

    let prod: u64 = u64(a0) * u64(v1);
    let hi: u32 = u32(prod >> 32);
    let lo: u32 = u32(prod);
    let diff: u32 = hi - lo;

    if diff == 0 {
        return a0;
    }

    return diff;
}

fn finalize_checksum(y: u32, x: u32) {
    let yts: u32 = y >> 27;
    let ytc: u32 = 32 - yts;

    let ybs: u32 = y & 0x1F;
    let ybc: u32 = 32 - ybs;

    let xbs: u32 = x & 0x1F;
    let xbc: u32 = 32 - xbs;

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

fn finalize_hi() -> u32 {
    var buf: array<u32, 2>;

    for (var i: u32 = 0; i < 2; i++) {
        buf[i] = state[0];
    }

    for (var i: u32 = 0; i < 16; i++) {
        let data: u32 = state[i];
        let shift: u32 = data & 0x1F;

        buf[0] += (data >> shift) | (data << (32 - shift));

        if data < buf[0] {
            buf[1] += data;
        } else {
            buf[1] = sum(buf[1], data, i);
        }
    }

    return sum(buf[0], buf[1], 16) & 0xFFFF;
}

fn finalize_lo() -> u32 {
    var buf: array<u32, 2> = array(state[0], state[0]);

    for (var i: u32 = 0; i < 16; i++) {
        let data: u32 = state[i];
        let tmp: u32 = (data & (1 << 1)) >> 1;
        let tmp2: u32 = data & (1 << 0);

        if tmp == tmp2 {
            buf[0] += data;
        } else {
            buf[0] = sum(buf[0], data, i);
        }

        if tmp2 == 1 {
            buf[1] ^= data;
        } else {
            buf[1] = sum(buf[1], data, i);
        }
    }

    return buf[0] ^ buf[1];
}

@compute @workgroup_size(LOCAL_WORKGROUP_SIZE)
fn main(
    @builtin(global_invocation_id) global_id: vec3<u32>,
    @builtin(num_workgroups) num_workgroups: vec3<u32>,
) {
    state = input.state;
    let y: u32 = input.y_offset;
    let x: u32 =
        (global_id.z * num_workgroups.y * num_workgroups.x * LOCAL_WORKGROUP_SIZE) +
        (global_id.y * num_workgroups.x * LOCAL_WORKGROUP_SIZE) +
        (global_id.x) +
        input.x_offset;

    finalize_checksum(y, x);
    if finalize_hi() == input.target_hi {
        if finalize_lo() == input.target_lo {
            if atomicOr(&output.found, 1) == 0 {
                output.x_result = x;
            }
        }
    }
}
