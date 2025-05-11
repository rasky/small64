use clap::Parser;

#[derive(Parser)]
pub struct Cli {
    /// Path to the source ROM file with IPL3 to be brute forced
    pub rom: std::path::PathBuf,

    /// Sign the source ROM file with found collision data
    #[arg(short = 's', long)]
    pub sign: bool,

    /// The CIC for which a checksum must be calculated
    #[arg(short = 'c', long, default_value("6102"), value_parser = cic_parser)]
    pub cic: (u8, u64),

    /// Y bits to use: 32-bit word indices and bit ranges (eg: 40[16..8],56[24..12]).
    #[arg(short = 'b', long, default_value("1022[31..0]"), value_parser = y_bits_parser)]
    pub y_bits: std::vec::Vec<u32>,

    /// The Y coordinate to start with
    #[arg(short = 'y', long, default_value("0"))]
    pub y_init: u32,

    /// The GPU to use (0 for first, 1 for second, etc.)
    #[arg(short = 'd', long, default_value("0"))]
    pub gpu_adapter: usize,

    /// The number of workgroups to use (x,y,z format, total threads = x*y*z*256)
    #[arg(short = 'w', long, default_value("256,256,256"), value_parser = workgroups_parser)]
    pub workgroups: (u32, u32, u32),

    /// The shader module to use
    #[arg(short = 'z', long, default_value("glsl"))]
    pub shader: ShaderType,
}

#[derive(Clone, clap::ValueEnum)]
pub enum ShaderType {
    Glsl,
    Wgsl,
}

fn u32_from_str(str: &str) -> Result<u32, String> {
    u32::from_str_radix(str, 10).map_err(|e| e.to_string())
}

fn cic_parser(str: &str) -> Result<(u8, u64), String> {
    let (seed, target_checksum) = match str {
        "6101" => (0x3F, 0x45CC73EE317A),
        "6102" | "7101" => (0x3F, 0xA536C0F1D859),
        "6103" | "7103" => (0x78, 0x586FD4709867),
        "6105" | "7105" => (0x91, 0x8618A45BC2D3),
        "6106" | "7106" => (0x85, 0x2BBAD4E6EB74),
        "8303" => (0xDD, 0x32B294E2AB90),
        "8401" => (0xDD, 0x6EE8D9E84970),
        "5167" => (0xDD, 0x083C6C77E0B1),
        "DDUS" => (0xDE, 0x05BA2EF0A5F1),
        _ => return Err(format!("Unknown CIC")),
    };
    Ok((seed, target_checksum))
}

fn y_bits_parser(str: &str) -> Result<Vec<u32>, String> {
    let slices: Vec<&str> = str.split(',').collect();

    let mut values = vec![];

    let mut push_y_bits = |index: u32, start: u32, end: u32| {
        for i in start..=end {
            values.push(((index - 16) * 32) + (31 - i));
        }
    };

    for slice in slices.iter() {
        let parts: Vec<&str> = slice.split('[').collect();

        if parts.len() == 0 {
            return Err(format!("empty Y bits index"));
        }

        let index = u32_from_str(parts[0])?;

        if (index <= 16) || (index >= 1023) {
            return Err(format!("invalid Y bits index: {index}"));
        }

        match parts.len() {
            1 => {
                push_y_bits(index, 0, 31);
            }
            2 => {
                let range_parts: Vec<&str> = parts[1].split("..").collect();

                if range_parts.len() != 2 {
                    return Err(format!("invalid Y bits range format for index {index}"));
                }

                let end = u32_from_str(range_parts[0])?;
                let start = u32_from_str(
                    range_parts[1]
                        .strip_suffix(']')
                        .ok_or(format!("invalid Y bits format for index {index}"))?,
                )?;

                if (start > end) || (end >= 32) {
                    return Err(format!(
                        "invalid Y bits range for index {index}: 0 < {start} <= {end} < 32"
                    ));
                }

                push_y_bits(index, start, end);
            }
            _ => return Err(format!("invalid Y bits format for index {index}")),
        }
    }

    values.sort();
    values.dedup();

    if values.len() > 32 {
        return Err(format!("too many Y bits: {} (max: 32)", values.len()));
    }

    Ok(values)
}

fn workgroups_parser(str: &str) -> Result<(u32, u32, u32), String> {
    let slices: Vec<&str> = str.split(',').collect();

    if slices.len() > 3 {
        return Err(format!("invalid workgroups format"));
    }

    let mut values = [1u32; 3];

    for (i, slice) in slices.iter().enumerate() {
        values[i] = u32_from_str(&slice)?;
    }

    Ok((values[0], values[1], values[2]))
}

pub fn parse() -> Cli {
    Cli::parse()
}
