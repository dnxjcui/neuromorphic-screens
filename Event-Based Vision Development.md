**TL;DR:** _Traditional screen capture grabs full frames (e.g. via NVIDIA’s Capture SDK or Desktop Duplication API),
which is costly when most pixels are unchanged. An event-based approach only records pixel-level changes (like
neuromorphic “DVS” cameras that log brightness changes instead of full images ). This yields far less data for
static or slowly-changing screens and can cut processing overhead dramatically. No mainstream “event-based
screen capture” tool exists yet, but related methods include video delta compression (VNC/RDP protocols, video
codecs) and research tools that convert frame video to neuromorphic event streams. We can implement
an event-based capture in C++ on Windows (easiest) by using the Desktop Duplication API or NVIDIA’s GPU capture
APIs to get changed regions , then output timestamped events (x, y, change) instead of full images. We’ll store
these events in formats similar to neuromorphic cameras (a time-ordered stream of pixel changes) to enable
efficient storage, 1:1 replay, and feeding into AI models. Simple demos (like event-based screen recording/replay)
and advanced applications (low-latency AI agents that react only to changes) show the benefits. Pros: Minimizes
redundant frames, lowers bandwidth/storage, and can improve latency for AI perception. Cons: If the screen is
very dynamic (e.g. video/games), event output approaches frame-rate data sizes; plus, using events in AI requires
new algorithms or reconstruction of frames. Overall, an event-driven screen capture is feasible with existing
APIs and research insights, promising for low-cost, real-time AI agents, but it will require careful engineering and
adoption of event-based vision techniques._

# 1. Existing Methods & Analogous Tools

**Frame-Based Screen Capture (Status Quo):** Current screen capture SDKs grab full frames at fixed
intervals. For example, NVIDIA’s Capture SDK (via NVFBC/NVIFR) and Windows’ Desktop Duplication API both
provide a complete framebuffer image each cycle. This ensures fidelity but means processing
millions of pixels even if only a few changed. NVIDIA’s NVFBC is highly optimized (capturing directly from the
GPU’s framebuffer and even using the GPU’s NVENC encoder) to minimize CPU overhead. It can stream
1080p/4K at 60 FPS with low latency by avoiding expensive GPU→CPU→GPU data transfers. The
Windows DXGI Desktop Duplication API similarly gives you a IDXGIOutputDuplication object per
monitor, from which you can AcquireNextFrame() at up to the display refresh rate. These APIs are battle-
tested for remote desktop, game streaming, etc., but they are fundamentally frame-oriented. The overhead
of full frames is **wasteful if the screen content is largely static** , and it incurs unnecessary processing for
AI agents that only need the _changes_.

**Delta/Region-Based Capture:** To mitigate redundant data, some systems track **dirty regions** between
frames. Notably, Windows’ Desktop Duplication API provides metadata: lists of _dirty rectangles_ (areas that
changed since last frame) and _move rectangles_ (blocks of pixels moved by the OS, e.g. a window drag)

. In fact, GetFrameDirtyRects returns non-overlapping rects for all updated screen regions. This
is analogous to video compression (where inter-frames encode only differences) and to remote desktop
protocols. For example, VNC and RDP only transmit areas that changed, sometimes even at the GDI call
level. The benefit is obvious: if 90% of the screen is unchanged, we only need to handle the 10% that
updated. Using dirty regions can **greatly reduce bandwidth and CPU use** – Microsoft notes that
sending move operations and dirty rects instead of full pixels cuts data for slow links. NVIDIA’s Capture
SDK even includes an optional _difference map_ to highlight changed blocks between frames. NVFBC can
output a bitmask of which blocks (down to 16×16 pixels) have changed frame-to-frame. This
hardware-accelerated diff map assists in quickly identifying update regions without scanning the entire
image. **Limitations:** These delta-based methods still operate on a _frame clock_ : changes are reported per

```
1
```
```
2 3
```
```
4
```
```
1
```
```
5 4
```
```
5
6 7
```
```
8
```
```
4
9 4
```
```
10
11
```
```
12 13
```

frame refresh. They won’t capture asynchronous mid-frame updates (which for a standard display rarely
matter, since the OS presents at vsync). Also, dirty rectangles may be coalesced if too many changes occur
, possibly marking a slightly larger area than the exact changed pixels – this simplifies rendering but
could introduce minor “false positive” events (which we can correct by finer diffing within that region if
needed).

**Event Cameras and Frame-to-Event Converters:** Our approach is inspired by **neuromorphic event
cameras** , which _emit asynchronous events for pixel brightness changes instead of outputting full frames_.
Each event encodes that a pixel at (x,y) location got brighter or darker at a given timestamp. Event-based
vision has huge advantages in dynamic range, negligible motion blur, and microsecond latency. For
physical cameras, these sensors and algorithms are a maturing field (with surveys and dedicated CVPR
workshops). While we don’t have a physical “screen event sensor,” researchers have built **event simulators**
that convert conventional video into event streams. For instance, **Pix2NVS (2017)** proposed a parameterized
conversion of frame video to neuromorphic vision streams. More recently, **v2e (Video to Events, 2021)**
and similar works generate realistic DVS events from standard video inputs. These tools take a
sequence of frames and output a list of events (with timestamps interpolated between frames according to
intensity changes). They essentially do what we need – detect pixel changes and output events – but are
typically used offline for data augmentation or benchmarking algorithms, not for live screen capture.
Nevertheless, they demonstrate that it’s feasible to represent visual changes as an event stream. **Benefits:**
Only changes above a threshold produce events, dramatically cutting data in static scenes. **Drawbacks:** If
changes are continuous (e.g. a video with every pixel changing gradually), these methods can still produce a
flood of events – essentially as many as the frame-based approach, just encoded differently. Also, most such
simulators assume grayscale intensity changes and require setting a change threshold (∆I) to trigger
events, which might need tuning for screen content. No turn-key “event-based screen recorder” exists yet to
our knowledge, but these research efforts and the delta mechanisms above form the toolkit we can build
upon.

**Other Related Tools:** It’s worth noting that **video codecs and remote desktop systems** have long
exploited “events” in a broad sense: H.264, for example, doesn’t send full frames each time – it sends _I-
frames_ occasionally and lots of _P-frames_ that encode motion vectors and residuals (which areas changed)
between frames. This is a _temporal compression_ akin to event-based updates, though codecs work in blocks
and still output at fixed frame intervals. Similarly, protocols like RDP can operate by sending drawing
commands or only updated pixels for portions of the screen. These approaches confirm that ignoring
unchanged pixels is key to efficiency. However, they are geared toward human viewing (with lossy
compression, etc.) rather than providing a raw event feed for an AI model. Our goal is to capture **exact
pixel-change events with precise timing** to feed into algorithms, rather than compress them for display.

**Summary of Existing Solutions:** In short, standard solutions (NVIDIA/Windows capture) give full frames
(heavy but simple), while some systems and research offer _change-driven outputs_ (dirty region metadata, diff
maps, or simulated event streams). Each has pros/cons. Frame-based methods are widely supported and
easy to feed into traditional vision algorithms, but carry a lot of redundant data. Event/delta-based methods
slash redundant information and latency (no need to wait or transmit unchanged pixels) , but they
challenge us to develop new handling and learning algorithms that can work on asynchronous sparse data

. The **NVIDIA Capture SDK** itself hints at hybrid possibilities: it can be set to output a diff-map and even
has an event callback for cursor updates , showing that capturing just changes was considered for
optimization. We aim to extend this idea to _all_ pixel changes, effectively treating the screen like an event
camera.

```
14
```
```
1
```
```
1
```
```
2
15
```
```
3
```
```
11
```
```
16
17
```

# 2. Design & Tools for Event‑Based Screen Capture

**Platform & API Choice (Windows vs. Linux):** We’ll start with **Windows** , as it provides an officially
supported path to capture the desktop and its changes. The DXGI Desktop Duplication API (available from
Windows 8 onward) is straightforward and efficient: it lets us grab the latest desktop frame as a GPU texture
and retrieve lists of updated regions since the last frame. This is ideal for implementing event capture,
because we don’t have to manually diff the entire screen – the OS already tells us _where_ changes occurred.
In C++, we can use IDXGIOutputDuplication::AcquireNextFrame (with a short timeout) in a loop to
get frames as they become available (e.g. ~60 Hz). The DXGI_OUTDUPL_FRAME_INFO returned includes a
timestamp and metadata size, and we then call GetFrameMoveRects and GetFrameDirtyRects to get
arrays of moved and dirty rectangles. Processing these, we can identify all pixels that have changed.
In many cases, using the dirty rects directly is enough (each represents an area of change). For more
precision, we can map the new frame texture to system memory (or copy just the dirty rect subregions) and
compare pixel-by-pixel with the previous frame’s pixels in those areas. This yields the exact set of pixel
changes (each with an old value and new value, effectively). On **Linux** , a similar approach is possible but
involves different tools – e.g. X11’s Damage extension provides events for regions of a window/screen that
changed (VNC uses this), and one can use XShmGetImage or DRM APIs to retrieve pixel data. Linux also has
NVIDIA’s Capture SDK available (as used in some high-performance streaming solutions ). However,
implementing this on Windows is likely **easiest** due to the well-documented DXGI API and wide usage.
Linux solutions may require more low-level work or third-party libraries (e.g. GStreamer plugins using X
or NVFBC as in that Witcher streaming example ). We will primarily focus on Windows first, leveraging
NVIDIA GPUs for acceleration, and note that Linux can be tackled similarly (possibly via NVIDIA’s API or
XDamage) later. Windows’ method is vendor-agnostic and will work on NVIDIA, AMD, or Intel GPUs; since we
“stick with NVIDIA” for performance, we also have the option to use NVIDIA’s proprietary path (discussed
next).

**Using NVIDIA GPU Capture Features:** With an NVIDIA GPU, we can choose to use the **NVIDIA Capture
SDK (NVFBC)** directly. NVFBC is known to be the lowest-latency way to grab the desktop on supported
GPUs, as it operates asynchronously to rendering and uses the GPU’s copy engines. It’s essentially a
driver-level screen grab that can copy the framebuffer or even encode it to H.264 on the fly. For our
purposes, NVFBC offers a couple of useful features: (1) It can capture frames to a CUDA buffer or system
memory without involving GDI or DWM, which can be faster. (2) Crucially, it supports a **DiffMap** mode – we
can request a _frame-to-frame difference map_ , at configurable block granularity, alongside each captured
frame. For example, we can set bDiffMap = TRUE and eDiffMapBlockSize =
NVFBC_DIFFMAP_BLOCKSIZE_16X16 (if the driver supports 16×16 blocks). Then, every time we grab a
frame, NVFBC provides a buffer indicating which 16×16 regions of the screen have changed since the last
grab. This is extremely helpful to pinpoint changes without scanning the whole image. We could use
16×16 or 32×32 blocks as a compromise between granularity and diff-map size. If even finer detail is
needed, we’d still do a pixel-level check within changed blocks (similar to dirty rect handling). It’s worth
noting that NVFBC can also overlay the hardware cursor separately (and even signal an event when the
cursor image changes ), which is useful if we want to treat the cursor movement as events distinct from
pixel changes beneath it. The **downside** of NVFBC is that official access may be limited on consumer GPUs –
NVIDIA historically restricts NVFBC to Quadro/RTX Enterprise cards or certain driver versions. There are
unofficial patches to enable it on GeForce, but if that’s not viable, the Desktop Duplication API is the fallback
(which, while slightly higher overhead, is still quite efficient and does use the GPU for the initial frame copy).

```
4
```
```
18 19
```
```
20
```
```
20
```
```
21
```
```
13 12
12
```
```
13
```
```
17
```

**Data Structure for Events:** Instead of storing full images, we will record **events** representing pixel
changes. Following the paradigm of neuromorphic cameras, an event could be defined as a tuple like _(t, x, y,
Δ)_ , where _t_ is the timestamp and _(x,y)_ is the pixel coordinate. For Δ (the change data), there are a few design
choices:

```
In a minimal DVS-like approach , we would store just the polarity of brightness change (e.g. +1 for
an increase, -1 for a decrease in intensity). This mirrors real event cameras which don’t report
absolute intensity, only that a threshold change occurred. To do this on a color screen, we might
first convert the screen to grayscale (luminance) and then generate events when the grayscale value
changes by more than a set threshold. Each event would then be (t, x, y, polarity). This yields the
sparsest representation but loses color information and fine magnitude info. Reconstructing the
exact image would then require algorithms (like event-based video reconstruction) or at least
knowledge of the last known intensity at that pixel. The benefit is that it’s directly compatible with
many neuromorphic algorithms and saves maximum bandwidth.
```
```
In a pragmatic approach , we could store more information per event, such as the new pixel value
(and maybe old value) for that location. For instance, an event could be (t, x, y, newColor). This way,
replay is trivial: you start from an initial frame and at time t set pixel (x,y) to newColor. This
ensures 100% fidelity reconstruction and avoids needing special reconstruction networks. The trade-
off is larger event size (e.g. 1 byte each for R,G,B plus maybe one for alpha or just 3 bytes color,
versus a single bit of sign in the pure DVS style). Even this is still far less data than storing full frames
if relatively few pixels change most of the time. We essentially treat a screen update as a collection of
“set pixel to X” events. This is similar to how some graphical diff tools or even GIF animations work
(which often just update changed pixels between frames). It does diverge from the neuromorphic
convention, but note that event camera research is already tackling color event cameras and video
reconstruction from events in color. In fact, recent work EVREAL provides tools for color event-
based video reconstruction, implying that storing color events is within scope of current methods
(they added “codes for color reconstruction” to their toolkit).
```
We could choose a hybrid: e.g. store polarity events for intensity but do it separately for RGB channels or
convert to luminance for event generation but also log some color info occasionally. However, to keep
things consistent, either pure intensity events (with separate handling for how to recover color) or direct
color events are preferable. Given the goal of **mirroring existing event-camera techniques** , starting with
intensity-only events might make sense; many event-based algorithms expect that format. We would then
have to handle color by perhaps running three event streams (one per channel) or just ignoring color
changes beyond their effect on intensity. This is a design decision to iterate on – perhaps start intensity-only
(which is simpler for algorithm testing), then extend to color once basic pipeline works.

For **timestamping** , we will use high-resolution timers. Desktop Duplication provides a timestamp (likely
based on the graphics system’s timing or a QPC value at frame presentation). NVFBC’s frames can be
timestamped using QueryPerformanceCounter or similar when the frame is captured. Either way, we’ll
assign each event an absolute or relative time. Event cameras often use microseconds resolution; here,
since display refresh is the limiting factor, millisecond precision is fine (a 60 Hz frame is ~16.67 ms apart,
though we could timestamp to the microsecond if the API gives it). The **order** of events will follow the order
of pixel updates within each frame interval. Note that if a large region changes exactly at once (say a new
frame of a video), we’ll get many pixel events with basically the same timestamp (or within that 16ms
window). That’s expected behavior (similarly, a real event camera pointed at a monitor showing a new

## •

```
1
```
## •

```
22
```
```
23
```

image would output a burst of simultaneous events). We will need to handle potentially _many events per
frame_ in such cases, but at least they’re all triggered only on actual changes.

**Minimizing Latency & Overhead:** We aim for _real-time capture_ and replay. To ensure we can capture and
output events at 60 FPS (or whatever the screen is doing) without frame drops, we must keep the
processing lightweight. C++ is appropriate for performance; additionally, we can leverage the GPU where
possible. For example, if using Desktop Duplication, the API hands us a DXGI ID3D11Texture2D
containing the new frame. We can write a compute shader or use DirectCompute/OpenCL/CUDA to
compare this texture with the previous frame’s texture entirely on the GPU, producing a list of changed pixel
coordinates. In practice, since the API gives us dirty rects, it’s probably efficient enough to copy just those
rects to CPU and process there. But for very high resolutions or higher refresh rates, a GPU diff could help.
NVIDIA’s diff-map in NVFBC offloads the change detection to the GPU hardware, which is ideal. We might
still need to fetch those changed pixel values to output the events, but we can use the diff map to limit what
we copy from GPU memory. For instance, if we know only 5% of 4K pixels changed, we could copy just those
in small chunks. This design prevents the GPU->CPU transfer from becoming a bottleneck, preserving low
latency. The **blog example** we saw demonstrated that copying full frames to CPU at 60 FPS consumed ~25%
of frame time for 1080p ; by doing it smarter with GPU assistance, that overhead can be cut significantly
.

**Data Storage Format:** To mirror neuromorphic techniques, we should store events in a standard or at least
convenient format. One common format in academic event-camera datasets is the **AEDAT** file (used by
iniVation’s DVS sensors), which logs each event as binary data (timestamps and addresses). Another is a
simple CSV or text log of events (timestamp, x, y, polarity). For performance, a binary format is preferred for
large event streams. EVREAL mentions using a _NumPy memmap_ format for event data , which suggests
storing events as a structured array on disk that can be memory-mapped for training. We might output a
stream and later convert it to whatever format needed (e.g. feed into Python for analysis via NumPy). The
key is that the data will be a _sequence of events in time_. We also need to decide how to handle an initial frame
state. In an event camera, you don’t get an absolute image, just changes from some unknown initial scene.
In our case, since we control the system, we can capture one initial full frame image at time t=0 (this is like a
synthetic “I-frame” that provides baseline intensities). After that, we log only events. This initial frame could
even be all black (and then the first events will effectively paint the first real image), but capturing the real
first frame ensures we start from the actual screen state. For replay or for feeding into algorithms that
require absolute intensities occasionally, having that initial frame is helpful. It’s analogous to event camera
algorithms that often assume a starting image or otherwise have to estimate one.

**Implementing the Replay (1:1 Visualization):** The first deliverable is to visualize a _replayed video_ from the
event log, ensuring the playback matches the original motion (1:1 in time and content). With our events +
initial frame, this is straightforward: we start at time 0 with the initial frame, then iterate through events in
chronological order, applying each change to a frame buffer. If multiple events share the same timestamp
(they will, per frame), we can apply them together before the next time step. We can throttle the replay by
timestamp differences to get the correct FPS – e.g. if an event occurred 16ms after the previous, we wait
16ms in the replay. Alternatively, we record the actual real-time intervals and use those. If our capture was
real-time, it effectively recorded ~60 events bursts per second for active periods, so replaying with those
timestamps should yield ~60 Hz motion. One thing we’ll monitor is **jitter** : if capturing or processing was
slow at any point, we might see irregular timestamp gaps. Part of the engineering task will be to measure
the capture loop’s performance to ensure we consistently hit the frame rate. In testing, we might log how
long each AcquireNextFrame + processing took. The Desktop Duplication API allows a timeout; we’ll use a

```
24
5
```
```
25
```

short timeout (e.g. if no new frame in e.g. 16ms, we loop – though typically it waits for vsync). As long as we
don’t block too long and the GPU isn’t overburdened, we should capture every display refresh update. The
result is that our event stream has effectively the same temporal resolution as the display.

Importantly, if the screen doesn’t change for 5 seconds, our event stream will contain **no events for 5
seconds** (aside from maybe a periodic frame with zero dirty rects which we could even omit). On replay,
nothing would change for that interval, which is exactly the desired behavior (contrast this with a normal
video which might still output identical frames or require a pause in playback). This sparsity is a big win for
storage and for an AI agent – no processing wasted when screen is idle.

**Libraries & Tools:** We plan to use **C++** with DirectX 11 for Windows capturing. OpenCV could be utilized for
convenience (e.g. converting DXGI texture to cv::Mat for diffing), but to maximize performance we might
bypass OpenCV and use DirectX APIs and perhaps some custom shaders. If we need to do image
processing (like converting to grayscale or thresholding), OpenCV or even NVIDIA’s CUDA libraries could
help. There’s also the possibility of integrating with existing event camera software: for example, if we
output events in a format compatible with the **event-based vision community** (like using the RPG’s _event
camera ROS driver_ format or the **Prophesee** SDK format), we could leverage their visualization or processing
tools. Given the scope, writing a simple renderer ourselves (to display events or reconstruct frames) is not
too difficult.

For storing and subsequent AI use: we might output a file per session (containing the initial frame and then
the event list). The **UZH RPG event camera resources** list provides many converters and dataset formats;
we could follow one of those so that downstream researchers can use their existing loaders. For instance,
the **Event Camera Dataset (ECD)** from UZH uses text files with one event per line or binary files – EVREAL
can convert various formats. Sticking to a common format will “incentivize people to develop models”
since they can reuse our data easily.

**Downstream AI/ML Considerations:** We need to keep in mind how AI agents will consume this event data.
Traditional deep learning models expect frame-based input (images or video), so feeding raw events will
require either **reconstructing frames** or using specialized models. There are already deep learning models
that operate on event streams – e.g. Spiking Neural Networks or tailored CNNs that accept event voxels
(events accumulated in a time window). For example, EVREAL’s benchmark includes _object detection and
classification_ tasks done on event-based inputs. These typically involve first converting a burst of events
into an image-like representation (such as an “event frame” – an image where pixel intensity represents
number of events or time of last event within a recent interval). Another approach is **event-by-event
processing** , where the model updates its state with each event (this is what true neuromorphic processors
do, and some RNN or SNN-based models emulate that). Our event capture pipeline should remain flexible
for either approach: we store precise timestamped events, which can later be binned or integrated into
frames if needed for a given model, or fed one-by-one to a spiking neural network. The key is that by
capturing **only changes** , we drastically reduce the amount of data an AI has to sift through when nothing is
happening on screen. This aligns with neuromorphic design principles that aim to minimize computation –
as one article puts it, event cameras allow robots to navigate while _minimizing computational costs_ by not
processing redundant frames. We expect similar benefits for screen-based AI agents: their perception
module can remain mostly idle during static periods and only wake up for meaningful changes.

**Cross-Platform Extension:** While focusing on Windows/NVIDIA now, the eventual goal can include Linux
support (using e.g. NVIDIA’s Capture SDK on Linux or X11/Wayland APIs). On Linux, one could use the same

```
25
```
```
22
```
```
16
```

NVFBC approach if available (NVIDIA’s Capture SDK is supported on Linux for certain drivers, as noted by
NVIDIA’s docs). There’s also a community of **Rust and Python wrappers** for Desktop Duplication (for
Windows) and some for Linux screen capture; these show that multiple platforms can be handled, though
Windows is currently more straightforward with official dirty-rect support. The easiest starting point is
Windows (with entire screen capture as specified), and among Windows options, using the DXGI API is open
and easy, whereas NVFBC might yield even lower latency if available. So we will likely prototype with
Desktop Duplication API, then optimize/hardware-accelerate using NVFBC if needed once the concept is
proven.

# 3. Applications: Demonstrations & Use Cases

Once our event-based capture system is running, we can showcase its capabilities with both **simple demos**
and more **advanced AI applications** to illustrate the impact:

```
(a) Event-Based Screen Recording & Replay: A fundamental demo is recording a segment of screen
activity as an event stream and then replaying it to verify fidelity. For example, we could record
ourselves moving a window around, typing text, or playing a short video clip on the screen – the
event log would capture only the pixels that changed (moving window edges, the text cursor
blinking, or the video’s moving regions). We then use our replay tool to regenerate a video from
these events. The replay should visually match the original and maintain the correct timing (e.g. if a
window took 2 seconds to drag across the screen, the replay does the same). We’ll be able to
measure the data size: e.g. a 10-second recording of mostly static screen might produce only a few
kilobytes of events (versus tens of megabytes as full video). This demo validates the core concept
and lets us measure performance (e.g. steady 60 FPS capture without drops, and the ability to play
back at 60 FPS). We can also visualize the events themselves : for instance, plotting each event as a dot
or brief flash at its (x,y) location. This would show a sparse cloud of changes moving around – a very
different visual than full frames. Such visualizations are common in event camera research to depict
activity (white dots for positive events, black for negative, on a blank canvas). It would highlight how,
say, a moving object is outlined by a flurry of pixel-change events while static background stays dark.
This demo is largely about efficiency : we can explicitly show the reduction in redundant data
(perhaps by overlaying the dirty rectangles on the original video or counting pixels processed vs
total pixels).
```
```
(b) Real-Time Change Highlighting (UI Monitoring): Another straightforward application is a tool
that runs and highlights screen regions as they change in real time. Using the stream of events, we
can draw bounding boxes or translucent overlays on parts of the screen that just updated. This is
somewhat like a “heatmap” of activity. For instance, if you’re looking at a dashboard and one number
updates, only that number’s area would glow briefly. This has practical debugging uses (seeing what
parts of an application are redrawing) and also demonstrates the immediacy of event-based updates
```
- our tool could react essentially instantly when something changes, without polling the entire
screen. It’s a simple form of an AI agent: the agent’s logic here is just to notify or visualize any
change events. We could also filter events by type; e.g., ignore small single-pixel changes (maybe
noise or cursor blinking) and only alert when larger regions change, showing how tunable the event
stream can be.

```
(c) Low-Latency AI Screen Agent (Concept): The motivation behind this project is to enable AI
agents that observe and interact with your screen more efficiently. Consider an AI assistant that can
```
## •

## •

## •


```
control UI elements or a game-playing agent (like a bot that plays a video game by looking at the
screen). Traditionally, such agents sample the screen at some interval (say every 100 ms or 16 ms)
and run image analysis (like OCR or object detection) on each frame. That means a lot of repeated
processing on mostly identical frames. With an event-based feed, the agent can sleep when
nothing changes and only process the sparse events when they arrive. We can demonstrate a
prototype agent that uses events to trigger its logic. For example, imagine a simple AI that monitors
a stock ticker on screen and presses a “buy” button when a certain number changes. Using event
capture, the agent gets an event exactly when the number changes (with the pixel coordinates and
new digit value if we tie in OCR) and can react immediately. In contrast, a frame-based approach
might have been checking the screen periodically and possibly reacting slower or doing needless
checks when the number hadn’t changed. Another scenario: an agent playing a game like Tetris –
large parts of the screen are static except when a piece moves. The event stream would contain a
flurry of events only as the pieces move; the agent can focus computation on those moments (and
possibly achieve higher responsiveness, as it doesn’t have to wait for a fixed next frame – though
practically at 60Hz it’s similar, but if we had a higher refresh or irregular timing, it matters). We could
start with a simple agent that just identifies an onscreen change (like a pop-up appearing) and
automatically clicks it – showing end-to-end how event-based perception can drive action with
minimal lag and overhead. This application underscores low-cost AI : if the agent runs on a local
machine or cloud, it will consume far fewer CPU/GPU cycles using events vs analyzing full-screen
images continuously, which translates to cost savings and possibly the ability to scale to many
agents or long-running tasks without heavy resource use.
```
```
(d) Event-Based Computer Vision on Screen Content: We can leverage existing event-based vision
algorithms to do higher-level tasks on our screen feed. Many algorithms developed for event camera
data could be repurposed. For instance, object detection and tracking can be done with events –
there are research papers on detecting objects or gestures using event streams (often requiring new
network architectures or adapting frame-based ones by converting events to frame-like
representations). In our context, imagine displaying a video on screen and using an event-based
detector to identify moving objects within it. The advantage would be that the detector only “sees”
the moving edges via events, potentially simplifying the task. As a concrete example, EVREAL’s toolkit
includes downstream tasks like object detection and image classification using event-based
reconstructed videos. We could demonstrate something similar: feed our captured event stream
(or the reconstructed frames from it) into an ML model to classify what’s happening on screen. A
simple demo: show an MNIST digit on screen that sometimes changes, and use an event-based
classifier to recognize the digit from the events generated when it changes (illustrating that if the
digit doesn’t change, no processing happens, but when it flips from say 3 to 8, a burst of events
allows the model to classify the new digit). More advanced: optical flow or motion analysis using
events – event cameras excel at optical flow because of the high temporal resolution. If we have, say,
a video of something moving on screen, an event-based optical flow algorithm could track the
motion smoothly. We could compare it to a traditional frame-based optical flow to show the event
method doing it with less data and potentially lower latency. This would borrow techniques from
robotics (e.g. event-based SLAM or tracking algorithms) but apply them in a synthetic scenario.
```
```
(e) Reduced-Bandwidth Screen Streaming: Beyond AI, another application of this technology is in
remote screen streaming (like remote desktop or cloud gaming). Our event-based capture could be
extended to send only the events over a network, which is conceptually similar to how VNC works
but we could further compress or process events. The client could reconstruct the screen from
```
## •

```
22
```
## •


```
events. If the network is bandwidth-limited and the screen is mostly static or simple, this could be
extremely efficient. While standard codecs probably outperform naive event streaming for natural
video (because of motion compensation, etc.), there might be cases (UI with sharp changes) where
an event stream is simpler and lossless. This is a side note, but demonstrates the versatility of
capturing only changes.
```
In summary, the applications range from **developer tools** (visualizing screen updates), to **AI system
enhancements** (making screen-reading AI faster and leaner), to even **new user experiences** (like ultra-low
latency remote interfaces). Each demo we propose highlights a different benefit: reduced data, immediate
reaction, or continuous sensing of change. Combined, they make a strong case that event-based screen
capture can _“advance video context”_ handling by shifting from heavy frame-by-frame processing to an on-
demand, change-driven paradigm.

# 4. Limitations & Feasibility Assessment

While promising, this approach comes with challenges and trade-offs that must be acknowledged:

```
High-Change Workloads: In the worst-case scenario, an event-based capture provides little to no
gain. If the entire screen is changing rapidly (imagine a fullscreen video or a game with lots of
motion), the event stream might be almost as large as the frame stream. For example, a 1920×
video at 60 FPS where every pixel changes each frame would produce on the order of 120 million
pixel events per second (which could be on the order of a few hundred MB/s of raw event data).
That’s comparable to raw video and far above what a typical compressed video would be. In such
cases, event-based capture doesn’t reduce bandwidth; in fact, it could be more data than an efficient
codec, because codecs exploit spatial and temporal redundancy with motion vectors and block
predictions, whereas a naive event list would list every pixel change. We can mitigate this by
integrating some compression – e.g., sending “block moved from here to here” events rather than
individual pixels (leveraging the move rect info). That starts to replicate what video codecs do,
though. The bottom line is that the approach shines in scenarios with sparse or localized
changes , but if the use-case inherently has dense change, we don’t save much storage or compute.
The good news for our target (screen UI and many applications) is that often large portions of the
screen are static or have simple movements (windowed content, static toolbars, etc.). And even in
video content, backgrounds often are static or change slowly relative to local motion. Nonetheless,
for very dynamic content, one might fall back to traditional encoding or accept that the event
processing system will have to handle a very high event rate (which is a challenge for real-time
processing).
```
```
Tooling and Algorithm Maturity: The ecosystem for frame-based vision is very mature – countless
libraries and pre-trained models exist. In contrast, working with raw event streams is still an active
research area. As Gallego et al. note, traditional vision algorithms cannot be directly applied to
event data ; new algorithms must exploit the asynchronous, high-temporal-resolution nature of
events. This means that to fully utilize our event-based screen data, we may need to develop or
adopt specialized AI models (e.g. spiking neural nets, event-specific CNNs) which are less common in
industry right now. In the near term, one workaround is to use event-to-frame reconstruction :
algorithms like E2VID can convert a stream of events into synthesized video frames. We could
then feed those frames to any standard vision model. EVREAL’s existence suggests that
reconstructing decent-quality videos from events is feasible (they benchmark various neural
```
## •

## •

```
16
```
```
26
26
```

```
networks for that). However, that reconstruction step adds computation overhead and some latency,
partly negating the efficiency we gained. It’s essentially a compatibility layer to use existing AI. The
more exciting (but challenging) path is to train AI agents that consume events directly, avoiding full
reconstruction. There are early successes in this domain (e.g., event-based object tracking, gesture
recognition demos), but if our goal is e.g. “an AI that reads my screen and responds,” significant
engineering is needed to incorporate event-based perception. We have to ensure the feasibility of
that: one approach is to treat events as just a more efficient sensing modality and still ultimately use
conventional decision logic on reconstructed or aggregated data; another is to invest in
neuromorphic models that truly leverage the fine temporal detail and sparsity (for maximum gain in
low latency and low compute). So there’s a learning curve and development effort required – not a
deal-breaker, but a consideration.
```
```
Complexity of Implementation: Building an event-based capture system is more complex than
using off-the-shelf screen capture. We must handle the nuances of OS APIs, GPU synchronization,
and new data formats. For instance, ensuring we correctly handle the Windows dirty rects (process
move rects first, etc., as recommended to properly reconstruct frames) is extra work. If the OS
coalesces updates and flags a slightly larger region than actual change, we either accept a few extra
events (which might be benign) or add a refinement step to filter out unchanged pixels by
comparing with the last frame – adding computation. Also, using NVIDIA’s NVFBC might require
dealing with driver and compatibility issues (and if the user doesn’t have the right GPU or drivers, we
need to fall back gracefully to Desktop Dup). Cross-platform support doubles this complexity by
needing different backends. All of this is surmountable – we are reusing known APIs and adding diff
logic on top – but it’s not a one-liner solution.
```
```
Data Volume and Storage: We champion that event-based recording reduces data, which is
generally true for sparse changes. But when recording long sessions, the raw event log can still
grow large if there’s frequent activity. We should consider compression of the event stream. Perhaps
we can borrow ideas from event camera compression (there are works on compressing event
streams by clustering events in space-time). Even a simple approach like run-length encoding of
events or grouping events by region could help. Storing each event as, say, a 16-byte record (with
time, x, y, and some value) is not too bad, but millions of them add up. We might end up using a
binary format and then compress with a general compressor (like zstd) which might find patterns
(e.g. many events with similar timestamps or spatial locality). Also, storing timestamps in an absolute
form could be inefficient (if events are frequent, storing the delta times which are small might
compress better). These are implementation details – the feasibility is still fine, it just means we’ll
design the format thoughtfully if large-scale logging is needed.
```
```
Screen Refresh Constraint: A subtle limitation is that standard displays and OS compositors update
at fixed intervals (60 Hz or 120 Hz, etc.). Unlike a true event camera which can spit out events at
microsecond resolution as soon as light changes, our system is still bound by frame rate for new
content generation. That means our latency advantage is bounded – we can’t get events earlier than
the screen refresh that produces them. So, if a game is running at 60 FPS, our events will also
effectively come at 60 Hz intervals (just all at once at frame boundaries). We are not beating
traditional frame capture in terms of reaction time to something that appears on screen at time T
(we will also only know at that same frame time T). The latency savings are more about not processing
unnecessary frames , rather than getting information sooner than frame-based capture would allow.
However, there is a scenario where we do gain: if the agent chose to only sample at, say, 10 Hz to
```
## •

```
19
```
## •

## •


```
save resources, it might miss changes that happened in between. With event-based, it can run at an
effective 0 Hz when idle and automatically spike to 60 Hz (or whatever needed) when changes occur
```
- so it’s both efficient and doesn’t miss temporal detail. If we had display technology or OS event
hooks that were finer grained (e.g. subregions updating out-of-phase with global vsync), event-
based capture could catch those immediately. Some modern compositors or e-ink like displays might
do partial updates asynchronously – our system would handle that naturally by logging events
whenever they come. But with standard monitors, essentially we’re getting a discrete-time event
stream synchronized to refresh. So one could say we are not fully exploiting the microsecond
capability of neuromorphic vision, since the source (the screen) isn’t asynchronous in that way. The
benefit we **do** exploit is the _spatial sparsity_ : not sending 2 million pixel values every 16ms when only,
say, 10k pixels changed.

```
Feasibility: Despite the above caveats, the core idea is highly feasible with today’s tech. All the
individual components exist: GPU-accelerated screen capture, diff detection, event data handling,
and event-based algorithms (in research). The project is essentially an integration and engineering
effort, with some innovation in adapting algorithms for the screen domain. In 2025, interest in
event-based vision is at an all-time high (e.g. multiple CVPR workshops, more hardware coming out),
meaning tools and community support are growing. By aligning with those standards (as we
plan by using similar data formats and methods), we ensure our event-based screen data can be
readily used by researchers familiar with neuromorphic vision – possibly accelerating development
of those low-latency agents we envision. The fact that EVREAL and other benchmarks exist for event
video reconstruction indicates that even tasks like converting our screen events back to video
frames can leverage state-of-the-art neural networks (like E2VID or HyperE2VID). So if we needed
high-quality reconstruction (say for a visualization or for an AI that can only handle images), it’s quite
achievable. Also, hardware is not a limiting factor: even modest CPUs can handle tens of
thousands of events per second (which is trivial compared to what they handle in frame processing),
and GPUs can help with any heavy lifting for diff or neural processing. In short, building the event
capture system is entirely feasible now , and it opens a path forward for more efficient screen-
interacting AI. The main limitations to keep in mind are: ensuring the approach is beneficial under
the given workload (sparse changes vs. full video), and bridging the gap between event data and
existing AI models (which is more of a software ecosystem challenge than a fundamental barrier).
```
**Conclusion:** By researching and combining these insights, we have a clear plan to implement an event-
based screen capture system. We will leverage Windows’ DXGI or NVIDIA’s NVFBC to capture only what
changes on the screen and represent those changes as an asynchronous stream of events. This approach is
analogous to how event cameras capture visual information and promises reduced overhead and latency
for screen-monitoring AI agents. We found no off-the-shelf tool doing this yet, indicating our solution would
be novel, but we’re standing on solid shoulders: prior work on difference-based capture and event-based
vision provides the needed groundwork. With careful engineering, the first deliverable (a working
capture and replay demo) can be achieved on Windows with NVIDIA GPUs. From there, we can explore
more sophisticated AI integrations, demonstrating both the **practical feasibility** and the **innovative
advantages** of moving from frame-based to event-based screen vision. The potential payoff is significant –
enabling AI systems to “see” your screen with far less data and react more immediately, which is a win for
scalability (lower compute cost) and performance (faster response) in the era of increasingly interactive and
intelligent desktop agents.

## •

```
27 28
```
```
26
```
```
29
```
```
4 15
```

**Sources:**

```
NVIDIA Capture SDK & GStreamer performance (blog)
Microsoft Desktop Duplication API (dirty rects & moves)
NVIDIA NVFBC difference map capability
Event-camera style vision (Gallego, 2022)
Frame-to-event conversion research (Pix2NVS 2017; v2e 2021)
EVREAL event-video reconstruction & tasks (CVPRW 2023)
```
Guillermo Gallego - Event-based Vision
https://sites.google.com/view/guillermogallego/research/event-based-vision?authuser=

GitHub - uzh-rpg/event-based_vision_resources: Event-based Vision Resources. Community
effort to collect knowledge on event-based vision technology (papers, workshops, datasets, code, videos,
etc)
https://github.com/uzh-rpg/event-based_vision_resources

Desktop Duplication API - Win32 apps | Microsoft Learn
https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api

e-INFRA CZ Blog - Witcher in a Browser
https://blog.e-infra.cz/blog/witcher-in-browser/

developer.download.nvidia.com
https://developer.download.nvidia.com/designworks/capture-sdk/docs/7.1/
NVIDIA%20Capture%20SDK%20Programming%20Guide.pdf

GitHub - ercanburak/EVREAL: EVREAL: Towards a Comprehensive Benchmark and Analysis
Suite for Event-based Video Reconstruction (CVPRW 2023)
https://github.com/ercanburak/EVREAL

## •^57

## •^410

## •^1312

## •^1

## •^23

## •^2622

```
1 16 28
```
```
2 3 15 27
```
```
4 8 9 10 11 14 18 19
```
```
5 6 7 20 24
```
```
12 13 17 21
```
```
22 23 25 26 29
```

