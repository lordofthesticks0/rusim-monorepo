use fframes::{EncoderOptions, RenderOptions, StaticMediaProvider, cli};
use fframes_skia_renderer::{
    SkiaFFramesRenderer, SkiaPipelineConcurrencyPolicy, SkiaPipelineConfig, cli::SkiaRenderers,
    vulkan::SkiaVulkanCtx,
};
use std::process::ExitCode;
use video_demo::{HEIGHT, VideoDemoMedia, VideoDemoVideo, WIDTH};

fn main() -> ExitCode {
    let media = VideoDemoMedia::prepare().expect("media");
    let video = VideoDemoVideo::new();
    let gpu = SkiaVulkanCtx::new(WIDTH, HEIGHT).expect("GPU context");

    cli::new(
        &video,
        RenderOptions {
            media: Some(&media),
            video_encoder_options: EncoderOptions {
                preferred_encoder: Some("libx264"),
                codec_params: Some(&[("crf", "18"), ("preset", "slow"), ("tune", "animation")]),
                ..Default::default()
            },
            ..Default::default()
        },
    )
    // Frame previews (frame, strip, onion, inspect) render with the same Skia backend.
    .backend(
        SkiaFFramesRenderer::new_vulkan(
            &gpu,
            SkiaPipelineConfig {
                concurrency_policy: SkiaPipelineConcurrencyPolicy::MaxPerformance,
                ..Default::default()
            },
        )
        .expect("skia renderer"),
    )
    // `--renderer skia-cpu` renders with Skia on the CPU. Used for the final export on
    // this machine: the Intel Xe GPU driver hung and reset during a long Vulkan render.
    // Without the flag the GPU backend above is used.
    .renderers(SkiaRenderers {
        default: None,
        pipeline: SkiaPipelineConfig {
            concurrency_policy: SkiaPipelineConcurrencyPolicy::MaxPerformance,
            ..Default::default()
        },
    })
    // `preview` opens the real-time GPU player.
    .preview(fframes_native_player::cli_preview)
    .run()
}
