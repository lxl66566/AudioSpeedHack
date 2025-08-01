use hound::{SampleFormat, WavReader, WavSpec, WavWriter};
use rubato::{
    Resampler, SincFixedIn, SincInterpolationParameters, SincInterpolationType, WindowFunction,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // WAV 文件读取
    let input_path = "input.wav";
    let output_path = "output.wav";

    let mut reader = WavReader::open(input_path)?;
    let spec = reader.spec();
    let samples: Vec<i16> = reader.samples::<i16>().map(|s| s.unwrap()).collect();

    // 转换 WAV 样本到 f64
    let sample_rate_in = spec.sample_rate as f64;
    let sample_rate_out = sample_rate_in * 2.0; // 2 倍速

    let params = SincInterpolationParameters {
        sinc_len: 256,
        f_cutoff: 0.95,
        interpolation: SincInterpolationType::Linear,
        oversampling_factor: 256,
        window: WindowFunction::BlackmanHarris2,
    };

    let mut resampler =
        SincFixedIn::<f64>::new(sample_rate_in / sample_rate_out, 2.0, params, 1024, 2)?;

    let buffer_in = vec![
        samples
            .iter()
            .map(|&s| s as f64 / i16::MAX as f64)
            .collect::<Vec<f64>>();
        2
    ];
    let buffer_out = resampler.process(&buffer_in, None)?;

    // 转换 f64 样本到 WAV 样本
    let samples_out: Vec<i16> = buffer_out[0]
        .iter()
        .map(|&s| (s * i16::MAX as f64).round() as i16)
        .collect();

    // WAV 文件写入
    let spec_out = WavSpec {
        channels: spec.channels,
        sample_rate: sample_rate_out as u32,
        bits_per_sample: 16,
        sample_format: SampleFormat::Int,
    };

    let mut writer = WavWriter::create(output_path, spec_out)?;
    for &sample in &samples_out {
        writer.write_sample(sample)?;
    }

    Ok(())
}
