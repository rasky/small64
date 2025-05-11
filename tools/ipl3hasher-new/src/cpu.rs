pub struct CPUHasher {
    ipl3: [u32; 1008],
    state: [u32; 16],
}

impl CPUHasher {
    pub const MAGIC: u32 = 0x6C078965;

    fn add(a1: u32, a2: u32) -> u32 {
        u32::wrapping_add(a1, a2)
    }

    fn sub(a1: u32, a2: u32) -> u32 {
        u32::wrapping_sub(a1, a2)
    }

    fn mul(a1: u32, a2: u32) -> u32 {
        u32::wrapping_mul(a1, a2)
    }

    fn rol(a: u32, s: u32) -> u32 {
        u32::rotate_left(a, s)
    }

    fn ror(a: u32, s: u32) -> u32 {
        u32::rotate_right(a, s)
    }

    fn sum(a0: u32, a1: u32, a2: u32) -> u32 {
        let prod = (a0 as u64).wrapping_mul(if a1 == 0 { a2 as u64 } else { a1 as u64 });
        let hi = ((prod >> 32) & 0xFFFFFFFF) as u32;
        let lo = (prod & 0xFFFFFFFF) as u32;
        let diff = hi.wrapping_sub(lo);
        return if diff == 0 { a0 } else { diff };
    }

    fn calculate(ipl3: &[u32; 1008], state: &mut [u32; 16], end: u32) {
        let end = end.min(1008);

        for i in 1..=end as u32 {
            let prev = ipl3[i.saturating_sub(2) as usize];
            let data = ipl3[i.saturating_sub(1) as usize];

            state[0] = Self::add(state[0], Self::sum(Self::sub(1007, i), data, i));
            state[1] = Self::sum(state[1], data, i);
            state[2] = state[2] ^ data;
            state[3] = Self::add(state[3], Self::sum(Self::add(data, 5), Self::MAGIC, i));
            state[4] = Self::add(state[4], Self::ror(data, prev & 0x1F));
            state[5] = Self::add(state[5], Self::rol(data, prev >> 27));
            state[6] = if data < state[6] {
                Self::add(state[3], state[6]) ^ Self::add(data, i)
            } else {
                Self::add(state[4], data) ^ state[6]
            };
            state[7] = Self::sum(state[7], Self::rol(data, prev & 0x1F), i);
            state[8] = Self::sum(state[8], Self::ror(data, prev >> 27), i);
            state[9] = if prev < data {
                Self::sum(state[9], data, i)
            } else {
                Self::add(state[9], data)
            };

            if i == end {
                break;
            }

            let next = ipl3[i as usize];

            state[10] = Self::sum(Self::add(state[10], data), next, i);
            state[11] = Self::sum(state[11] ^ data, next, i);
            state[12] = Self::add(state[12], state[8] ^ data);
            state[13] = Self::add(
                state[13],
                Self::add(Self::ror(data, data & 0x1F), Self::ror(next, next & 0x1F)),
            );
            state[14] = Self::sum(
                Self::sum(state[14], Self::ror(data, prev & 0x1F), i),
                Self::ror(next, data & 0x1F),
                i,
            );
            state[15] = Self::sum(
                Self::sum(state[15], Self::rol(data, prev >> 27), i),
                Self::rol(next, data >> 27),
                i,
            );
        }
    }

    pub fn finalize(state: &[u32; 16]) -> u64 {
        let mut buffer = vec![state[0]; 4];

        for i in 0..16 as u32 {
            let data = state[i as usize];

            buffer[0] = Self::add(buffer[0], Self::ror(data, data & 0x1F));
            buffer[1] = if data < buffer[0] {
                Self::add(buffer[1], data)
            } else {
                Self::sum(buffer[1], data, i)
            };
            buffer[2] = if ((data & 0x02) >> 1) == (data & 0x01) {
                Self::add(buffer[2], data)
            } else {
                Self::sum(buffer[2], data, i)
            };
            buffer[3] = if (data & 0x01) == 0x01 {
                buffer[3] ^ data
            } else {
                Self::sum(buffer[3], data, i)
            };
        }

        let final_sum = Self::sum(buffer[0], buffer[1], 16);
        let final_xor = buffer[3] ^ buffer[2];

        (((final_sum & 0xFFFF) as u64) << 32) | (final_xor as u64)
    }

    pub fn new(ipl3_raw_data: &[u8; 4032], seed: u8) -> Self {
        let mut ipl3 = [0u32; 1008];

        for (i, bytes) in ipl3_raw_data.chunks(4).enumerate() {
            ipl3[i] = u32::from_be_bytes(bytes[0..4].try_into().unwrap());
        }

        let mut state = [0u32; 16];

        state.fill(Self::add(Self::mul(Self::MAGIC, seed as u32), 1) ^ ipl3[0]);

        Self { ipl3, state }
    }

    fn apply_y_bits(&self, y_bits: Vec<u32>, y: u32) -> [u32; 1008] {
        let mut ipl3 = self.ipl3.clone();

        for (i, offset) in y_bits.iter().enumerate() {
            let index = (offset / 32) as usize;
            let bit = 31 - (offset % 32);
            let shift = y_bits.len() - 1;
            let value = (y >> (shift - i)) & (1 << 0);

            ipl3[index] &= !(1 << bit);
            ipl3[index] |= value << bit;
        }

        ipl3
    }

    pub fn y_round(&self, y_bits: Vec<u32>, y: u32) -> (u32, [u32; 16]) {
        let ipl3 = self.apply_y_bits(y_bits, y);
        let mut state = self.state.clone();

        Self::calculate(&ipl3, &mut state, 1007);

        let prev = ipl3[1005];
        let data = ipl3[1006];

        // OPTIMIZATION: Precalculate some values to speed up computation on the GPU side

        state[10] = Self::add(state[10], data);
        state[11] = state[11] ^ data;
        state[12] = Self::add(state[12], state[8] ^ data);
        state[13] = Self::add(state[13], Self::ror(data, data & 0x1F));
        state[14] = Self::sum(state[14], Self::ror(data, prev & 0x1F), 1007);
        state[15] = Self::sum(state[15], Self::rol(data, prev >> 27), 1007);

        (data, state)
    }

    pub fn verify(&self, y_bits: Vec<u32>, y: u32, x: u32) -> u64 {
        let mut ipl3 = self.apply_y_bits(y_bits, y);
        let mut state = self.state.clone();

        ipl3[1007] = x;

        Self::calculate(&ipl3, &mut state, 1008);
        Self::finalize(&state)
    }
}
