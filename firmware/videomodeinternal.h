#ifndef _VIDEO_MODE_INTERNAL_H_
#define _VIDEO_MODE_INTERNAL_H_

#include <cstdint>
#include <videomode.h>


//----------------------------------------------------------------------------
// Video Mode Driver data structures and prototypes

constexpr int MAX_WINDOWS = 64;

struct ScanlineSpan
{
    int windowListIndex; // index in windowList
    uint16_t start;
    uint16_t length;
};

// A ScanlineRange represents a series of scanlines that all have the same spans
struct ScanlineRange
{
    uint16_t start;
    uint16_t count;
    std::vector<ScanlineSpan> spans;
};

struct Window
{
    int id = -1;
    int mode = -1;
    int16_t position[2]; // Could be negative, would result in starting off screen top and/or left
    int16_t size[2]; // negative values are invalid
    uint32_t subsystemRootOffset;
    uint32_t modeRootOffset;
};

typedef uint32_t (*VideoSubsystemAllocateFunc)(size_t size);

struct VideoModeDriver
{
    virtual const char *getName() const = 0;
    virtual VideoModeType getModeType() const = 0;
    virtual void getAspectRatio(int *aspectX, int *aspectY) const = 0;
    virtual bool isMonochrome() const = 0; // NTSC might use this, e.g., to turn off colorburst if FRONT window is monochrome

    // fixed size, e.g. TMS9918A or Woz or Channel F, NTSC knows
    // that if this gives minimum Size 704x460, this mode is fullscreen
    // overscan only
    virtual bool isFixedSize() const = 0;

    // if a fixed size, return fixed size
    virtual void getMinimumSize(int *w, int *h) const = 0;

    // mostly for text modes - only succeed for multiples of text
    // size, put nearest smaller window size in W and H (for outline
    // during resizing)
    virtual void getNearestNotLarger(int w, int h, int *nearestNotLargerW, int *nearestNotLargerH) = 0;

    /*
    Ranges are always clipped to the screen dimensions before calling this function
    allocates per-scanline or per-window as necessary, keeps track of its own kind of allocation
    might make a per-window allocation e.g. colormap
    allocate a "root" buffer containing additional allocations and store root buffer in rootOffset
        e.g. color tables and then scanline data for pixmaps
        or window + windowspans
        or allocate one big buffer and carve everything up in there e.g. text?, TMS9918, Wolfenstein
    copy old allocations from vram to new allocations in vramTemp if copyContents
        e.g. color tables, but leave scanline pixmap regions junk or 0
        e.g. copy text because all text kept around?
        e.g. copy TMS9918 data or Wolfenstein data or segments
    if !copyContents, then window system is probably trying to resize or create a new window and testing a layout.
    if copyContents and all windows succeed, then rootOffsets will be changed and vramTemp will be copied to
        vram as quickly as possible during VBLANK
    therefore no pointers within VRAM, only offsets
    if vram is nullptr and oldOffset is 0xFFFFFFFF, then window didn't exist before
    if driver sets enqueueRedrawEvents, then subsystem should enqueue REDRAW for at least all exposed regions
        a backing store window like advanced Pixmap or Text or Wolfenstein could set to false;
    */
    static constexpr uint32_t ALLOCATION_FAILED = 0xFFFFFFFF;
    virtual bool reallocateForWindow(Window& window,
        const std::vector<ScanlineRange>& ranges,
        void* stagingVRAM, VideoSubsystemAllocateFunc allocate,
        bool *enqueueExposedRedrawEvents) = 0;
};


//----------------------------------------------------------------------------
// Pixmap (palettized packed bitmap)

struct PixmapModeDriver
{
    virtual PixmapFormat getPixmapFormat() const = 0;
    virtual void getPixelScale(int *scaleX, int *scaleY) const = 0;
    virtual PaletteSize getPaletteSize() const = 0;
    virtual void setPaletteContents(Window& window,
        PaletteIndex which, unsigned char (*palette)[3]) = 0;
    virtual void setRowPalette(Window& window,
        int row, PaletteIndex which) = 0;
    virtual void drawPixelRect(Window& window,
        int left, int top, int width, int height,
        size_t rowBytes, unsigned char *pixmap) = 0; // packed like pixel format
};


//----------------------------------------------------------------------------
// Textport

struct TextportModeDriver
{
    virtual void setCursorPosition(Window& window, int col, int row) = 0;
    virtual void setCursorAttributes(Window& window, uint32_t attributes) = 0;
    virtual void clearArea(Window& window, int col, int row, int cols, int rows) = 0;
    virtual void drawChar(Window& window, int col, int row, char c) = 0;
    virtual void copyArea(Window& window, int col, int row, int cols, int rows, int newcol, int newrow) = 0;
    virtual void drawArea(Window& window, int col, int row, int cols, int rows, const char *text) = 0;
};


//----------------------------------------------------------------------------
// Other video modes, finalizing TBD

struct SegmentModeDriver : public VideoModeDriver
{
    // but this is variable size?  maybe there can only ever
    // be one of these and its reallocated last and allocates as
    // much as it can.
    virtual int setScanlineSegments(Window& window, int scanlineCount, const VideoSegmentedScanline *scanlines) = 0;
};

struct WolfensteinModeDriver : public VideoModeDriver
{
    virtual void setElements(Window& window, const VideoWolfensteinElement* elements) = 0;
};

struct DCTModeDriver : public VideoModeDriver
{
};

struct WozModeDriver : public VideoModeDriver
{
};

struct TMS9918AModeDriver : public VideoModeDriver
{
};

struct VESModeDriver : public VideoModeDriver
{
};


//----------------------------------------------------------------------------
// VideoSubsystem

struct VideoSubsystemDriver
{
    virtual void start() = 0;
    virtual int getModeCount() const = 0;
    virtual void setBackgroundColor(float r, float g, float b) = 0;
    virtual VideoModeDriver* getModeDriver(int n) const = 0;
    virtual void setDebugRow(int row, const char *rowText) = 0; // system sets debug information
    virtual void waitFrame() = 0;
    virtual void stop() = 0;
    virtual bool attemptWindowConfiguration(std::vector<Window>& windowsBackToFront) = 0;
};


//----------------------------------------------------------------------------
// NTSC-specific video

struct NTSCModeDriver : public VideoModeDriver
{
    virtual void fillRow(uint32_t rootOffset, uint32_t startOnScreen /* for color phase */, uint32_t startWithinWindow, uint32_t length, uint32_t rowOnScreen /* for flipping ColorBurst */, uint32_t rowWithinWindow, uint8_t *rowBuffer) = 0;
};

extern "C" {

void VideoSetSubsystem(VideoSubsystemDriver *drv);
void VideoStopSubsystem();
VideoSubsystemDriver* GetNTSCVideoSubsystem();
void NTSCVideoRegisterDriver(NTSCModeDriver* driver);

extern uint8_t *VRAM;
size_t GetVRAMSize();

};

template <typename T>
static T& GetAllocationFromBuffer(void *buffer, size_t offset)
{
    return *(T*)(((uint8_t*)buffer) + offset);
}


#endif /* _VIDEO_MODE_INTERNAL_H_ */
