mod cli;
mod cpu;
mod error;
mod gpu;
mod hasher;

fn print_round_execution_time(y: u32, time: std::time::Instant) {
    println!("Y={} took {:?}", y, time.elapsed());
}

fn run_hasher() -> Result<(), error::HasherError> {
    let cli::Cli {
        rom,
        sign,
        cic,
        y_bits,
        y_init,
        gpu_adapter,
        workgroups,
        shader,
    } = cli::parse();
    let (seed, target_checksum) = cic;

    let shader = match shader {
        cli::ShaderType::Glsl => gpu::GPUHasherShader::Glsl,
        cli::ShaderType::Wgsl => gpu::GPUHasherShader::Wgsl,
    };

    let mut hasher = hasher::Hasher::new(
        rom.clone().into(),
        gpu_adapter,
        workgroups,
        shader,
        seed,
        target_checksum,
        y_bits.clone(),
        y_init,
    )?;

    let gpu_info = hasher.get_gpu_info();

    println!(
        "GPU: \"{}\", backend: \"{}\"",
        gpu_info.name, gpu_info.backend
    );

    println!("Target seed and checksum: 0x{seed:02X} 0x{target_checksum:012X}");

    loop {
        let time = std::time::Instant::now();

        let y_current = hasher.get_y();

        match hasher.compute_round()? {
            hasher::HasherResult::Found(y, x) => {
                print_round_execution_time(y_current, time);
                println!("Found collision: Y={y:08X} X={x:08X}");
                if sign {
                    hasher::Hasher::sign_rom(rom.into(), y_bits, y, x)?;
                    println!("ROM has been successfully signed");
                }
                return Ok(());
            }
            hasher::HasherResult::Continue => {
                print_round_execution_time(y_current, time);
            }
            hasher::HasherResult::End => {
                break;
            }
        }
    }

    println!("Sorry nothing");

    Ok(())
}

fn main() {
    if let Err(error) = run_hasher() {
        println!("IPL3 hasher error: {error}");
    }
}
