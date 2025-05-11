#[derive(Debug)]
pub enum HasherError {
    ChecksumVerifyError(u32, u32, u64),
    GPUAdapterOutOfBounds,
    WgpuRequestDeviceError(wgpu::RequestDeviceError),
    WgpuPollError(wgpu::PollError),
    IoError(std::io::Error),
}

impl std::fmt::Display for HasherError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ChecksumVerifyError(y, x, verify_checksum) => f.write_fmt(format_args!(
                "GPU Hasher result is wrong: Y={y:08X} X={x:08X} | 0x{verify_checksum:012X}"
            )),
            Self::GPUAdapterOutOfBounds => f.write_str("Selected GPU adapter doesn't exist"),
            Self::WgpuRequestDeviceError(error) => f.write_str(error.to_string().as_str()),
            Self::WgpuPollError(error) => f.write_str(error.to_string().as_str()),
            Self::IoError(error) => f.write_str(error.to_string().as_str()),
        }
    }
}

impl From<wgpu::RequestDeviceError> for HasherError {
    fn from(value: wgpu::RequestDeviceError) -> Self {
        Self::WgpuRequestDeviceError(value)
    }
}

impl From<wgpu::PollError> for HasherError {
    fn from(value: wgpu::PollError) -> Self {
        Self::WgpuPollError(value)
    }
}

impl From<std::io::Error> for HasherError {
    fn from(value: std::io::Error) -> Self {
        Self::IoError(value)
    }
}
