use crate::error::HasherError;

pub enum GPUHasherShader {
    Wgsl,
    Glsl,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct GPUHasherInput {
    target_hi: u32,
    target_lo: u32,
    y_offset: u32,
    x_offset: u32,
    state: [u32; 16],
}

impl GPUHasherInput {
    fn new(target_checksum: u64, y_offset: u32, x_offset: u32, state: [u32; 16]) -> Self {
        Self {
            target_hi: ((target_checksum >> 32) & 0xFFFF) as u32,
            target_lo: (target_checksum & 0xFFFFFFFF) as u32,
            y_offset,
            x_offset,
            state,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct GPUHasherOutput {
    found: i32,
    x_result: u32,
}

impl GPUHasherOutput {
    fn get_result(&self) -> Option<u32> {
        if self.found != 0 {
            Some(self.x_result)
        } else {
            None
        }
    }
}

pub enum GPUHasherResult {
    Found(u32),
    Continue(u32),
    End,
}

pub struct GPUHasher {
    adapter: wgpu::Adapter,
    device: wgpu::Device,
    queue: wgpu::Queue,
    output_buffer: wgpu::Buffer,
    download_buffer: wgpu::Buffer,
    bind_group: wgpu::BindGroup,
    compute_pipeline: wgpu::ComputePipeline,
    workgroups: (u32, u32, u32),
}

impl GPUHasher {
    const LOCAL_WORKGROUP_SIZE: u32 = 256;
    const ENTRY_POINT: &str = "main";

    pub fn list_gpu_adapters() -> Vec<wgpu::Adapter> {
        wgpu::Instance::new(&wgpu::InstanceDescriptor::default())
            .enumerate_adapters(wgpu::Backends::all())
    }

    pub fn get_gpu_info(&self) -> wgpu::AdapterInfo {
        self.adapter.get_info()
    }

    pub fn new(
        adapter: wgpu::Adapter,
        shader: GPUHasherShader,
        workgroups: (u32, u32, u32),
    ) -> Result<Self, HasherError> {
        let (device, queue) =
            pollster::block_on(adapter.request_device(&wgpu::wgt::DeviceDescriptor {
                label: None,
                required_features: wgpu::Features::PUSH_CONSTANTS | wgpu::Features::SHADER_INT64,
                required_limits: wgpu::Limits {
                    max_push_constant_size: 128,
                    ..wgpu::Limits::downlevel_defaults()
                },
                memory_hints: wgpu::MemoryHints::Performance,
                trace: wgpu::Trace::Off,
            }))?;

        let shader_module_descriptor = match shader {
            GPUHasherShader::Wgsl => wgpu::include_wgsl!("shaders/hasher.wgsl"),
            GPUHasherShader::Glsl => wgpu::include_spirv!("shaders/hasher.spv"),
        };

        let shader_module = unsafe {
            device.create_shader_module_trusted(
                shader_module_descriptor,
                wgpu::ShaderRuntimeChecks::unchecked(),
            )
        };

        let output_buffer = device.create_buffer(&wgpu::wgt::BufferDescriptor {
            label: None,
            size: std::mem::size_of::<GPUHasherOutput>() as wgpu::BufferAddress,
            usage: wgpu::BufferUsages::STORAGE | wgpu::BufferUsages::COPY_SRC,
            mapped_at_creation: false,
        });

        let download_buffer = device.create_buffer(&wgpu::wgt::BufferDescriptor {
            label: None,
            size: std::mem::size_of::<GPUHasherOutput>() as wgpu::BufferAddress,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });

        let bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: None,
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::COMPUTE,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Storage { read_only: false },
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

        let bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: None,
            layout: &bind_group_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: output_buffer.as_entire_binding(),
            }],
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: None,
            bind_group_layouts: &[&bind_group_layout],
            push_constant_ranges: &[wgpu::PushConstantRange {
                range: (0..std::mem::size_of::<GPUHasherInput>() as u32),
                stages: wgpu::ShaderStages::COMPUTE,
            }],
        });

        let compute_pipeline = device.create_compute_pipeline(&wgpu::ComputePipelineDescriptor {
            label: None,
            layout: Some(&pipeline_layout),
            module: &shader_module,
            entry_point: Some(Self::ENTRY_POINT),
            compilation_options: wgpu::PipelineCompilationOptions::default(),
            cache: None,
        });

        Ok(Self {
            adapter,
            device,
            queue,
            output_buffer,
            download_buffer,
            bind_group,
            compute_pipeline,
            workgroups,
        })
    }

    pub fn x_round(
        &mut self,
        target_checksum: u64,
        y_offset: u32,
        x_offset: u32,
        initial_state: [u32; 16],
    ) -> Result<GPUHasherResult, HasherError> {
        let (wx, wy, wz) = self.workgroups;

        let mut command_encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor { label: None });

        {
            let mut compute_pass =
                command_encoder.begin_compute_pass(&wgpu::ComputePassDescriptor {
                    label: None,
                    timestamp_writes: None,
                });
            compute_pass.set_pipeline(&self.compute_pipeline);
            compute_pass.set_bind_group(0, &self.bind_group, &[]);
            compute_pass.set_push_constants(
                0,
                bytemuck::bytes_of(&GPUHasherInput::new(
                    target_checksum,
                    y_offset,
                    x_offset,
                    initial_state,
                )),
            );
            compute_pass.dispatch_workgroups(wx, wy, wz);
        }

        command_encoder.copy_buffer_to_buffer(
            &self.output_buffer,
            0,
            &self.download_buffer,
            0,
            self.output_buffer.size(),
        );

        let command_buffer = command_encoder.finish();

        self.queue.submit([command_buffer]);

        let buffer_slice = self.download_buffer.slice(..);

        buffer_slice.map_async(wgpu::MapMode::Read, |_| {});

        self.device.poll(wgpu::PollType::Wait)?;

        let result = *bytemuck::from_bytes::<GPUHasherOutput>(&buffer_slice.get_mapped_range());

        self.download_buffer.unmap();

        Ok(match result.get_result() {
            None => {
                let x_step = wx as u64 * wy as u64 * wz as u64 * Self::LOCAL_WORKGROUP_SIZE as u64;
                if x_offset as u64 + x_step > u32::MAX as u64 {
                    GPUHasherResult::End
                } else {
                    GPUHasherResult::Continue(x_step as u32)
                }
            }
            Some(x) => GPUHasherResult::Found(x),
        })
    }
}
