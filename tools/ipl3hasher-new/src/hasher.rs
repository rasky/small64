use crate::{cpu, error::HasherError, gpu};
use std::io::{Read, Seek, Write};

pub enum HasherResult {
    Found(u32, u32),
    Continue,
    End,
}

pub struct Hasher {
    cpu: cpu::CPUHasher,
    gpu: gpu::GPUHasher,
    target_checksum: u64,
    y_bits: Vec<u32>,
    y: u32,
}

impl Hasher {
    pub fn new(
        path: std::path::PathBuf,
        gpu_adapter_id: usize,
        workgroups: (u32, u32, u32),
        shader: gpu::GPUHasherShader,
        seed: u8,
        target_checksum: u64,
        y_bits: Vec<u32>,
        y_init: u32,
    ) -> Result<Self, HasherError> {
        let ipl3 = Self::load_ipl3(path)?;

        let cpu = cpu::CPUHasher::new(&ipl3, seed);

        let adapters = gpu::GPUHasher::list_gpu_adapters();
        let adapter = adapters
            .get(gpu_adapter_id)
            .ok_or(HasherError::GPUAdapterOutOfBounds)?;
        let gpu = gpu::GPUHasher::new(adapter.clone(), shader, workgroups)?;

        Ok(Self {
            cpu,
            gpu,
            target_checksum,
            y_bits,
            y: y_init,
        })
    }

    fn load_ipl3(path: std::path::PathBuf) -> Result<[u8; 4032], HasherError> {
        let mut f = std::fs::File::open(path)?;

        let mut ipl3 = [0u8; 4032];

        f.seek(std::io::SeekFrom::Start(64))?;
        f.read_exact(&mut ipl3)?;

        Ok(ipl3)
    }

    pub fn sign_rom(
        path: std::path::PathBuf,
        y_bits: Vec<u32>,
        y: u32,
        x: u32,
    ) -> Result<(), HasherError> {
        let mut f = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(path)?;

        for (i, offset) in y_bits.iter().enumerate() {
            let index = offset / 8;
            let bit = 7 - (offset % 8);
            let shift = y_bits.len() - 1;
            let value = ((y >> (shift - i)) & (1 << 0)) as u8;

            let mut byte = [0u8];

            f.seek(std::io::SeekFrom::Start(index as u64 + 64))?;
            f.read_exact(&mut byte)?;

            byte[0] &= !(1 << bit);
            byte[0] |= value << bit;

            f.seek(std::io::SeekFrom::Current(-1))?;
            f.write_all(&mut byte)?;
        }

        f.seek(std::io::SeekFrom::Start(4092))?;
        f.write_all(&mut x.to_be_bytes().to_vec())?;

        f.flush()?;

        Ok(())
    }

    pub fn get_y(&self) -> u32 {
        self.y
    }

    fn is_y_finished(&self) -> bool {
        (self.y as u64) > ((1u64 << self.y_bits.len()) - 1)
    }

    pub fn get_gpu_info(&self) -> wgpu::AdapterInfo {
        self.gpu.get_gpu_info()
    }

    pub fn compute_round(&mut self) -> Result<HasherResult, HasherError> {
        if self.is_y_finished() {
            return Ok(HasherResult::End);
        }

        let (y_offset, state) = self.cpu.y_round(self.y_bits.clone(), self.y);

        let mut x_offset = 0;

        loop {
            let result = self
                .gpu
                .x_round(self.target_checksum, y_offset, x_offset, state)?;

            match result {
                gpu::GPUHasherResult::Found(x) => {
                    let verify_checksum = self.cpu.verify(self.y_bits.clone(), self.y, x);
                    if verify_checksum != self.target_checksum {
                        return Err(HasherError::ChecksumVerifyError(self.y, x, verify_checksum));
                    }
                    return Ok(HasherResult::Found(self.y, x));
                }
                gpu::GPUHasherResult::Continue(x_step) => {
                    x_offset += x_step;
                }
                gpu::GPUHasherResult::End => {
                    break;
                }
            }
        }

        if self.is_y_finished() {
            return Ok(HasherResult::End);
        }

        self.y += 1;

        Ok(HasherResult::Continue)
    }
}
