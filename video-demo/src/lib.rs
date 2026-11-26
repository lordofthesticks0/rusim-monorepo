//! Display walkthrough: a slow demo video of the `display-v9` LVGL touch UI.
//!
//! The footage is the real UI. `media/ui-*.png` are snapshots written by the LVGL PC
//! simulator (`[env:display-sim]`, `SIM_SHOTS=<dir>`, see `docs/display-sim.md`).
//! The video puts them on a drawn device and moves one continuous camera between the
//! parts of each screen. Every step of the walkthrough is one entry of `BEATS`: the
//! camera, the spotlight, the touch marker and the captions are all built from that
//! table in `VideoDemoVideo::new`, so changing a beat's length moves everything after it.
use fframes::{
    AnimateRuntimeInput, AudioMap, AudioTimestamp::*, AudioTrack, Color, Duration, FFramesContext, Frame, Scene, Scenes, Svgr,
    Video,
    animation::{AnimationRuntime, Easing, KeyFrame, KeyFramesAnimation},
    include_media_dir,
};
use std::sync::LazyLock;

// Every file of `media/` is embedded into the binary. Fonts are used by family name.
include_media_dir!(pub struct VideoDemoMedia, "media");

pub const WIDTH: usize = 1920;
pub const HEIGHT: usize = 1080;

// Colours. The light values are the display's own tokens from
// `firmware/display-v9/ui_config.h`, so the frame matches the screenshots.
/// Backdrop, a darker UI_COL_SCRIM (#2A313C). One flat colour on purpose: a soft
/// gradient or blur behind the device turns into visible bands and blocks after video
/// compression.
const STAGE: &str = "#0e1319";
const ACCENT_SOFT: &str = "#8cbde0"; // UI_COL_ACCENT lightened for the dark backdrop
const TEXT: &str = "#eef2f7"; // UI_COL_BG
const MUTED: &str = "#a3afbd";
const SHADE: &str = "#0a1017"; // spotlight dimming
/// LVGL's built-in fonts are Montserrat, so the captions use the same family.
const FONT: &str = "Montserrat";

// Geometry. Everything on the device is drawn in display pixels (800 x 480, the
// Sunton panel) and the camera maps display pixels to video pixels.
const UI_W: f32 = 800.0;
const UI_H: f32 = 480.0;
/// Bezel width around the panel, in display pixels.
const BEZEL: f32 = 16.0;
/// Display pixels to video pixels at zoom 1.
const BASE_SCALE: f32 = 1.5;
/// The device lives in the area above `STAGE_BOTTOM`; captions live below it.
const STAGE_CX: f32 = 960.0;
const STAGE_CY: f32 = 425.0;
const STAGE_BOTTOM: f32 = 850.0;
const STAGE_MARGIN: f32 = 40.0;
/// How far the camera follows the focus (0 = stays centred, 1 = centres the focus).
/// Sideways follow is small so the device never hugs one edge of the frame.
const PAN_X: f32 = 0.3;
const PAN_Y: f32 = 1.0;
/// Zoom levels. At `WIDE` the whole device fits the stage and the spotlight points at
/// the subject. `CLOSE` is for small targets: the device is clearly larger than the
/// frame, so the crop reads as a close-up. Values in between clip the device edge by a
/// few pixels, which looks like a mistake, so they are not used.
const TITLE_ZOOM: f32 = 0.9;
const WIDE: f32 = 1.0;
const CLOSE: f32 = 1.4;
/// Padding and corner radius of the spotlight around a focus rectangle.
const SPOT_PAD: f32 = 6.0;
const SPOT_RADIUS: f32 = 12.0;

// Timing in seconds. Slow on purpose: the viewer is learning the screens.
/// Camera and spotlight travel between two beats.
const GLIDE: f32 = 1.8;
/// One screenshot dissolving into the next.
const CROSSFADE: f32 = 0.9;
/// Zoom creep during a hold, so a still screen never looks frozen.
const DRIFT: f32 = 1.025;
/// Darkness of the spotlight outside the focus.
const DIM: f32 = 0.55;
/// `UI_HOLD_TO_CONFIRM_MS` (2000 ms): the real hold-to-confirm time.
const HOLD_SECONDS: f32 = 2.0;

// Sound effects in `media/`, synthesised with ffmpeg (see `docs/video-demo.md`).
const SFX_TAP: &str = "sfx-tap.wav";
const SFX_HOLD: &str = "sfx-hold.wav";
const SFX_ERROR: &str = "sfx-error.wav";
const SFX_WHOOSH: &str = "sfx-whoosh.wav";
const SFX_SLEEP: &str = "sfx-sleep.wav";
const SFX_WAKE: &str = "sfx-wake.wav";
const SFX_SWELL: &str = "sfx-swell.wav";
/// The whoosh file is loudest at its middle (0.6 s). Starting it 0.3 s into a glide puts
/// that peak at the glide's fastest point (half of `GLIDE`).
const WHOOSH_LEAD: f32 = GLIDE / 2.0 - 0.6;

static EASE_IN: Easing = Easing::EaseIn;
static EASE_OUT: Easing = Easing::EaseOut;
static EASE_IN_OUT: Easing = Easing::EaseInOut;
static LINEAR: Easing = Easing::Linear;

static CROSSFADE_RUNTIME: LazyLock<AnimationRuntime> =
    LazyLock::new(|| AnimationRuntime::new(CROSSFADE, &EASE_IN_OUT));

// Snapshots from the simulator. `06-bottle-missing` is left out: the simulator's preview
// data draws a trend chart for the absent bottle, which the real feed never does.
const SETUP: &str = "ui-01-setup.png";
const HOME: &str = "ui-02-home.png";
const PICKER: &str = "ui-03-data-picker.png";
const NODE: &str = "ui-04-node.png";
const BOTTLE: &str = "ui-05-bottle.png";
const CONTROL: &str = "ui-07-control.png";
const INFO: &str = "ui-08-info.png";
const MODAL: &str = "ui-09-modal.png";
const RUNNING: &str = "ui-10-home-running.png";

#[derive(Clone, Copy, Debug, PartialEq)]
enum Kind {
    /// Opening card: larger caption.
    Title,
    Step,
    /// The backlight turns off during this beat.
    Sleep,
    /// The backlight comes back and everything fades out.
    End,
}

/// What the finger does at the end of a beat. Coordinates are display pixels.
#[derive(Clone, Copy, Debug)]
enum Gesture {
    None,
    Tap(f32, f32),
    /// Press and hold for `HOLD_SECONDS`, like every binary toggle on the display.
    Hold(f32, f32),
}

/// One step of the walkthrough.
#[derive(Debug)]
struct Beat {
    /// Scene name in the CLI (`strip Overview`, `frame Data[1]@2s`).
    name: &'static str,
    kind: Kind,
    /// Small upper-case label above the caption.
    chapter: &'static str,
    screen: &'static str,
    seconds: f32,
    /// Spotlight rectangle `[x, y, w, h]` in display pixels; `None` lights everything.
    focus: Option<[f32; 4]>,
    zoom: f32,
    caption: &'static str,
    detail: &'static str,
    /// Performed at the end of the beat; the next beat shows its result.
    gesture: Gesture,
}

// Tap targets, read off the snapshots (display pixels).
const TAB_HOME: (f32, f32) = (58.0, 24.0);
const TAB_DATA: (f32, f32) = (164.0, 24.0);
const TAB_CONTROL: (f32, f32) = (270.0, 24.0);
const TAB_INFO: (f32, f32) = (376.0, 24.0);

const BEATS: &[Beat] = &[
    Beat {
        name: "Title",
        kind: Kind::Title,
        chapter: "IN-VITRO RUMEN SIMULATION",
        screen: SETUP,
        seconds: 7.0,
        focus: None,
        zoom: TITLE_ZOOM,
        caption: "How to use the display",
        detail: "From network login to a running experiment.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Login",
        kind: Kind::Step,
        chapter: "NETWORK LOGIN",
        screen: SETUP,
        seconds: 8.5,
        focus: Some([16.0, 104.0, 316.0, 316.0]),
        zoom: WIDE,
        caption: "First, log in to the network",
        detail: "Scan the code with your phone, then log in on the page that opens.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Login",
        kind: Kind::Step,
        chapter: "NETWORK LOGIN",
        screen: SETUP,
        seconds: 8.5,
        focus: Some([346.0, 96.0, 446.0, 262.0]),
        zoom: WIDE,
        caption: "This screen moves on by itself",
        detail: "The display never sees your username or password.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Overview",
        kind: Kind::Step,
        chapter: "OVERVIEW",
        screen: HOME,
        seconds: 8.0,
        focus: Some([8.0, 4.0, 418.0, 40.0]),
        zoom: CLOSE,
        caption: "Four tabs run along the top",
        detail: "Tap a tab, or swipe sideways, to change page.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Overview",
        kind: Kind::Step,
        chapter: "OVERVIEW",
        screen: HOME,
        seconds: 9.0,
        focus: Some([16.0, 104.0, 376.0, 360.0]),
        zoom: WIDE,
        caption: "The run card starts an experiment",
        detail: "Set the duration, then hold the round button for 2 seconds.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Overview",
        kind: Kind::Step,
        chapter: "OVERVIEW",
        screen: HOME,
        seconds: 8.0,
        focus: Some([408.0, 104.0, 376.0, 360.0]),
        zoom: WIDE,
        caption: "Status sums up the whole rig",
        detail: "Bottles online, missing readings, setpoint and equipment.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Overview",
        kind: Kind::Step,
        chapter: "OVERVIEW",
        screen: HOME,
        seconds: 9.0,
        focus: Some([552.0, 4.0, 240.0, 40.0]),
        zoom: CLOSE,
        caption: "Link state and alerts sit top right",
        detail: "Tap the chip for details, or the bell to list missing readings.",
        gesture: Gesture::Tap(TAB_DATA.0, TAB_DATA.1),
    },
    Beat {
        name: "Data",
        kind: Kind::Step,
        chapter: "DATA",
        screen: PICKER,
        seconds: 8.0,
        focus: Some([16.0, 104.0, 245.0, 172.0]),
        zoom: WIDE,
        caption: "Data lists the six nodes",
        detail: "Each node holds four bottles. Grey cards are not connected.",
        gesture: Gesture::Tap(138.0, 200.0),
    },
    Beat {
        name: "Data",
        kind: Kind::Step,
        chapter: "DATA",
        screen: NODE,
        seconds: 8.5,
        focus: Some([16.0, 148.0, 768.0, 252.0]),
        zoom: WIDE,
        caption: "Each bottle shows its temperature",
        detail: "Bottle 4 sends no data, so it is marked in red.",
        gesture: Gesture::Tap(106.0, 270.0),
    },
    Beat {
        name: "Data",
        kind: Kind::Step,
        chapter: "DATA",
        screen: BOTTLE,
        seconds: 8.0,
        focus: Some([16.0, 104.0, 768.0, 124.0]),
        zoom: WIDE,
        caption: "Five sensors per bottle",
        detail: "CO2, methane, pressure, pH and temperature.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Data",
        kind: Kind::Step,
        chapter: "DATA",
        screen: BOTTLE,
        seconds: 8.5,
        focus: Some([16.0, 244.0, 768.0, 220.0]),
        zoom: WIDE,
        caption: "Tap a sensor tile to chart it",
        detail: "The chart covers the last 12 minutes.",
        gesture: Gesture::Tap(TAB_CONTROL.0, TAB_CONTROL.1),
    },
    Beat {
        name: "Control",
        kind: Kind::Step,
        chapter: "CONTROL",
        screen: CONTROL,
        seconds: 9.0,
        focus: Some([16.0, 260.0, 376.0, 204.0]),
        zoom: WIDE,
        caption: "Set the experiment number and duration",
        detail: "Hold the minus or plus button to step faster.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Control",
        kind: Kind::Step,
        chapter: "CONTROL",
        screen: CONTROL,
        seconds: 9.0,
        focus: Some([408.0, 280.0, 376.0, 184.0]),
        zoom: WIDE,
        caption: "Switches need a 2 second hold",
        detail: "A short tap does nothing, so nothing switches by accident.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Control",
        kind: Kind::Step,
        chapter: "CONTROL",
        screen: CONTROL,
        seconds: 9.0,
        focus: Some([408.0, 104.0, 376.0, 170.0]),
        zoom: CLOSE,
        caption: "Hold the start button for 2 seconds",
        detail: "The ring fills while you hold.",
        gesture: Gesture::Hold(477.0, 194.0),
    },
    Beat {
        name: "Control",
        kind: Kind::Step,
        chapter: "CONTROL",
        screen: MODAL,
        seconds: 9.0,
        focus: Some([140.0, 128.0, 520.0, 224.0]),
        zoom: CLOSE,
        caption: "Without the database, the start is blocked",
        detail: "The message tells you what to check next.",
        gesture: Gesture::Tap(400.0, 308.0),
    },
    Beat {
        name: "Control",
        kind: Kind::Step,
        chapter: "CONTROL",
        screen: CONTROL,
        seconds: 6.5,
        focus: Some([326.0, 4.0, 100.0, 40.0]),
        zoom: CLOSE,
        caption: "Next, open the Info page",
        detail: "",
        gesture: Gesture::Tap(TAB_INFO.0, TAB_INFO.1),
    },
    Beat {
        name: "Info",
        kind: Kind::Step,
        chapter: "INFO",
        screen: INFO,
        seconds: 9.0,
        focus: Some([16.0, 104.0, 768.0, 172.0]),
        zoom: WIDE,
        caption: "Info shows the link and the database",
        detail: "Red values point to the problem. Here, the WT32 is not connected.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Info",
        kind: Kind::Step,
        chapter: "INFO",
        screen: INFO,
        seconds: 8.5,
        focus: Some([16.0, 292.0, 768.0, 172.0]),
        zoom: WIDE,
        caption: "Below: the run, upload queue and sensor feed",
        detail: "UI thread load shows how busy the display is.",
        gesture: Gesture::Tap(TAB_HOME.0, TAB_HOME.1),
    },
    Beat {
        name: "Running",
        kind: Kind::Step,
        chapter: "OVERVIEW",
        screen: RUNNING,
        seconds: 9.5,
        focus: Some([16.0, 104.0, 376.0, 360.0]),
        zoom: WIDE,
        caption: "During a run, Overview shows progress",
        detail: "Duration is locked. Hold the stop button for 2 seconds to end it.",
        gesture: Gesture::None,
    },
    Beat {
        name: "Sleep",
        kind: Kind::Sleep,
        chapter: "SLEEP",
        screen: RUNNING,
        seconds: 10.0,
        focus: None,
        zoom: WIDE,
        caption: "After 60 seconds without a touch, the screen sleeps",
        detail: "The first touch only wakes it, so nothing is pressed by accident.",
        gesture: Gesture::Tap(400.0, 300.0),
    },
    Beat {
        name: "End",
        kind: Kind::End,
        chapter: "IN-VITRO RUMEN SIMULATION",
        screen: RUNNING,
        seconds: 6.0,
        focus: None,
        zoom: TITLE_ZOOM,
        caption: "",
        detail: "",
        gesture: Gesture::None,
    },
];

/// One keyframe: `from => to` between `start` and `end` seconds.
fn kf<T: fframes::animation::Animatable>(
    start: f32,
    end: f32,
    from: T,
    to: T,
    easing: &'static Easing,
) -> KeyFrame<'static, T> {
    KeyFrame { start, end: Some(end), from, to, easing }
}

/// Camera for a beat: `(x, y)` is the display point placed at the stage centre.
fn camera_for(focus: Option<[f32; 4]>, zoom: f32) -> (f32, f32, f32) {
    let scale = BASE_SCALE * zoom;
    let (fx, fy) = match focus {
        Some([x, y, w, h]) => (x + w / 2.0, y + h / 2.0),
        None => (UI_W / 2.0, UI_H / 2.0),
    };
    let x = camera_axis(fx, PAN_X, UI_W, scale, STAGE_CX, WIDTH as f32);
    let y = camera_axis(fy, PAN_Y, UI_H, scale, STAGE_CY, STAGE_BOTTOM);
    (x, y, zoom)
}

/// One camera axis: follow the focus by `pan`, then clamp so the device stays inside the
/// stage when it fits, or keeps covering the stage when it is larger.
fn camera_axis(focus: f32, pan: f32, panel: f32, scale: f32, center: f32, extent: f32) -> f32 {
    let (lo, hi) = (-BEZEL, panel + BEZEL);
    let a = (center - (extent - STAGE_MARGIN)) / scale + hi;
    let b = (center - STAGE_MARGIN) / scale + lo;
    let target = panel / 2.0 + (focus - panel / 2.0) * pan;
    target.clamp(a.min(b), a.max(b))
}

/// Spotlight rectangle for a beat; the whole panel when nothing is in focus.
fn spot_for(focus: Option<[f32; 4]>) -> ((f32, f32, f32, f32), f32) {
    match focus {
        Some([x, y, w, h]) => {
            // Stay 2 px inside the panel so the outline is never cut by the panel edge.
            let x0 = (x - SPOT_PAD).max(2.0);
            let y0 = (y - SPOT_PAD).max(2.0);
            let x1 = (x + w + SPOT_PAD).min(UI_W - 2.0);
            let y1 = (y + h + SPOT_PAD).min(UI_H - 2.0);
            ((x0, y0, x1 - x0, y1 - y0), DIM)
        }
        None => ((0.0, 0.0, UI_W, UI_H), 0.0),
    }
}

/// A finger marker for one gesture, in global seconds.
#[derive(Debug)]
struct Touch {
    /// Drawn only inside this window.
    window: (f32, f32),
    position: KeyFramesAnimation<(f32, f32)>,
    opacity: KeyFramesAnimation<f32>,
    /// Scale of the fingertip: below 1 while pressed.
    press: KeyFramesAnimation<f32>,
    ripple_radius: KeyFramesAnimation<f32>,
    ripple_opacity: KeyFramesAnimation<f32>,
    /// Hold progress 0..1, only for `Gesture::Hold`.
    ring: Option<KeyFramesAnimation<f32>>,
}

/// `(appear, arrive, press)` seconds of a gesture that commits at `at`. A hold presses
/// `HOLD_SECONDS` before the commit, a tap 0.7 s before.
fn touch_times(hold: bool, at: f32) -> (f32, f32, f32) {
    let press = if hold { at - HOLD_SECONDS } else { at - 0.7 };
    let arrive = press - 0.5;
    (arrive - 1.6, arrive, press)
}

/// One sound effect at a global second. `pan` is -1 (left) to 1 (right).
#[derive(Debug)]
struct Cue {
    file: &'static str,
    at: f32,
    gain_db: f32,
    pan: f32,
}

/// Stereo position of a touch at display x, so a tap on the left is heard on the left.
fn pan_for(x: f32) -> f32 {
    (x / UI_W * 2.0 - 1.0) * 0.4
}

/// Every sound of the video, derived from `BEATS` and the same timing functions the
/// animation uses, so changing a beat's length moves its sounds with it.
fn cues(starts: &[f32], total: f32) -> Vec<Cue> {
    let cue = |file, at, gain_db, pan| Cue { file, at, gain_db, pan };
    let mut cues = vec![
        // The device fades in (`presence`) and out at the end.
        cue(SFX_SWELL, 0.3, -7.0, 0.0),
        cue(SFX_SWELL, total - 3.6, -10.0, 0.0),
    ];
    for (i, (beat, &start)) in BEATS.iter().zip(starts).enumerate() {
        let commit = start + beat.seconds;
        // A quiet whoosh under each camera glide.
        if start > 0.0 {
            cues.push(cue(SFX_WHOOSH, start + WHOOSH_LEAD, -16.0, 0.0));
        }
        match beat.gesture {
            Gesture::None => {}
            Gesture::Tap(x, _) => {
                let (_, _, press) = touch_times(false, commit);
                cues.push(cue(SFX_TAP, press + 0.1, -3.0, pan_for(x)));
            }
            Gesture::Hold(x, _) => {
                let (_, _, press) = touch_times(true, commit);
                cues.push(cue(SFX_TAP, press + 0.1, -3.0, pan_for(x)));
                // The rising tone runs while the ring fills.
                cues.push(cue(SFX_HOLD, press, -10.0, pan_for(x)));
                // The only hold leads to the "start blocked" message.
                let blocked = BEATS.get(i + 1).is_some_and(|next| next.screen == MODAL);
                cues.push(cue(if blocked { SFX_ERROR } else { SFX_TAP }, commit + 0.05, -5.0, 0.0));
            }
        }
        // Same seconds as `backlight_off` in `VideoDemoVideo::new`.
        match beat.kind {
            Kind::Sleep => cues.push(cue(SFX_SLEEP, start + 2.2, -8.0, 0.0)),
            Kind::End => cues.push(cue(SFX_WAKE, start + 0.1, -8.0, 0.0)),
            _ => {}
        }
    }
    cues
}

impl Touch {
    /// `at` is when the gesture commits and the next screen appears.
    fn new(gesture: Gesture, at: f32) -> Option<Self> {
        let (x, y, hold) = match gesture {
            Gesture::None => return None,
            Gesture::Tap(x, y) => (x, y, false),
            Gesture::Hold(x, y) => (x, y, true),
        };
        // The finger comes in from below right, slowly, and rests before it presses.
        let away = (x + 60.0, y + 80.0);
        let (appear, arrive, press_at) = touch_times(hold, at);
        let leave = at + 0.5;

        let release = if hold {
            kf(at + 0.05, at + 0.45, 0.84, 1.0, &EASE_OUT)
        } else {
            kf(press_at + 0.25, press_at + 0.65, 0.84, 1.0, &EASE_OUT)
        };
        let ripple_at = if hold { at - 0.05 } else { press_at + 0.15 };

        Some(Self {
            window: (appear, leave + 1.0),
            position: KeyFramesAnimation::new(vec![kf(appear, arrive, away, (x, y), &EASE_IN_OUT)]),
            opacity: KeyFramesAnimation::new(vec![
                kf(appear, appear + 0.8, 0.0, 1.0, &EASE_OUT),
                kf(leave, leave + 0.9, 1.0, 0.0, &EASE_IN_OUT),
            ]),
            press: KeyFramesAnimation::new(vec![
                kf(press_at, press_at + 0.25, 1.0, 0.84, &EASE_OUT),
                release,
            ]),
            ripple_radius: KeyFramesAnimation::new(vec![kf(
                ripple_at,
                ripple_at + 1.2,
                18.0,
                if hold { 58.0 } else { 46.0 },
                &EASE_OUT,
            )]),
            ripple_opacity: KeyFramesAnimation::new(vec![
                kf(ripple_at, ripple_at + 0.1, 0.0, 0.8, &EASE_OUT),
                kf(ripple_at + 0.1, ripple_at + 1.2, 0.8, 0.0, &EASE_OUT),
            ]),
            ring: hold.then(|| {
                KeyFramesAnimation::new(vec![kf(press_at, at, 0.0, 1.0, &LINEAR)])
            }),
        })
    }

    fn render<'a>(&'a self, frame: &Frame) -> Svgr<'a> {
        let (x, y) = frame.animate(&self.position);
        let press = frame.animate(&self.press);
        let ripple_radius = frame.animate(&self.ripple_radius);
        let ripple_opacity = frame.animate(&self.ripple_opacity);
        let progress = self.ring.as_ref().map_or(0.0, |ring| frame.animate(ring));

        // Hold progress, drawn the way the display fills the arc of a hold button.
        let circumference = std::f32::consts::TAU * 30.0;
        let ring = (progress > 0.001).then(|| {
            fframes::svgr!(<g transform="rotate(-90)" opacity={1.0 - ripple_opacity}>
                <circle r="30" fill="none" stroke="#ffffff" stroke-opacity="0.35" stroke-width="4" />
                <circle r="30" fill="none" stroke={STAGE} stroke-opacity="0.35" stroke-width="8"
                    stroke-dasharray={circumference} stroke-dashoffset={circumference * (1.0 - progress)} />
                <circle r="30" fill="none" stroke="#ffffff" stroke-width="4" stroke-linecap="round"
                    stroke-dasharray={circumference} stroke-dashoffset={circumference * (1.0 - progress)} />
            </g>)
        }).unwrap_or_default();

        fframes::svgr!(<g transform={format!("translate({x} {y})")} opacity={frame.animate(&self.opacity)}>
            <g opacity={ripple_opacity}>
                <circle r={ripple_radius} fill="none" stroke={STAGE} stroke-opacity="0.3" stroke-width="6" />
                <circle r={ripple_radius} fill="none" stroke="#ffffff" stroke-width="2.5" />
            </g>
            {ring}
            <g transform={format!("scale({press})")}>
                <circle r="18" fill="#1b2330" fill-opacity="0.3" stroke={STAGE} stroke-opacity="0.3" stroke-width="6" />
                <circle r="18" fill="none" stroke="#ffffff" stroke-width="2.5" />
                <circle r="5" fill="#ffffff" opacity="0.9" />
            </g>
        </g>)
    }
}

/// The caption of one beat. Scenes only draw captions; the device is drawn by the video
/// so the camera can move across beat boundaries without a cut.
#[derive(Debug)]
struct BeatScene {
    beat: &'static Beat,
    label: KeyFramesAnimation<f32>,
    caption: KeyFramesAnimation<f32>,
    caption_rise: KeyFramesAnimation<f32>,
    detail: KeyFramesAnimation<f32>,
    detail_rise: KeyFramesAnimation<f32>,
}

impl BeatScene {
    fn new(beat: &'static Beat, previous: Option<&Beat>, next: Option<&Beat>) -> Self {
        let d = beat.seconds;
        let (caption_in, label_in) = match beat.kind {
            Kind::Title => (1.8, 1.2),
            Kind::End => (1.4, 1.4),
            _ => (0.9, 0.5),
        };
        let out = d - 0.8;
        let fade = |start: f32| {
            KeyFramesAnimation::new(vec![
                kf(start, start + 0.9, 0.0, 1.0, &EASE_OUT),
                kf(out, out + 0.55, 1.0, 0.0, &EASE_IN),
            ])
        };
        let rise = |start: f32| KeyFramesAnimation::new(vec![kf(start, start + 1.4, 16.0, 0.0, &EASE_OUT)]);

        // The chapter label stays up while consecutive beats share it.
        let same_prev = previous.is_some_and(|p| p.chapter == beat.chapter);
        let same_next = next.is_some_and(|n| n.chapter == beat.chapter) && beat.kind != Kind::End;
        let mut label = Vec::new();
        if !same_prev {
            label.push(kf(label_in, label_in + 0.9, 0.0, 1.0, &EASE_OUT));
        }
        if !same_next {
            label.push(kf(out, out + 0.55, 1.0, 0.0, &EASE_IN));
        }
        if label.is_empty() {
            label.push(kf(0.0, d, 1.0, 1.0, &LINEAR));
        }

        Self {
            beat,
            label: KeyFramesAnimation::new(label),
            caption: fade(caption_in),
            caption_rise: rise(caption_in),
            detail: fade(caption_in + 0.35),
            detail_rise: rise(caption_in + 0.35),
        }
    }
}

impl Scene for BeatScene {
    fn duration(&self) -> Duration<'_> {
        Duration::Seconds(self.beat.seconds)
    }

    fn name(&self) -> &'static str {
        self.beat.name
    }

    fn render_frame<'a>(&'a self, frame: Frame, _ctx: &FFramesContext<'a, '_>) -> Svgr<'a> {
        let title = self.beat.kind == Kind::Title;
        let (caption_size, caption_weight, caption_y) = if title { (64, 600, 954) } else { (44, 500, 940) };
        let caption = (!self.beat.caption.is_empty()).then(|| {
            fframes::svgr!(<g opacity={frame.animate(&self.caption)}
                transform={format!("translate(0 {})", frame.animate(&self.caption_rise))}>
                <text x="960" y={caption_y} text-anchor="middle" font-family={FONT}
                    font-size={caption_size} font-weight={caption_weight} fill={TEXT}>{self.beat.caption}</text>
            </g>)
        }).unwrap_or_default();
        let detail = (!self.beat.detail.is_empty()).then(|| {
            fframes::svgr!(<g opacity={frame.animate(&self.detail)}
                transform={format!("translate(0 {})", frame.animate(&self.detail_rise))}>
                <text x="960" y={caption_y + 50} text-anchor="middle" font-family={FONT}
                    font-size="30" font-weight="500" fill={MUTED}>{self.beat.detail}</text>
            </g>)
        }).unwrap_or_default();

        fframes::svgr!(<g>
            <g opacity={frame.animate(&self.label)}>
                <text x="960" y="880" text-anchor="middle" font-family={FONT} font-size="28"
                    font-weight="600" letter-spacing="5" fill={ACCENT_SOFT}>{self.beat.chapter}</text>
            </g>
            {caption}
            {detail}
        </g>)
    }
}

pub struct VideoDemoVideo {
    scenes: Vec<BeatScene>,
    /// Global start second of every beat.
    starts: Vec<f32>,
    total: f32,
    /// `(x, y, zoom)`: display point at the stage centre and zoom.
    camera: KeyFramesAnimation<(f32, f32, f32)>,
    spot_rect: KeyFramesAnimation<(f32, f32, f32, f32)>,
    spot_dim: KeyFramesAnimation<f32>,
    /// Black over the panel: the backlight turning off and on.
    backlight_off: KeyFramesAnimation<f32>,
    /// Fade of the whole device at the start and end.
    presence: KeyFramesAnimation<f32>,
    touches: Vec<Touch>,
    cues: Vec<Cue>,
}

impl VideoDemoVideo {
    pub fn new() -> Self {
        let mut starts = Vec::with_capacity(BEATS.len());
        let mut t = 0.0;
        for beat in BEATS {
            starts.push(t);
            t += beat.seconds;
        }
        let total = t;

        // Camera and spotlight: glide to each beat's framing, then drift slowly.
        let mut camera = Vec::new();
        let mut spot_rect = Vec::new();
        let mut spot_dim = Vec::new();
        let mut previous_camera = camera_for(None, 0.84);
        let mut previous_spot = spot_for(None);
        for (beat, &start) in BEATS.iter().zip(&starts) {
            let framing = camera_for(beat.focus, beat.zoom);
            let drifted = (framing.0, framing.1, framing.2 * DRIFT);
            let glide = if start == 0.0 { beat.seconds * 0.6 } else { GLIDE };
            camera.push(kf(start, start + glide, previous_camera, framing, &EASE_IN_OUT));
            camera.push(kf(start + glide, start + beat.seconds, framing, drifted, &EASE_IN_OUT));
            previous_camera = drifted;

            let spot = spot_for(beat.focus);
            spot_rect.push(kf(start, start + GLIDE, previous_spot.0, spot.0, &EASE_IN_OUT));
            spot_dim.push(kf(start, start + GLIDE, previous_spot.1, spot.1, &EASE_IN_OUT));
            previous_spot = spot;
            // Before a tap the spotlight lifts while the finger comes in, so the tapped
            // element is not sitting in the dimmed part. A hold keeps it: there the
            // focus is the held button itself.
            if let Gesture::Tap(..) = beat.gesture {
                let (appear, arrive, _) = touch_times(false, start + beat.seconds);
                if spot.1 > 0.0 {
                    spot_dim.push(kf(appear, arrive, spot.1, 0.0, &EASE_IN_OUT));
                }
                previous_spot = (spot.0, 0.0);
            }
        }

        // The backlight goes dark during the Sleep beat and returns when the End beat
        // starts (the waking touch is at the end of Sleep).
        let mut backlight_off = Vec::new();
        for (beat, &start) in BEATS.iter().zip(&starts) {
            match beat.kind {
                Kind::Sleep => backlight_off.push(kf(start + 2.2, start + 4.8, 0.0, 0.97, &EASE_IN_OUT)),
                Kind::End => backlight_off.push(kf(start + 0.1, start + 1.4, 0.97, 0.0, &EASE_IN_OUT)),
                _ => {}
            }
        }
        if backlight_off.is_empty() {
            backlight_off.push(kf(0.0, total, 0.0, 0.0, &LINEAR));
        }

        let touches = BEATS
            .iter()
            .zip(&starts)
            .filter_map(|(beat, &start)| Touch::new(beat.gesture, start + beat.seconds))
            .collect();

        let cues = cues(&starts, total);

        let scenes = BEATS
            .iter()
            .enumerate()
            .map(|(i, beat)| BeatScene::new(beat, i.checked_sub(1).map(|p| &BEATS[p]), BEATS.get(i + 1)))
            .collect();

        Self {
            scenes,
            starts,
            total,
            camera: KeyFramesAnimation::new(camera),
            spot_rect: KeyFramesAnimation::new(spot_rect),
            spot_dim: KeyFramesAnimation::new(spot_dim),
            backlight_off: KeyFramesAnimation::new(backlight_off),
            presence: KeyFramesAnimation::new(vec![
                kf(0.2, 3.2, 0.0, 1.0, &EASE_OUT),
                kf(total - 3.2, total - 0.3, 1.0, 0.0, &EASE_IN_OUT),
            ]),
            touches,
            cues,
        }
    }

    /// Index of the beat that is on screen at `seconds`.
    fn beat_at(&self, seconds: f32) -> usize {
        self.starts.partition_point(|&s| s <= seconds).saturating_sub(1)
    }

    /// The snapshot of the current beat, dissolving over the previous one after a change.
    fn screens<'a>(&'a self, frame: &Frame, ctx: &FFramesContext<'a, '_>) -> Vec<Svgr<'a>> {
        let seconds = frame.seconds();
        let i = self.beat_at(seconds);
        let current = BEATS[i].screen;
        let mut layers = Vec::with_capacity(2);
        let changing = i > 0 && BEATS[i - 1].screen != current && seconds < self.starts[i] + CROSSFADE;
        if changing && let Some(previous) = ctx.get_image(BEATS[i - 1].screen) {
            layers.push(fframes::svgr!(<image href={previous.href()} x="0" y="0" width="800" height="480" />));
        }
        let opacity = if changing {
            frame.animate_runtime(AnimateRuntimeInput {
                on_second: self.starts[i],
                from: 0.0,
                to: 1.0,
                animation_runtime: &CROSSFADE_RUNTIME,
            })
        } else {
            1.0
        };
        if let Some(image) = ctx.get_image(current) {
            layers.push(fframes::svgr!(<image href={image.href()} x="0" y="0" width="800" height="480" opacity={opacity} />));
        }
        layers
    }

    /// Dims everything on the panel except the focus rectangle.
    fn spotlight<'a>(&'a self, frame: &Frame) -> Svgr<'a> {
        let dim = frame.animate(&self.spot_dim);
        if dim < 0.004 {
            return Svgr::empty();
        }
        let (x, y, w, h) = frame.animate(&self.spot_rect);
        let r = SPOT_RADIUS.min(w / 2.0).min(h / 2.0);
        let (x1, y1) = (x + w, y + h);
        // Panel rectangle with a rounded hole: `evenodd` leaves the hole undimmed.
        let path = format!(
            "M0 0H{UI_W}V{UI_H}H0Z M{a} {y}H{b}A{r} {r} 0 0 1 {x1} {c}V{d}A{r} {r} 0 0 1 {b} {y1}H{a}A{r} {r} 0 0 1 {x} {d}V{c}A{r} {r} 0 0 1 {a} {y}Z",
            a = x + r,
            b = x1 - r,
            c = y + r,
            d = y1 - r,
        );
        fframes::svgr!(<g>
            <path d={path} fill={SHADE} fill-rule="evenodd" opacity={dim} />
            <rect x={x} y={y} width={w} height={h} rx={r} fill="none" stroke={ACCENT_SOFT}
                stroke-width="2" opacity={dim / DIM} />
        </g>)
    }
}

impl Default for VideoDemoVideo {
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Debug for VideoDemoVideo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("VideoDemoVideo").field("seconds", &self.total).finish()
    }
}

impl Video for VideoDemoVideo {
    const FPS: usize = 30;
    const WIDTH: usize = WIDTH;
    const HEIGHT: usize = HEIGHT;
    const BACKGROUND_COLOR: Color = Color::BLACK;

    fn duration(&self) -> Duration<'_> {
        Duration::Auto
    }

    fn audio(&self) -> AudioMap<'_> {
        // Sound effects only: no music or voice-over was supplied.
        self.cues
            .iter()
            .map(|cue| AudioTrack::new(cue.file, Second(cue.at)..Eof).gain_db(cue.gain_db).pan(cue.pan))
            .collect()
    }

    fn define_scenes(&self) -> Scenes<'_> {
        Scenes::from(self.scenes.iter().map(|scene| scene as &dyn Scene).collect::<Vec<_>>())
    }

    fn render_frame<'a>(&'a self, frame: Frame, ctx: &FFramesContext<'a, '_>) -> Svgr<'a> {
        let (cx, cy, zoom) = frame.animate(&self.camera);
        let scale = BASE_SCALE * zoom;
        let camera = format!(
            "matrix({scale} 0 0 {scale} {} {})",
            STAGE_CX - cx * scale,
            STAGE_CY - cy * scale
        );
        let seconds = frame.seconds();
        let touches: Vec<Svgr> = self
            .touches
            .iter()
            .filter(|touch| seconds >= touch.window.0 && seconds < touch.window.1)
            .map(|touch| touch.render(&frame))
            .collect();

        fframes::svgr!(
            <svg xmlns="http://www.w3.org/2000/svg" width={WIDTH} height={HEIGHT} viewBox="0 0 1920 1080">
                <defs>
                    // The device fades out above the captions instead of running into them.
                    <linearGradient id="stage-fade" x1="0" y1="812" x2="0" y2="858" gradientUnits="userSpaceOnUse">
                        <stop offset="0" stop-color="#ffffff" />
                        <stop offset="1" stop-color="#000000" />
                    </linearGradient>
                    <mask id="stage" maskUnits="userSpaceOnUse" x="0" y="0" width="1920" height="1080">
                        <rect width="1920" height="1080" fill="url(#stage-fade)" />
                    </mask>
                    <clipPath id="panel">
                        <rect width="800" height="480" />
                    </clipPath>
                </defs>
                <rect width="1920" height="1080" fill={STAGE} />
                <g mask="url(#stage)" opacity={frame.animate(&self.presence)}>
                    <g transform={camera}>
                        <rect x="-16" y="-16" width="832" height="512" rx="22" fill="#07090c"
                            stroke="#2b333e" stroke-width="1" />
                        <rect x="-1" y="-1" width="802" height="482" fill="#000000" />
                        <g clip-path="url(#panel)">
                            {self.screens(&frame, ctx)}
                            <rect width="800" height="480" fill="#000000" opacity={frame.animate(&self.backlight_off)} />
                            {self.spotlight(&frame)}
                        </g>
                        {touches}
                    </g>
                </g>
                {ctx.render_scenes(&frame)}
            </svg>
        )
    }
}
