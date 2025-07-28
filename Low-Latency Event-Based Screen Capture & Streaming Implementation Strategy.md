# Low-Latency Event-Based Screen Capture &

# Streaming Implementation Strategy

## Overview and Goals

Building a **low-latency, event-based screen capture** system on Windows 10/11 (using NVIDIA RTX
3050/3090 GPUs) requires capturing screen changes and user input with minimal overhead, then streaming
these as “events” to a local visualization and to AI agents. We aim to achieve responsiveness comparable to
NVIDIA ShadowPlay (minimal performance impact ). The system will capture pixel-level changes (deltas)
and input events, reconstruct the screen on another display or window in real time, and feed a
reinforcement learning (RL) agent with a stream of asynchronous screen updates. Key design
considerations include using NVIDIA’s GPU-accelerated capture APIs, intercepting OS drawing calls for fine-
grained updates, and possibly employing a virtual display driver to mirror the main screen. We also outline
how a lightweight RL model can ingest these event streams and run continuously (with user-interrupt
capability), with the option to later port it to C++ for performance. Below, we detail each capture approach
(with C++ integration points and code patterns), the event streaming/visualization design, and the RL model
integration, noting prerequisites and trade-offs at each step.

## 1. Capturing Screen Events (Pixel-Level Changes & Input)

### 1a. NVIDIA NVFBC with Difference Maps

**Setup & Prerequisites:** NVIDIA’s **Frame Buffer Capture (NVFBC)** API is a high-performance capture
method that can grab frames directly from the GPU’s framebuffer. It requires the NVIDIA Capture SDK (part
of NVIDIA’s NDA developer programs) and typically works on professional/enterprise GPUs or unlocked
drivers. Ensure the NVIDIA driver supports NVFBC (on consumer cards, NVFBC may be disabled by
default without a patch or special license ). You’ll need to include the NVIDIA Capture SDK headers (e.g.,
NvFBC.h) and link against the provided libraries. Also install the **NVIDIA GPU driver** that corresponds to
the SDK version and enable NVFBC if required (some setups need calling NvFBC_Enable() or registry
tweaks to allow capture).

**Using NVFBC in C++:** You can create an NVFBC capture session either to system memory (NVFBCToSys) or
to a DirectX surface (NVFBCToDx9Vid or similar). We will use the **difference map** feature to get block-level
change indicators between frames. When setting up NVFBC, request the diff-map:

```
// Pseudo-code for NVFBC initialization with diff-map (C++)
NVFBCToSys*fbc= NvFBC_CreateToSys(); // Create NVFBC to system-memory
instance
NVFBC_TOSYS_SETUP_PARAMSsetupParams = { NVFBC_TOSYS_SETUP_PARAMS_VER};
setupParams.bWithHWCursor= true; // capture cursor as needed
setupParams.bDiffMap = true; // enable difference
```
```
1
```
```
2
2
```

```
mapping
setupParams.eDiffMapBlockSize = NVFBC_TOSYS_DIFFMAP_BLOCKSIZE_16X16; //
smallest blocks
setupParams.ppDiffMap = (void**)&g_diffMapBuffer;
setupParams.dwDiffMapBuffSize = diffMapBufferSize; // size in bytes for diff
map
NVFBCRESULT res= fbc->NvFBCToSysSetUp(&setupParams);
```
In the above setup, we enable NVFBC’s diff-map with a 16×16 pixel block size (if supported by the driver
). The NVFBC status can be queried via NvFBC_GetStatusEx to see if smaller blocks like 16×16 are
supported by the GPU driver (otherwise it falls back to 128×128 blocks). The difference map is allocated
by NVFBC as a byte array where **each byte corresponds to a block of the screen** (in row-major order).
A non-zero byte indicates that **some pixels in that block have changed** since the last captured frame.

**Grabbing frames and events:** After setup, we enter a capture loop. Each iteration, call the NVFBC capture
function (e.g., NvFBCToSysGrabFrame()) to get the latest frame:

```
NVFBC_TOSYS_GRAB_FRAME_PARAMSgrabParams= {
NVFBC_TOSYS_GRAB_FRAME_PARAMS_VER};
grabParams.pSysBuffer = myFrameBuffer; // CPU memory buffer for image
grabParams.dwTargetWidth = screenWidth;
grabParams.dwTargetHeight= screenHeight;
NVFBC_FRAME_GRAB_INFO frameInfo= {};
grabParams.pNvFBCFrameGrabInfo= &frameInfo;
res= fbc->NvFBCToSysGrabFrame(&grabParams);
if(res== NVFBC_SUCCESS) {
// Access the diff map buffer for changed regions
uint8_t* diffMap= g_diffMapBuffer;
// Process diffMap to identify which blocks changed...
}
```
On a successful grab, myFrameBuffer contains the current full-frame image (pixels), and
g_diffMapBuffer contains the diff-map (e.g., each byte 0/1 indicating unchanged/changed block). To
generate _event-based updates_ , compare the diff map to the previous iteration. For each byte that is non-zero
(i.e., the corresponding block has changed pixels ), we can treat that as a “pixel change event.” We might
record the block’s coordinates and even the pixel data from myFrameBuffer for that region. This yields a
list of changed regions (like “dirty rectangles”). If no pixel in a block changed, the byte stays zero, so no
event for that block. NVFBC diff-maps are incremental, so to correctly reconstruct, we should apply all diff-
maps sequentially. If frames are dropped (not processed), NVFBC suggests _merging diff maps by bitwise OR_
to avoid missing changes (since the diff map acts like a bit array of changes).

**Performance considerations:** NVFBC is extremely efficient – it captures frames directly from the GPU with
minimal impact. In fact, using NVFBC yields **only ~2–3% frame rate cost versus 8–15% with typical
desktop duplication capture**. This low overhead is why ShadowPlay uses NVFBC. To minimize
latency, use the NVFBC_TOSYS_GRAB_FLAGS_NOWAIT flag (non-blocking grab) so you don’t wait for a new

```
3 3 4 4 4 5
```
```
1 6
```

vsync if you want to poll faster. Also, NVFBC can capture into a DX9 GPU surface to avoid copying pixels to
CPU: for example, NVFBC can directly populate a DX9 texture, which you could then use for rendering or
encoding without ever copying to system memory. This trick, used in **NvFBC-Relay** , keeps the buffer in
GPU memory and uses pointer swapping to display or encode it, eliminating unnecessary CPU copies.
In our case, if we want ultra-low latency local display, we could capture to a GPU surface and immediately
blit it to a window (see Reconstruction section below). However, for feeding an RL model (often CPU or
separate process), having the frame in system memory is convenient – we can still use the diff map to only
process changed parts.

**Capturing Input Events:** In parallel with NVFBC, we capture **keyboard and mouse events** at the OS level.
For low-latency input capture, use Windows **Raw Input** or low-level hooks. For example, set a global **low-
level keyboard hook** (SetWindowsHookEx(WH_KEYBOARD_LL, ...)) and a **mouse hook**
(WH_MOUSE_LL) to log key presses and mouse movements/clicks. These hooks deliver event data (key
codes, mouse coordinates, etc.) asynchronously on a callback. Alternatively, Raw Input
(RegisterRawInputDevices) can be used to get raw keyboard and mouse data with minimal overhead.
Capturing input is crucial so that the event stream includes user actions (key presses, mouse clicks) along
with screen pixel changes, which an AI agent can use to correlate UI changes with user input. The input
events can be timestamped and queued in the same stream as the visual events. (If the RL agent will also
take actions, you might later intercept or synthesize input events, but here we focus on capturing them.)

**Summary:** The NVFBC approach gives us **pixel-difference events per frame** at the granularity of 16×16 (or
larger) blocks, with very low overhead on the GPU. It’s ideal for high-frequency capture and is the closest to
ShadowPlay’s method. Limitations are that NVFBC may not be enabled on all GPUs by default (workarounds
or enterprise GPUs may be needed ), and it operates on full frames (so events are still tied to frame
intervals – typically the display refresh or application frame rate). Next, we consider methods to get even
more granular or earlier notifications of drawing changes.

### 1b. Intercepting OS-Level Drawing Calls (GPU Command Hooks)

Another strategy is to **hook into the OS or GPU rendering pipeline** to catch drawing events _as they occur_ ,
rather than after a full frame is composed. The idea is to intercept API calls or GPU commands that update
the screen. This could, in theory, yield sub-frame latency (notifications immediately when a drawing
command executes, possibly faster than waiting for the next vsync).

**Hooking DirectX Present:** Many applications (games, etc.) render via DirectX or OpenGL. By hooking the
_Present_ call (for DirectX, e.g. IDXGISwapChain::Present or Direct3D Present() in DX9), one can
intercept frames at the moment the app presents them to Windows. A typical approach is to inject a DLL
into target processes or use a global hook with a library like Microsoft Detours or MinHook. For example,
we override the Present function:

```
// Pseudocode for hooking IDXGISwapChain::Present
typedefHRESULT(__stdcall*PresentFn)(IDXGISwapChain*, UINT, UINT);
PresentFnRealPresent =nullptr;
HRESULT__stdcallHookedPresent(IDXGISwapChain* swapChain, UINTsyncInt, UINT
flags) {
// Grab the back buffer as a texture
```
```
7
7
```
```
2
```

```
ID3D11Texture2D* backBuffer =nullptr;
swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
// Option 1: Copy backBuffer to CPU or shared surface and identify changes
// Option 2: Tag this frame as an "event" (the whole frame changed for this
window)
// ... (we could compute diff with last frame if needed)
SafeRelease(backBuffer);
return RealPresent(swapChain, syncInt,
flags); // call original to actually present
}
```
By installing HookedPresent in place of the real Present, we can access each frame just as it’s about to be
displayed. This method yields full frames, so to get _difference events_ we would compute a diff against the
previous frame (this could be done on GPU or CPU). However, hooking Present per application means we’d
have to hook all processes that render to the screen (which is not trivial for the whole desktop). It is more
practical when focusing on a specific application’s output (e.g., a game window). The latency improvement is
modest – you get the frame perhaps a few milliseconds earlier than the desktop compositor shows it, but
you are still fundamentally limited by the app’s frame rate and Present timing.

**Hooking GDI/GDI+ drawing:** Legacy or UI applications might use GDI calls (BitBlt, StretchBlt, TextOut, etc.)
for drawing. One could hook these GDI functions in user32.dll or gdi32.dll to catch when any
window draws. For instance, hooking BitBlt could let you log the source and destination DC (device
context) and the rectangle of pixels being blitted. However, globally hooking all GDI calls in every process is
complex and can slow down the system. Moreover, modern Windows GUI rendering is largely done via
DirectComposition/DWM and GPU acceleration rather than raw GDI in many cases.

**Desktop Composition and DWM interception:** Windows Desktop Window Manager (DWM) composes the
final desktop image each frame (typically at 60Hz or more). Intercepting the composition could mean
hooking the DWM process’s DirectX calls. This is _not officially supported_ , but tools like **Desktop Duplication
API** or **Windows Graphics Capture** essentially tap into the DWM output. We’ll cover the Desktop
Duplication API below as a cleaner alternative to hooking DWM internals.

**Windows Desktop Duplication API:** This is a public API introduced in Windows 8 (DXGI 1.2) that allows
capturing the desktop image and provides change information. It’s not exactly “faster than vsync” (it
delivers frames at the refresh rate), but it does provide **dirty rects (regions of change)** and **move rects** per
frame. Using this API in C++ is straightforward: 1. Create a DXGI output duplication
(IDXGIOutputDuplication) for the primary monitor. 2. Each loop iteration, call
AcquireNextFrame(timeout, &frame, &frameInfo). This returns a DXGI surface for the new frame
and a DXGI_OUTDUPL_FRAME_INFO containing metadata. 3. Use GetFrameDirtyRects() to retrieve an
array of RECT that changed in this frame. Also use GetFrameMoveRects() to get if any screen
areas were moved (e.g., a window dragged) – move rects give old & new positions. 4. Copy out the dirty rect
regions (or the whole frame initially).

The Desktop Duplication API thus naturally provides an **event list of changed rectangles per frame**. For
example, if only a small menu opened, you might get a dirty rect for that area only. Microsoft’s
documentation notes that to reconstruct exactly, you should first apply move rects then paint dirty rects

```
8 9
```
```
9
```

(so you handle things like scrolling or window moves efficiently). Using this API, you could convert each
dirty rectangle into an event (with the pixel data for that rectangle). The code looks like:

```
IDXGIOutputDuplication* deskDupl
= /* created from DXGI Output1->DuplicateOutput */;
// ... in capture loop:
DXGI_OUTDUPL_FRAME_INFOinfo;
ComPtr<IDXGIResource> desktopResource;
HRESULThr = deskDupl->AcquireNextFrame(16, &info, &desktopResource);
if(SUCCEEDED(hr)) {
// Map frame to CPU if needed via ID3D11Texture2D
D3D11_MAPPED_SUBRESOURCEmapped;
d3dContext->Map(cpuTexture, 0, D3D11_MAP_WRITE, 0, &mapped);
// get dirty rects
UINTdirtyCount = 0;
deskDupl->GetFrameDirtyRects(0,nullptr, &dirtyCount);
std::vector<RECT> dirtyRects(dirtyCount);
deskDupl->GetFrameDirtyRects(dirtyCount* sizeof(RECT), dirtyRects.data(),
&dirtyCount);
// Process each dirty rectangle
for (RECTrc : dirtyRects) {
// Extract pixel data for rc from mapped.pData
// Create an "event" with rc coordinates and pixels
}
// Release frame for next
deskDupl->ReleaseFrame();
}
```
**Comparison and Latency:** The Desktop Duplication method is effectively what many screen recorders (like
OBS when not using NVFBC) rely on. It ensures we only handle updated regions (saving some CPU if small
changes occur). However, it is tied to the display’s frame rate – you won’t get updates faster than the
monitor refresh (if the GPU renders faster, extra frames might be dropped or queued by DWM). Hooking an
app’s Present, in contrast, could capture faster-than-refresh frames for that app (e.g., a game running 120
FPS on a 60Hz display – though those extra frames aren’t displayed, an agent could theoretically see them).
But capturing unseen frames may not be useful if we care about what’s actually shown. In practice, **for
entire desktop capture, Desktop Duplication is the robust approach** (when NVFBC is unavailable),
whereas hooking is more useful for targeted apps or specialized situations.

**Trade-offs:** Hooking at the OS/GPU level is complex and can reduce stability. Maintaining hooks for multiple
graphics APIs (DirectX 9/11/12, OpenGL, Vulkan) is a huge effort (similar to what tools like FRAPS or OBS
hooks do). It also may miss kernel-level optimizations. In contrast, official APIs like Desktop Duplication give
a reliable feed of changes at low overhead (though not as low as NVFBC). For completeness, **Windows
Graphics Capture** (introduced in Windows 10) is another API that allows app-level or screen-level capture
via a Direct3D11 frame pool; it’s similar in function to Desktop Duplication but with easier integration with
UWP/WinRT. It also delivers frames with dirty rect metadata and can capture specific windows with lower
overhead in some cases.


**Capturing input events:** Regardless of visual capture, you would also use low-level hooks or Raw Input (as
described earlier) to capture keyboard and mouse events. This is independent of the graphical hooking –
e.g., even if you intercept DirectX presents, you still need a separate hook for input events. In C++, a simple
approach is using SetWindowsHookEx with WH_KEYBOARD_LL and WH_MOUSE_LL to log events
globally. Ensure to process these quickly (perhaps just push into an atomic queue or buffer for the main
thread to handle) to minimize hook delay.

**Summary:** Intercepting OS drawing calls can, in theory, yield the most granular events (even individual
draw commands as events). For example, a GDI hook on TextOut could treat each text rendering as an
event (with the text’s rectangle). However, implementing this generally across the system is complex. A more
practical middle-ground is using the Desktop Duplication API which provides per-frame dirty rectangles –
this is easier to implement and doesn’t require code injection. It will have slightly higher overhead and
latency than NVFBC (since it’s CPU-based copying of frame data), but it’s supported on all Windows 10+
systems. In an implementation, one might first attempt NVFBC and fall back to DXGI Desktop Duplication if
NVFBC is unavailable. Both yield a stream of changed regions that we can feed into our event pipeline.

### 1c. Virtual Display Driver (Mirror Driver Approach)

This approach involves creating a **virtual monitor** that duplicates the main screen, allowing us to intercept
the rendering at the driver level. In Windows, this can be done via a **Mirror Driver** (in legacy XPDM) or the
modern **Indirect Display Driver (IDD)** model in WDDM. The idea is that the OS will treat our driver as an
additional display; when we set it to mirror (clone) the primary display, the OS will send all rendering output
to our driver as well. The driver can then capture drawing commands or the final pixel data.

**How it works:** A mirror/virtual driver typically creates a **software surface that the OS draws into**. For
example, in a classic mirror driver, you create a device surface in DrvEnableSurface and use
EngModifySurface to tell GDI about it, possibly pointing it to a shared memory buffer. You also **hook
certain drawing functions** (like DrvBitBlt, DrvTextOut) by specifying flags in
EngAssociateSurface. When the primary display updates, GDI will call your driver’s hooks for each
drawing operation (in parallel to the real driver). As an expert explained: _“Every time the desktop changes,
GDI calls on the one hand the driver of the physical graphics device and on the other hand the corresponding
DrvXxx() function of your mirror driver... you have detailed information about the ‘changed area’ of the desktop
within the parameters of the DrvXxx call (the target rectangle and clip region).”_. In other words, the mirror
driver directly tells you which region is being updated and even what drawing operation (bitblt, line draw,
text) is happening, giving very fine-grained events.

In the modern WDDM world, things are slightly different because drawing is GPU-accelerated and goes
through DWM. Instead of hooking GDI calls, an **Indirect Display Driver (IDD)** uses a user-mode driver that
cooperates with the OS via the **IddCx** class extension. You essentially register a virtual monitor device, and
the OS will provide bitmaps for each frame that needs to be displayed on that monitor. In duplicate mode, it
should give the same frames as the real monitor sees. The IDD can allocate a frame buffer (e.g., in system
memory) and the OS will render the desktop into that buffer for the virtual monitor. Our user-mode driver
or service can then access that buffer (since we allocated or shared it) and thus get the pixel data for each
frame. We can also potentially get only the delta if we track changes between frames ourselves. While IDD
might not give explicit dirty rect callbacks as easily as Desktop Dup, we could implement change tracking by
comparing the buffer or using partial presentation calls.

```
10
```
```
10
```

**Implementation steps:** Implementing an IDD requires writing a **driver in C++** and using the Windows
Driver Kit (WDK). Microsoft provides a sample (for example, the _IndirectDisplay_ example driver in the WDK,
or open-source samples ). Key steps: - Create a virtual monitor device with desired resolution. This
can often be done purely in user-mode using IddCx (no kernel-mode code needed other than the provided
extension). - When the device is started, allocate a framebuffer (e.g., a shared memory section or user-mode
accessible memory) that will represent the screen pixels. - Present this to the OS: when the OS wants to
draw a frame, your driver’s callback (e.g., EvtIddCxMonitorAssignSwapChain) will be called with a
swap-chain or frame buffer you provide. You can copy the desktop image into your buffer or directly point
it. - Because we want _duplication_ of the main screen, you instruct the OS (perhaps via user control panel or
programmatically) to set the new virtual display in “clone” mode with the real display. In clone mode, the OS
will ensure the same content is sent to both outputs. For the virtual output, since it’s not a real GPU, the OS
will compose the image and call your driver to display it. - As the OS writes each frame, you capture the
changes. If using GDI mirroring (older style), your hooked Drv functions directly tell you what changed (with
coords). If using IDD, you likely get the full frame; you can diff it with the previous to find changes or
potentially use dirty rect info if available.

**Reading the screen data:** In a mirror driver, typically a section of shared memory is used. For example, the
driver can create a file mapping object and set the surface’s pvScan0 pointer to that memory. The
user-space application can open the same file mapping and thus have direct access to the pixel bits.
Every time the OS draws into the mirror surface, those pixel bits change. The driver might also maintain a
list of updated regions (it could, for instance, store a list of dirty rects or set a bitflag in shared memory that
an update occurred). The user app can poll or be signaled to read the new data.

In an IDD approach, there isn’t a built-in shared memory by default, but one can create a shared DXGI
texture or use CreateFileMapping on a section provided by the driver. The IDD sample uses a circular
buffer or so for frames if I recall, but we can adapt it to ensure access. A simpler route: the user-mode
driver could copy each frame into a shared memory buffer and also store some metadata about changes
(maybe compute dirty rects by comparing to last frame in driver, then signal an event). The client application
can then consume from this.

**Overhead:** A virtual driver introduces some CPU overhead because the OS is effectively rendering the
desktop twice (once to real GPU, once to system memory for the mirror). This is similar to Desktop
Duplication’s overhead. However, because the mirror driver might be notified of only the specific operations
or regions, it could be more efficient if only small areas change (the OS doesn’t necessarily need to copy the
whole screen if only a small region updates – it might just call the driver’s BitBlt hook for that region). In
practice though, with DWM always sending full frames, the benefit may be limited. The overhead should still
be relatively low and does not impact GPU much (mostly CPU and memory bandwidth). It’s the approach
used historically by remote desktop and VNC servers.

**Sample driver code pattern:** While a full driver code is beyond scope, a snippet illustrating the principle:

```
// Pseudo-code inside mirror driver (old model) DrvBitBlt hook
BOOLDrvBitBlt(SURFOBJ* pDest, SURFOBJ* pSrc, ... RECTL* prclDest, RECTL*
prclSrc, ...){
// Let GDI do the actual drawing on the primary (or use EngBitBlt to draw
into our surface).
```
```
11 12
```
```
13
13
```

```
EngBitBlt(...); // do the default operation on our surface memory
// Now we know the rect that was updated (prclDest).
LogChangedRect(*prclDest);
return TRUE;
}
// Pseudo-code for sharing memory:
HSURFDrvEnableSurface(DHPDEVdhpdev) {
// allocate a section of memory for screen bits
VOID* pBits= EngAllocMem(0, screenWidth * screenHeight* BytesPerPixel,
'mrdr');
HSURF hSurf= EngCreateDeviceSurface((DHSURF)pBits, sizl, screenFormat);
EngModifySurface(hSurf, dhpdev, HDEV(),0, 0, (VOID*)pBits, screenStride,
NULL);
return hSurf;
}
```
The above is highly simplified. The key point is that EngModifySurface shares the pointer to our pixel
buffer with GDI , so GDI will render into that memory. Every time DrvBitBlt (or other hooked
function) is called, we get the coordinates of the change (prclDest). We’d accumulate those or signal
them to user-space (perhaps via an IOCTL on a device interface or by writing into a shared “changed
regions” buffer).

**Installation & usage:** To use this approach, the driver must be installed (registered as a display adapter). In
development, test-signing mode would be used unless you have a Microsoft-signed driver. The user would
then “add” a monitor via this driver. Programmatically, one can enumerate and enable the virtual display.
Once active, switch it to duplicate the primary display (this can be done with the Windows API or by the user
in display settings). From that point, our driver is receiving the output.

**Limitations:** Developing a custom display driver is non-trivial. Also, with WDDM and modern secure boot
requirements, deploying a custom driver can be challenging. That said, using the IDD model (which is user-
mode) simplifies it and is the officially supported way to create a virtual display in Windows 10/.
Another limitation is that we might still be limited to vsync rates – the virtual display will refresh at the same
rate as the primary (since it’s a clone). However, the advantage is **detailed change information** (especially
in the legacy mirror driver case where you get function-level detail of drawing operations). This could
enable very precise event generation (like “button X was drawn at these pixels”). For our purposes,
capturing pixel changes by region is sufficient, which this driver can do as well.

**Capturing input:** The virtual driver doesn’t inherently capture input (except maybe multi-touch or pen if it
were an interactive display). So we still rely on the normal input hooking as discussed. One benefit though:
if the RL agent in future needs to **inject input** , a virtual driver could potentially be extended to emulate an
input device or we could leverage existing virtual HID drivers. But that’s separate from screen capture.

**Summary:** The virtual display approach gives us a lot of flexibility and potentially the most _event-rich_ data
(with careful driver hooks). It introduces extra complexity in development and deployment. If successful,
though, it can run continuously in the background like an OS component and provide a feed of changes
with minimal visible impact to the user (similar to how Remote Desktop mirror drivers work quietly). For

```
13
```
```
12
```

initial development, one might use the Desktop Dup API (1b) for ease, and only move to a custom driver if
absolutely needed for performance or granular control.

## 2. Reconstructing and Streaming Events in Real Time

Once we have a stream of “screen events” (changed pixel blocks/regions and input events), we need to
**reconstruct and visualize** them in real-time, and also consider **streaming them off-device** for a remote
agent or client. This involves assembling the events back into an image or a meaningful representation on a
second display or window, and sending event data over a network or IPC.

### 2a. Local Visualization – Pixel Reconstruction & Symbolic Overlay

For debugging and demonstration, it’s useful to visualize the events on a local GUI. We can create a simple
**C++ GUI window** (for instance using Win32 GDI, SFML, SDL, or OpenGL/Direct3D) that shows the second-by-
second reconstruction of the screen from events.

**Pixel-accurate image reconstruction:** We maintain a framebuffer (an array of pixels) representing the last
known full screen image. At start, we might not have anything, so we either initialize it once we get a full
frame (e.g., the first frame from NVFBC or Desktop Dup) or clear it to black. As events (changed regions)
come in, we **update only those pixels** in our framebuffer. For example, with NVFBC diff-map events, when
a 16×16 block is marked changed, we copy that 16×16 region from the captured frame into our stored
framebuffer. With Desktop Dup’s dirty rect list, we copy each dirty rect from the acquired frame. Because
events are processed sequentially and cover all changes, our framebuffer stays up-to-date. We can then blit
this framebuffer to the screen (the second window) each cycle.

A straightforward approach is to use GDI to blit the updated regions onto a window. However, for speed
(especially at high resolution), using DirectX or OpenGL is better. One pattern is: - Create a D3D11 texture
for the framebuffer and a swap chain for the window. - Each time events update the CPU pixel buffer,
update a subresource of the texture with the changed rects (using UpdateSubresource for D3D, or
glTexSubImage2D for OpenGL). - Render a fullscreen quad with that texture to the window.

Since the changes may be small, updating only subregions is efficient. The result is a near-live mirror of the
main screen on this second window, but potentially with a slight delay (a few milliseconds or one frame of
latency depending on processing).

**Symbolic visualization of events:** In addition to the accurate image, it’s helpful to overlay markers or
highlights to show where events occurred. For instance, whenever a block or rect is updated, draw a semi-
transparent **colored rectangle or dot** on that area that fades after a short time. This mimics how
neuromorphic event cameras often visualize “spikes” as dots on a canvas. We could have one color for
intensity increase, another for decrease (though in our case, changes aren’t explicit polarity, but we could
compare new vs old pixel to derive some metric). At the very least, a flashing overlay where something
changed provides an intuitive sense of activity.

We can implement this by keeping a list of recent event regions with timestamps: - Every time an event (e.g.,
a 16x16 block change) comes in, add an entry (x,y,width,height,timestamp). - In the render loop for the GUI,
draw the current framebuffer image to the window, then iterate over the event list and for each event that


occurred in the last say 100ms, draw a small rectangle (e.g., outline or transparent fill) at that location. If an
event is older than 100ms, remove it from the list so the highlight disappears.

Drawing the overlay can be done via GDI (Rectangle on HDC) or via the GPU by drawing primitives. If using
D3D11, one could have a second texture or simply use shaders to draw colored points. Simpler: lock the
backbuffer and GDI-draw rectangles, but that is slow; instead, we could prepare a vertex buffer of small
colored quads and render them in one go.

**Transient Dots Example:** Suppose each time a diff event comes, we mark the center of that block with a
small circle. We could create a point sprite for each event and render them. These would appear as
flickering dots where changes happen (like raindrops). This provides a _neuromorphic visualization_ of the
screen activity, distinct from the full image.

**Frame Rate & Loop:** The visualization window can update as often as events come in. If the main screen is
60Hz, we can also target 60Hz for the mirror. If events come faster (say NVFBC might allow 100+ FPS capture
on some systems), our window can try to match that or throttle to monitor refresh of the second display.
The loop will roughly do:

```
while(running) {
EventListevents = GetEvents(); // gather recent events (diffs and input)
for(auto& e : events.pixelChanges) {
// update offscreen framebuffer for each changed region
BlitPixels(framebufferTexture, e.rect, e.pixelData);
recentEvents.push_back({e.rect, currentTime});
}
// draw the updated framebuffer to window
d3dContext->OMSetRenderTargets(...);
DrawTexturedQuad(framebufferTexture);
// draw overlays for recent events
for(autoit = recentEvents.begin(); it != recentEvents.end();) {
if(currentTime- it->time> OVERLAY_DURATION) {
it = recentEvents.erase(it);
continue;
}
DrawOverlayRect(it->rect, color, alpha);
++it;
}
swapChain->Present(1, 0);
}
```
This pseudo-code assumes we have the pixel data for changes (e.g., from NVFBC or Desktop Dup) readily
available. If we only had blocks without pixel data (as NVFBC diff-map alone would indicate), we use the
captured frame’s pixels for that region as shown. In NVFBCToSys, we do have the full image in
pSysBuffer, so pixel data is available. If we ever had only coordinates (say, from a driver hooking draw
calls but not the pixels), we’d need another way to fetch those pixels (not an issue in our described
approaches, since both NVFBC and Duplication give pixel content).


**Minimal visual overhead:** The second window/dummy monitor can be hidden or on a secondary GPU to
ensure the user doesn’t see it or get performance impact (similar to ShadowPlay’s hidden window). We want
_minimal impact on the user’s visible desktop._ All capturing should ideally be in the background. The
visualization is mainly for development; in deployment to stream to an agent, you might not even show
anything locally (or perhaps just a tiny preview).

### 2b. Streaming Event Data to External Consumers (API/WebSocket)

To feed the captured events to a downstream AI agent (which might run on another machine or process),
we need to stream the data with low latency. There are a few design options:

```
In-process API: If the AI agent can be a module or thread in the same application, we can directly
pass events in memory (function calls or lock-free queues). However, the prompt suggests off-device,
so assume the agent might be on a separate system or at least a separate process.
Shared Memory or IPC: For a local, separate process, one could use shared memory, memory-
mapped files, or named pipes for efficiency. Shared memory could be as simple as copying the diff-
map and pixel data into a buffer that the other process reads. You’d need some synchronization (like
an atomic ring buffer or semaphore events).
WebSocket Streaming: Using a WebSocket is a convenient way to allow either local or remote
clients to receive the events in real-time. We can run a lightweight WebSocket server in the capture
application (for example, using Boost.Beast or uWebSockets library in C++). The server can push
out a message for each event or batch of events per frame. The consumer could be a Python script,
another PC, or even a browser (if we implement a web client for visualization).
```
Given the need for minimal latency, we might choose a binary protocol over WebSocket. For instance,
define a binary message format like:

```
[frame_id][num_events][event1][event2]...
```
where each event could be encoded as
(x, y, width, height, pixel_data_length, pixel_data...) for a changed region. If
pixel_data_length is large (e.g., 16x16x4 bytes for RGBA), sending many events could be heavy. We could
compress events by grouping adjacent dirty blocks into one rect or by sending just a hash or ID for known
content (these optimizations start to resemble video codecs). Since the goal is not compression but low
latency, sending raw changed pixels might be fine if bandwidth permits (e.g., a 1920x1080 screen, even if it
all changed, full image uncompressed is ~8MB for RGBA, which at 60Hz is ~480MB/s – too high for network,
but diff approach typically reduces this drastically as most frames change partially).

**Streaming Frequency:** We can choose to send at the display refresh rate or when events occur. If the agent
can handle asynchronous bursts, we might send events as soon as they happen. NVFBC/diff is still frame-
based under the hood, so likely we’ll send updates frame by frame. If using Present hooks, you might emit
an event for each present call.

**Local vs Remote:** For local consumption (same machine, perhaps Python RL running concurrently), a local
WebSocket connection (ws://localhost) or a named pipe would work. For remote, a WebSocket over a
LAN is straightforward. It has a bit more overhead than a raw UDP or TCP, but it’s easier to implement and

#### •

#### •

#### •


flexible (and can be used in browsers for monitoring too). If needed, one could send critical data via UDP
(which is lower latency, no handshake per packet), but UDP would need a custom protocol and handling of
drops.

**Example using WebSocket (pseudo-code with Boost.Beast):**

```
// Serialize events to JSON or binary
jsonj;
j["frame"] = frameIndex;
for(auto& ev : events) {
j["events"].push_back({
{"x", ev.x}, {"y", ev.y},
{"w", ev.w}, {"h", ev.h},
{"pixels", base64_encode(ev.pixels, ev.size)}
});
}
std::string text= j.dump();
websocket.send(net::buffer(text));
```
This uses JSON with Base64-encoded pixels for simplicity, but that’s not the most efficient. A binary message
could pack the pixel bytes directly (Beast allows sending binary frames). The client (which could be a Python
script using websockets library or a C++ agent) receives these messages and can reconstruct the image
or directly feed the data to an AI model.

**Alternative: RESTful or gRPC API:** If real-time streaming is required, WebSocket is preferable to polling. An
HTTP REST API that returns the latest frame diff on request would add latency and overhead. gRPC with a
streaming response could also be used if one prefers a more structured protocol than WebSockets.

**Ensuring minimal overhead:** The act of streaming should not bog down the capture. Running the network
send on a separate thread or even separate CPU core is advisable. We can have one thread dedicated to
capturing frames/events and another thread that packages and transmits them. They communicate via a
thread-safe queue of events. This decoupling prevents the network delays from slowing down capture. Use
of efficient serialization (avoid large memory allocations per frame) and possibly compressing large pixel
sections (maybe using a fast compression like lz4 for big rects) could help if bandwidth is a concern.

**Security/Access:** If streaming off-device, consider that NVFBC and such might capture sensitive screen
content – ensure the connection is secure (if over network) or limited to a trusted network.

In summary, **WebSocket streaming** provides a flexible pipeline to feed our event stream to any consumer
with minimal delay. The consumer, e.g., an RL agent, will receive a timeline of events (pixel changes + input
events). Next, we discuss how the RL model can make use of this stream.


## 3. Integrating a Lightweight Reinforcement Learning Model

With the event-based screen data flowing, we want a **lightweight RL model** (prototyped in Python) that can
consume these events in real-time, make decisions, and potentially be upgraded to a native C++
implementation later. Key aspects include how to feed the asynchronous event stream into the model,
keeping the model running continuously (rather than turn-based like typical RL environments), and allowing
user interruption.

### 3a. Ingesting Live Neuromorphic Screen Events

Traditional RL models expect state observations at fixed time steps, but here we have a continuous stream
of events (similar to a neuromorphic vision sensor’s output). We have two main strategies to handle this:

```
Frame-based handling: Convert the event stream into a sequence of discrete time-step
observations by aggregating events over short time windows (or each display frame). For example,
every 16ms (60 Hz) combine all events in that interval to reconstruct the latest screen image (or an
“event image” where pixels that changed are marked). This way, we feed the RL agent a sequence of
frame-like inputs (could be full images or a channels representation of changes). This is simpler and
allows using standard deep RL techniques (CNNs processing each frame). However, it somewhat
diminishes the temporal resolution advantage of the true event stream.
```
```
Event-driven handling: Feed events to the model asynchronously as they occur, allowing the model
to update its internal state with each event. This is closer to how a spiking neural network (SNN) or a
continuous-time model would operate. In fact, recent research has explored continuous-time RL with
event cameras , where the network directly processes the event stream without aggregating into
frames. For instance, the CERiL algorithm introduces specialized network layers that operate on
a stream of events, enabling greatly enhanced reactivity and solving tasks that frame-based
networks can’t. The takeaway is that by not forcing a frame rate, the agent can respond quicker
and potentially more efficiently to changes.
```
Given the “lightweight” constraint, we might start with a simpler frame-based approximation, then move to
event-driven. One could design a small neural network that takes as input a downsampled image or the last
$N$ events coordinates. Alternatively, an **SNN (spiking neural network)** can naturally ingest event spikes.
SNNs process asynchronous spikes at very low power and latency, especially if neuromorphic hardware is
used. In our context, an SNN could treat each changed pixel as a spike input to a layer of neurons. There
are Python frameworks like **Norse** (for PyTorch) or **Brian2** that let you construct spiking neural nets, and
even OpenAI Gym wrappers for them. Another approach is using a standard ANN but updated
incrementally: e.g., a recurrent neural network (like an LSTM or a continuous-time LSTM) that gets event
data as input one at a time.

For initial implementation, a pragmatic solution is to **simulate an event camera frame** : create a blank
image of the same resolution as the screen, and plot short-lived “events” (e.g., white pixel for a change that
then decays to black over a few frames). The RL model (say a convolutional network) then reads this image
as the state. This is similar to how DVS (dynamic vision sensor) data is often converted into frame sequences
for using standard CNNs. It’s not fully exploiting the sparseness, but it leverages existing deep learning
tools easily.

#### •

#### •

```
14
```
```
14
```
```
15
```

### 3b. Continuous RL Loop with User Interrupt

We will write the RL model in Python for ease of development and use high-level libraries. For example, we
can use **PyTorch** or **TensorFlow** to define a small neural network. Let’s say our network is a simple CNN or
MLP that outputs an action given the current observation (which could be the raw pixel image or some
processed form of the event stream). If we have a specific task (e.g., the agent controlling something on
screen), we’d also need to compute rewards. But since the question focuses on ingestion and continuous
running, we’ll assume for now the model is doing inference on the incoming data (and perhaps logging
data for offline training, or a reward signal is computed elsewhere).

A skeleton for the Python loop might look like:

```
import torch
frommodelimport MyAgentModel # define a small NN model class
```
```
model= MyAgentModel()
model.load_state_dict(torch.load("agent_initial.pth")) # load weights if
available
model.eval()
```
```
# Connect to the event stream (e.g., a websocket client or shared memory reader)
ws = connect_to_websocket("ws://localhost:port")
```
```
# Optionally, spawn a thread to listen for user input (to allow interruption)
stop_flag= False
defcheck_user_interrupt():
import keyboard # using keyboard library for simplicity
if keyboard.is_pressed('q'):
return True
return False
```
```
# Main loop
whilenot stop_flag:
event_batch = ws.recv() # receive a batch of events (could be blocking or
async)
obs = process_events_to_state(event_batch) # e.g., construct image or
feature vector
obs_tensor = torch.from_numpy(obs).unsqueeze(0) # add batch dimension
withtorch.no_grad():
action = model(obs_tensor)
# Here, we could send action to environment or simply print/log it.
# If training, we would also accumulate reward and maybe do backprop
occasionally.
if check_user_interrupt():
stop_flag= True
```

In this pseudo-code, process_events_to_state would implement the scheme from 3a (frame
aggregation or event integration). The loop runs as fast as events arrive. If no events, recv() might block
or timeout – in practice, you might want a small sleep or integration window. The user can press 'q' to break
out (this uses a simple keyboard polling; one could also handle a GUI close event or an IPC signal). This
addresses the “input listener for user-interrupt”: it's important to be able to stop the agent loop gracefully,
especially in a continuous learning setting where there’s no natural end of episode.

We might integrate this loop with an OpenAI Gym environment wrapper if we had one, but here we’re
effectively making our own loop. This is fine for continuous tasks.

**Lightweight model choices:** We want the model to be lightweight so it can keep up with real-time data.
This means using a small network architecture (few layers, small input size). For example, if we feed a
downsampled $160\times90$ grayscale image of changes, a CNN with 2-3 conv layers and a small fully
connected part could run in microseconds on modern hardware. If using an SNN, maybe a couple of layers
of spiking neurons. Also, since we might later port to C++, we prefer architectures that are easy to
implement in C++ (e.g., basic conv and relu, nothing too exotic from Python). We should also avoid Python-
side bottlenecks in the loop. Using NumPy and PyTorch which do vectorized operations is fine; the loop
itself should not do heavy per-pixel Python processing (that’s why processing the events into an observation
might need to use efficient array operations or happen in C++ side before sending to Python).

**Continuous learning vs inference:** If we intend to train the model (reinforcement learning) while running,
we’d incorporate a learning algorithm like A3C or DQN. For example, we could accumulate transitions (state,
action, reward, next state) and run a learning step every few frames. Libraries like **Stable Baselines3** don’t
directly support event-based async data, but we could create a custom Gym environment that yields the
latest event-built observation each “step” and define step duration ourselves. In initial stages, we might run
the model in inference mode (no learning, just to test the pipeline). “Lightweight” might also imply not
training a huge deep net live, but rather using something like a linear policy or small network that can
adapt quickly.

### 3c. Path to a High-Performance C++ Agent

Once the Python prototype is validated, we’d aim to port the model to C++ for performance (and to
integrate it tightly with the capture pipeline, avoiding the overhead of sending data via sockets). There are a
few ways to achieve this:

```
Export the model to ONNX: Using PyTorch, for instance, we can export the trained model to ONNX
format. Then in the C++ application, use an inference runtime (like ONNX Runtime or load it with
OpenCV’s DNN or TensorRT if GPU optimized) to run the model. This way, the logic remains the same
but runs in C++ with potentially optimized libraries.
```
```
LibTorch (PyTorch C++ API): We can directly load the PyTorch model in C++ using LibTorch. This
would require building the C++ app with the LibTorch library, then doing:
```
```
torch::jit::script::Module model= torch::jit::load("agent_model.pt");
at::Tensoroutput = model.forward({inputTensor}).toTensor();
```
#### •

#### •


```
This gives near parity with Python’s speed (possibly faster since no GIL, etc.). The model would be the
same architecture as in Python.
```
```
Reimplement manually: If the network is very small (say a 3-layer MLP or CNN), one could even
recode it in pure C++ (especially if it uses basic operations). But using existing libraries is less error-
prone and can leverage hardware acceleration.
```
The C++ agent could then receive events directly from the capture (bypassing WebSocket). For example, it
could hook into the capture loop: after updating the frame with events, convert it to the format the model
needs and run inference to decide an action. If the agent needs to act (say move the mouse or press a key),
the C++ code can then simulate input (using SendInput Win32 API to inject keyboard/mouse events) to
close the loop.

**Lightweight RL algorithm considerations:** Since the question is about models, not just networks, note
that a reinforcement learning algorithm typically involves exploring and learning from reward. A lightweight
algorithm could be as simple as **Q-learning** with function approximation (the network predicts Q-values for
actions) or a policy gradient with a small network. Given the streaming nature, an **on-policy algorithm** like
A2C/A3C could work (these run continuously and update periodically). In Python prototype, one could use
**Stable Baselines3** ’s implementations but would need a custom environment wrapper. However, to keep it
lightweight, implementing a small training loop manually is also feasible.

**Neuromorphic angle:** If using SNNs or event-driven models, porting to C++ might involve specialized
libraries (like using an SNN library in C++ or even neuromorphic hardware). For example, Intel’s Loihi or IBM
TrueNorth are neuromorphic chips that could, in theory, consume event streams with very low latency.
But integrating those is beyond our immediate scope. Instead, one could simulate an SNN in C++ (there are
frameworks like BindsNET in Python, not sure about C++ ones). Alternatively, use an ANN approximation
that’s easier to handle.

**Major Limitations & Considerations:** - Synchronization between capture and the RL loop: If the agent is
too slow to process frames (not truly “lightweight”), a backlog of events could build up, increasing latency.
We should monitor processing time and possibly drop frames if needed (since the agent might not need to
see every single frame if it can’t keep up). - Overhead of Python: Garbage collection and GIL could hiccup.
We mitigate by using vectorized operations and possibly pin the process to a CPU core separate from
capture thread. - User interrupt and safety: The agent loop should be interruptible and ideally should time
out or exit safely if anything goes wrong (to not require killing the process if it gets stuck). Using
KeyboardInterrupt (Ctrl+C) or a special key as we showed is one way.

Finally, once the model proves effective, migrating fully to C++ (possibly as part of the same program as
capture) will cut down latency further. We’d remove the WebSocket and directly feed the model with in-
memory data, achieving end-to-end processing on GPU/CPU without context switches.

## Conclusion & Architectural Choices

We presented a multi-faceted implementation strategy for an event-based screen capture and streaming
system, akin to giving a computer “neuromorphic vision” of the desktop. **Approach 1 (NVFBC)** is the
preferred route for lowest latency and overhead: it directly taps the GPU’s framebuffer and provides

#### •

```
15
```

difference maps for efficient event generation. If NVFBC is available, it can capture with negligible
performance cost , although it may require a Quadro/RTX Enterprise GPU or driver tweaks.
**Approach 2** explores intercepting rendering calls; while powerful in concept (even sub-frame updates), it is
complex and less general. In practice, using the Desktop Duplication API is a robust way to get per-frame
dirty rectangles and is a good fallback if NVFBC isn’t usable. **Approach 3 (Virtual Display Driver)** offers
deep integration by duplicating the output to a software device, allowing direct access to drawing
operations and memory. This has the highest implementation complexity but could be the most flexible
in terms of capturing all change information.

We then detailed how to reconstruct the image from events and optionally overlay visual indicators to
represent the transient changes, providing a real-time mirror that highlights activity. This is useful for
verifying that our event capture is working correctly and understanding the spatio-temporal distribution of
screen changes.

For **streaming to an agent** , a WebSocket-based pipeline was proposed to keep things simple and portable,
though various IPC methods could be used. The data transmitted can be as lean as a list of changed
rectangles with pixel values, which the agent can use to update its own internal representation of the
screen.

On the RL side, we emphasized starting with a **Python prototype** for ease of development, using a
lightweight model and continuous processing loop. The model could range from conventional deep RL (with
minor adaptations for frame-by-frame processing of event-frames) to cutting-edge event-driven networks
that process each change as it comes. Notably, researchers have demonstrated that operating directly
on event streams (without forcing a fixed timestep) can significantly improve reactivity and even solve tasks
standard methods cannot. This validates our approach of treating screen changes in an asynchronous
manner. We also considered spiking neural networks as an interesting direction, given their alignment with
event-based data and efficiency.

The plan ensures that once the concept is proven, we can migrate critical components to C++ for
performance. Capturing and inference can then happen within the same process, potentially achieving
millisecond end-to-end latency (from a pixel change occurring to the agent reacting). We would need to
carefully manage threads (capture vs inference) and possibly use double-buffering for frames to avoid
blocking either process.

**Major architectural decisions & limitations:** - **Using GPU for capture vs CPU:** NVFBC (GPU) is fastest but
proprietary; Desktop Dup (CPU copy) is more broadly available but costs some performance. A hybrid might
attempt NVFBC first and failover to Dup if unavailable. - **Data volume vs latency:** Sending raw pixel diffs
over network can saturate bandwidth if the screen is high-res and changes a lot. If latency is priority, we
might skip heavy compression, but if bandwidth is an issue, some encoding (like video codec or at least
region merging) might be needed. An alternative approach is to actually stream a standard encoded video
to the agent and have the agent use a frame-differencing approach; however, video compression adds
latency and is not event-by-event, so we avoided that. - **Complexity of driver approach:** The virtual driver
method gives the finest control but at the cost of driver development and maintenance. Unless absolutely
needed for extra low-level events, one might avoid this if NVFBC or Desktop Dup suffice. However, if the
project’s aim is also to experiment with capturing at the _draw call_ level, the driver provides that insight (e.g.,
you could potentially detect that “a line was drawn” vs “an image blit occurred”). - **Agent design:** The RL
model design will heavily depend on the eventual task (e.g., is the agent trying to mimic the user, or

```
4
1 2
```
```
8
```
```
10
```
```
14
```
```
14
```
```
15
```

perform its own task on the UI?). We outlined a general approach but the reward design and training
procedure would be task-specific. Ensuring the agent is lightweight enough (perhaps by limiting input size
or network size) is key to keep real-time speed.

In summary, the implementation will likely proceed in stages: start with NVFBC (or Desktop Dup) capturing
diff events and showing them on a mirror window, stream those events to a simple Python agent that logs
or randomly acts (to test the pipeline), then incrementally build the RL logic to make it intelligent.
Throughout, focus is on **low latency** , meaning use of GPU where possible, avoiding unnecessary copies (as
demonstrated by NVFBC’s GPU->GPU path ), and keeping processing pipelines parallel where feasible. By
carefully combining these techniques, we lay the foundation for a system that perceives screen changes in a
sparse, event-driven way and can eventually drive AI agents that respond with minimal delay.

**Sources:**

```
NVIDIA Capture SDK Documentation – Difference Maps and NVFBC usage
Markus Semrau, OSR Forum – Mirror Driver change tracking explanation
Microsoft DXGI Desktop Duplication API – Dirty rect retrieval
Reddit: NvFBC-Relay by user nmacleod – Performance of NVFBC vs DXGI and usage of GPU surfaces
```
```
Event-based RL (CERiL, 2023) – Advantages of continuous event stream for RL
Scaramuzza Lab, UZH – Spiking neural nets processing asynchronous events efficiently
```
NvFBC-Relay: A new utility to maximize performance in a dual-PC recording/streaming setup :
r/nvidia
https://www.reddit.com/r/nvidia/comments/1auf304/nvfbcrelay_a_new_utility_to_maximize_performance/

developer.download.nvidia.com
https://developer.download.nvidia.com/designworks/capture-sdk/docs/7.0/NVIDIA-Capture-SDK-Programming-Guide.pdf

IDXGIOutputDuplication::GetFrameDirtyRects (dxgi1_2.h) - Win32 apps | Microsoft Learn
https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutputduplication-getframedirtyrects

How to capture desktop change using mirror driver - NTDEV - OSR Developer Community
https://community.osr.com/t/how-to-capture-desktop-change-using-mirror-driver/

Virtual Display Drivers Knowledge Base | Getting Rid From ... - GitHub
https://github.com/pavlobu/deskreen/discussions/

Indirect Display Driver Model Overview - Windows - Learn Microsoft
https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-model-overview

[2302.07667] CERiL: Continuous Event-based Reinforcement Learning
https://arxiv.org/abs/2302.

Event Cameras, Event camera SLAM, Event-based Vision, Event-based Camera, Event SLAM
https://rpg.ifi.uzh.ch/research_dvs.html

```
7
```
#### •^43

#### •^10

#### •^89

#### •^1

```
7
```
-^14
-^15

```
1 2 6 7
```
```
3 4 5
```
```
8 9
```
```
10 13
```
```
11
```
```
12
```
```
14
```
```
15
```

