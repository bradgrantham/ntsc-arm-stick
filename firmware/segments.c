#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

int debug = 1;
int validateEverything = 1;

typedef struct VideoSegmentedScanlineSegment
{
    /* 
       Function providing this data is expected to provide sequential,
       nonoverlapping, pixel-center-sampled segments covering the
       entire scanline and no more, as an example converted from
       multiple, subpixel, overlapping segments.  A later extension
       might include partial coverage and possibly multiple fractional
       coverage per pixel (e.g. to enable pure analytic antialiasing)
    */
    uint16_t pixelCount;
    float r0, g0, b0;
    float r1, g1, b1;
} VideoSegmentedScanlineSegment;

void DumpSegment(VideoSegmentedScanlineSegment *segment, int currentStart, int indent)
{
    printf("%*s%d: {%f,%f,%f} to {%f,%f,%f} over %d pixels\n", indent, "",
        currentStart,
        segment->r0, segment->g0, segment->b0,
        segment->r1, segment->g1, segment->b1,
        segment->pixelCount);
}

void DumpSegments(VideoSegmentedScanlineSegment *segment, int pixelCount, int indent)
{
    int currentStart = 0;
    while(currentStart < pixelCount) {
        DumpSegment(segment, currentStart, indent);
        currentStart += segment->pixelCount;
        segment++;
    }
}

void SetSegment(VideoSegmentedScanlineSegment *seg, int pixelCount, float r0, float g0, float b0, float r1, float g1, float b1)
{
    seg->pixelCount = pixelCount;
    seg->r0 = r0;
    seg->g0 = g0;
    seg->b0 = b0;
    seg->r1 = r1;
    seg->g1 = g1;
    seg->b1 = b1;
}

int CalculateSegmentEnd(VideoSegmentedScanlineSegment *seg, int start)
{
    return start + seg->pixelCount - 1;
}

void ClipSegmentWithNewStart(VideoSegmentedScanlineSegment *src, int oldStart, int newStart, VideoSegmentedScanlineSegment *dst)
{
    if(debug) printf("    ClipSegmentWithNewStart(..., %d, %d, ...)\n", oldStart, newStart);
    dst->pixelCount = src->pixelCount - (newStart - oldStart);
    dst->r0 = src->r0 + (src->r1 - src->r0) / src->pixelCount * (newStart - oldStart);
    dst->g0 = src->g0 + (src->g1 - src->g0) / src->pixelCount * (newStart - oldStart);
    dst->b0 = src->b0 + (src->b1 - src->b0) / src->pixelCount * (newStart - oldStart);
    dst->r1 = src->r1;
    dst->g1 = src->g1;
    dst->b1 = src->b1;
}

void ClipSegmentWithNewEnd(VideoSegmentedScanlineSegment *src, int newCount, VideoSegmentedScanlineSegment *dst)
{
    dst->pixelCount = newCount;
    dst->r0 = src->r0;
    dst->g0 = src->g0;
    dst->b0 = src->b0;
    dst->r1 = src->r0 + (src->r1 - src->r0) / src->pixelCount * newCount;
    dst->g1 = src->g0 + (src->g1 - src->g0) / src->pixelCount * newCount;
    dst->b1 = src->b0 + (src->b1 - src->b0) / src->pixelCount * newCount;
}

int MergeSegment(VideoSegmentedScanlineSegment *newSegment, int start, VideoSegmentedScanlineSegment *oldSegments, int pixelCount, VideoSegmentedScanlineSegment *resultSegments, int maxNewSegmentCount, int *newCount)
{
    VideoSegmentedScanlineSegment *src = oldSegments;
    VideoSegmentedScanlineSegment *dst = resultSegments;
    int srcSegStart = 0;
    int segmentCount = 0;

    // If new segment is zero length, it wouldn't insert anything, so return success;
    if(newSegment->pixelCount == 0) {
        return 0;
    }

    // Copy all segments entirely before this new segment
    while(CalculateSegmentEnd(src, srcSegStart) < start) {
        if(segmentCount >= maxNewSegmentCount) {
            return 1; // out of space copying start segments
        }
        if(debug) printf("    copy old segment before\n");
        *dst = *src;
        srcSegStart += src->pixelCount; segmentCount++; dst++; src++;
    }

    // Copy the segment that is overlapped by the start of the new segment, if there is one
    if(srcSegStart < start) {
        if(segmentCount >= maxNewSegmentCount) {
            return 2; // out of space clipping segment overlapped at beginning
        }
        if(debug) printf("    copy old segment overlapping beginning\n");
        ClipSegmentWithNewEnd(src, start - srcSegStart, dst);
        if(debug) {
            printf("    new segment = ");
            DumpSegment(dst, srcSegStart, 0);
        }
        segmentCount++; dst++;
    }

    // Put in the new segment
    if(segmentCount >= maxNewSegmentCount) {
        return 3; // out of space copying new segment
    }
    if(debug) printf("    copy in new segment\n");

    dst->r0 = newSegment->r0;
    dst->g0 = newSegment->g0;
    dst->b0 = newSegment->b0;
    dst->r1 = newSegment->r1;
    dst->g1 = newSegment->g1;
    dst->b1 = newSegment->b1;
    dst->pixelCount = newSegment->pixelCount;
    segmentCount++;
    dst++;

    if(debug) {
        printf("    next segment entering skip loop body = ");
        DumpSegment(src, srcSegStart, 0);
    }

    // Skip until segments not covered by the new segment
    while( (srcSegStart <= CalculateSegmentEnd(newSegment, start)) &&
        (CalculateSegmentEnd(src, srcSegStart) <= CalculateSegmentEnd(newSegment, start))) {

        if(debug) printf("    skip old overlapped segment\n");
        srcSegStart += src->pixelCount;
        src++;
        if(debug) {
            if( (srcSegStart <= CalculateSegmentEnd(newSegment, start)) &&
                (CalculateSegmentEnd(src, srcSegStart) <= CalculateSegmentEnd(newSegment, start))) {

                printf("    possible next segment end of skip loop body = ");
                DumpSegment(src, srcSegStart, 0);
            }
        }
    }

    // Copy the segment that is overlapped by the end of the new segment, if there is one
    if(srcSegStart <= CalculateSegmentEnd(newSegment, start)) {
        if(segmentCount >= maxNewSegmentCount) {
            return 4; // out of space clipping segment overlapped at end
        }
        if(debug) printf("    copy segment overlapped at end\n");
        ClipSegmentWithNewStart(src, srcSegStart, CalculateSegmentEnd(newSegment, start) + 1, dst);
        if(debug) {
            printf("    new segment = ");
            DumpSegment(dst, srcSegStart, 0);
        }
        srcSegStart += src->pixelCount; segmentCount++; src++; dst++;
    }

    // Copy all segments entirely after this new segment
    while(srcSegStart < pixelCount) {
        if(segmentCount >= maxNewSegmentCount) {
            return 5; // out of space copying end segments
        }
        if(debug) printf("    copy segment after end of new segment\n");
        *dst = *src;
        srcSegStart += src->pixelCount; segmentCount++; dst++; src++;
    }

    *newCount = segmentCount; // For validation purposes, not used by video mode
    return 0;
}

int ValidateSegments(VideoSegmentedScanlineSegment *segments, int segmentCount, int pixelCount)
{
    int totalPixels = 0;

    for(int i = 0; i < segmentCount; i++) {
        if(segments[i].pixelCount == 0) {
            return 1; // a segment has to have non-zero length
        }

        if(segments[i].pixelCount > pixelCount) {
            return 2; // no segment can be larger than the scanline
        }

        totalPixels += segments[i].pixelCount;
    }

    if(totalPixels < pixelCount) {
        return 3; // Too few pixels covered
    }

    if(totalPixels > pixelCount) {
        printf("%d, %d\n", totalPixels, pixelCount);
        return 4; // Too many pixels covered
    }

    return 0; // covers exactly all pixels
}

enum {
    SEGMENT_MAX = 12, 
    PIXEL_COUNT = 704, 
    IMAGE_HEIGHT = 460, 
};

float pixelRows[2][PIXEL_COUNT][3];

int PaintSegment(VideoSegmentedScanlineSegment *seg, int start, float (*pixelRow)[3], int pixelCount)
{
    if(start + seg->pixelCount > pixelCount) {
        return 1; // too long
    }

    float dr = (seg->r1 - seg->r0) / seg->pixelCount;
    float dg = (seg->g1 - seg->g0) / seg->pixelCount;
    float db = (seg->b1 - seg->b0) / seg->pixelCount;
    float r = seg->r0;
    float g = seg->g0;
    float b = seg->b0;

    for(int i = start; i < start + seg->pixelCount; i++) {
        pixelRow[i][0] = r;
        pixelRow[i][1] = g;
        pixelRow[i][2] = b;
        r += dr;
        g += dg;
        b += db;
    }

    return 0;
}

int PaintSegments(VideoSegmentedScanlineSegment *segs, float (*pixelRow)[3], int pixelCount)
{
    int currentStart = 0;
    VideoSegmentedScanlineSegment *seg = segs;

    while(currentStart < pixelCount) {
        int result = PaintSegment(seg, currentStart, pixelRow, pixelCount);
        if(result != 0) {
            return result;
        }
        currentStart += seg->pixelCount;
        seg++;
    }
    return 0;
}

int CompareRows(float (*pixelRow0)[3], float (*pixelRow1)[3], int pixelCount)
{
    for(int i = 0; i < pixelCount; i++) {
        for(int c = 0; c < 3; c++) {
            if(fabsf(pixelRow0[i][c] - pixelRow1[i][c]) > .00001f) {
                return i + 1;
            }
        }
    }
    return 0;
}

int TestMerge(VideoSegmentedScanlineSegment *newseg, int start, VideoSegmentedScanlineSegment *src, int segmentCount, int pixelCount, VideoSegmentedScanlineSegment *dst, int maxNewSegmentCount, int indent)
{
    int newSegmentCount;
    int result;

    result = ValidateSegments(src, segmentCount, pixelCount);
    if(result != 0) {
        printf("%*svalidation of test input failed with %d\n", indent, "", result);
        return 1000 + result;
    }

    if(start > pixelCount - 1) {
        printf("%*snew segment is %d and that's past pixelCount %d\n", indent, "", start, pixelCount);
        return 1; // new segment starts too late
    }

    if(start + newseg->pixelCount > pixelCount) {
        printf("%*snew segment ends at %d and that's past end %d\n", indent, "", start + newseg->pixelCount, pixelCount);
        return 2; // new segment ends too late
    }

    result = MergeSegment(newseg, start, src, pixelCount, dst, maxNewSegmentCount, &newSegmentCount);

    if(result != 0) {
        return 2000 + result; // error from MergeSegment
    }

    if(newSegmentCount > segmentCount + 2) {
        printf("%*snew segment count %d is too much larger than old count %d\n", indent, "", newSegmentCount, segmentCount);
        return 3; // unexpectedly larger
    }

    result = ValidateSegments(dst, newSegmentCount, pixelCount);
    if(result != 0) {
        printf("%*svalidation of test results failed with %d\n", indent, "", result);
        return 3000 + result;
    }

    for(int i = 0; i < pixelCount; i++) { for(int c = 0; c < 3; c++) { pixelRows[0][i][c] = 666.666; } }
    PaintSegments(src, pixelRows[0], pixelCount);
    PaintSegment(newseg, start, pixelRows[0], pixelCount);

    for(int i = 0; i < pixelCount; i++) { for(int c = 0; c < 3; c++) { pixelRows[1][i][c] = 777.777; } }
    PaintSegments(dst, pixelRows[1], pixelCount);

    result = CompareRows(pixelRows[0], pixelRows[1], pixelCount);
    if(result != 0) {
        printf("%*scomparison of test results with paint failed with %d\n", indent, "", result);
        return 4000 + result;
    }

    return 0;
}

int image[IMAGE_HEIGHT][PIXEL_COUNT][3];

VideoSegmentedScanlineSegment imageSegments[IMAGE_HEIGHT][SEGMENT_MAX];
int imageSegmentsCount[IMAGE_HEIGHT]; // only for validation

int ClearSegmentedScreen(float r, float g, float b)
{
    for(int i = 0; i < IMAGE_HEIGHT; i++) {
        VideoSegmentedScanlineSegment *seg = imageSegments[i];
        seg->pixelCount = PIXEL_COUNT;
        seg->r0 = .2f; seg->g0 = .15f; seg->b0 = .15f;
        seg->r1 = .2f; seg->g1 = .15f; seg->b1 = .15f;
        imageSegmentsCount[i] = 1;
#ifndef ROCINANTE
        if(validateEverything) {
            int result = ValidateSegments(imageSegments[i], imageSegmentsCount[i], PIXEL_COUNT);
            if(result != 0) {
                printf("result %d validating cleared row %d\n", result, i);
                return 1;
            }
        }
#endif /* ROCINANTE */
    }
    return 0;
}

int ScanconvertSphere(int cx, int cy, int cr, float r, float g, float b)
{
    VideoSegmentedScanlineSegment newseg;
    static VideoSegmentedScanlineSegment tmpSegments[SEGMENT_MAX];

    int miny = cy - cr + 1;
    int maxy = cy + cr - 1;

    miny = (miny >= 0) ? miny : 0;
    maxy = (maxy < IMAGE_HEIGHT) ? maxy : 0;

    for(int row = miny; row <= maxy; row++) {

        int y = row - cy;
        int x = sqrtf(cr * cr - y * y);

        newseg.r0 = r;
        newseg.g0 = g;
        newseg.b0 = b;
        newseg.r1 = r;
        newseg.g1 = g;
        newseg.b1 = b;

        int start = cx - x;
        int end = cx + x;

        if(start < 0) {
            newseg.pixelCount += -start;
            start = 0;
        }
        if(end >= PIXEL_COUNT) {
            end = PIXEL_COUNT - 1;
        }
        newseg.pixelCount = end - start + 1;

        int result = MergeSegment(&newseg, start, imageSegments[row], PIXEL_COUNT, tmpSegments, SEGMENT_MAX, &imageSegmentsCount[row]);
        if(result != 0) {
            printf("error scanconverting sphere at row %d\n", row);
            return result;
        }
#ifndef ROCINANTE
        if(validateEverything) {
            result = ValidateSegments(tmpSegments, imageSegmentsCount[row], PIXEL_COUNT);
            if(result != 0) {
                printf("result %d validating sphere row %d\n", result, row);
                return 1;
            }
        }
#endif
        memcpy(imageSegments[row], tmpSegments, imageSegmentsCount[row] * sizeof(VideoSegmentedScanlineSegment));
    }
    return 0;
}

int SpheresTest()
{
    int result;

    result = ClearSegmentedScreen(.2f, .15f, .15f);
    if(result != 0) {
        return result;
    }

    for(int sphere = 0; sphere < 10; sphere++) {
        result = ScanconvertSphere(10 + drand48() * (PIXEL_COUNT - 20), 10 + drand48() * (IMAGE_HEIGHT - 20), 50 + drand48() * 50, drand48(), drand48(), drand48());
        if(result != 0) {
            printf("result %d drawing sphere %d\n", result, sphere);
            return 1;
        }
        if(validateEverything) {
            for(int i = 0; i < IMAGE_HEIGHT; i++) {
                result = ValidateSegments(imageSegments[i], imageSegmentsCount[i], PIXEL_COUNT);
                if(result != 0) {
                    printf("result %d validating sphere %d row %d segments\n", result, sphere, i);
                    return 1;
                }
            }
        }
    }

    int totalSegments = 0;
    int maxSegments = 0;
    for(int i = 0; i < IMAGE_HEIGHT; i++) {
        result = ValidateSegments(imageSegments[i], imageSegmentsCount[i], PIXEL_COUNT);
        if(result != 0) {
            printf("result %d validating drawn sphere row %d segments\n", result, i);
            return 1;
        }
        totalSegments += imageSegmentsCount[i];
        maxSegments = (imageSegmentsCount[i] > maxSegments) ? imageSegmentsCount[i] : maxSegments;
    }
    printf("maxSegments = %d\n", maxSegments);
    printf("totalSegments = %d\n", totalSegments);

    FILE *fp = fopen("output.ppm", "wb");
    fprintf(fp, "P6 %d %d 255\n", PIXEL_COUNT, IMAGE_HEIGHT);
    for(int i = 0; i < IMAGE_HEIGHT; i++) {
        PaintSegments(imageSegments[i], pixelRows[0], PIXEL_COUNT);
        for(int j = 0; j < PIXEL_COUNT; j++) {
            unsigned char rgb[3];
            rgb[0] = pixelRows[0][j][0] * 255;
            rgb[1] = pixelRows[0][j][1] * 255;
            rgb[2] = pixelRows[0][j][2] * 255;
            fwrite(rgb, sizeof(rgb), 1, fp);
        }
    }
    fclose(fp);

    return 0;
}

int main()
{
    printf("sizeof(imageSegments) == %zd\n", sizeof(imageSegments));

    int result;
    VideoSegmentedScanlineSegment newseg;

    {
        for(int i = 0; i < PIXEL_COUNT; i++) { for(int c = 0; c < 3; c++) { pixelRows[0][i][c] = 666.666; } }
        for(int i = 0; i < PIXEL_COUNT; i++) { for(int c = 0; c < 3; c++) { pixelRows[1][i][c] = 777.777; } }
        result = CompareRows(pixelRows[0], pixelRows[1], PIXEL_COUNT);
        if(result == 0) {
            printf("comparison of different rows succeeded unexpectedly\n");
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < PIXEL_COUNT; i++) { for(int c = 0; c < 3; c++) { pixelRows[1][i][c] = 666.666; } }
        pixelRows[1][0][0] = 5.0f;
        result = CompareRows(pixelRows[0], pixelRows[1], PIXEL_COUNT);
        if(result == 0) {
            printf("comparison of different rows succeeded unexpectedly\n");
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < PIXEL_COUNT; i++) { for(int c = 0; c < 3; c++) { pixelRows[1][i][c] = 666.666; } }
        pixelRows[1][0][1] = 5.0f;
        result = CompareRows(pixelRows[0], pixelRows[1], PIXEL_COUNT);
        if(result == 0) {
            printf("comparison of different rows succeeded unexpectedly\n");
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < PIXEL_COUNT; i++) { for(int c = 0; c < 3; c++) { pixelRows[1][i][c] = 666.666; } }
        pixelRows[1][0][2] = 5.0f;
        result = CompareRows(pixelRows[0], pixelRows[1], PIXEL_COUNT);
        if(result == 0) {
            printf("comparison of different rows succeeded unexpectedly\n");
            exit(EXIT_FAILURE);
        }
    }


    if(0){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT, 0, 0, 0, 1, 1, 1},
        };
        printf("test dumping one segment:\n");
        DumpSegments(segs1, PIXEL_COUNT, 4);
    }

    if(0){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT - 10, 0, 0, 0, 1, 1, 1},
            {10, 1, 1, 1, 0, 0, 0},
        };
        printf("test dumping more than one segment:\n");
        DumpSegments(segs1, PIXEL_COUNT, 4);
    }
    
    {
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT, 0, 0, 0, 1, 1, 1},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("add middle segment:\n");
        SetSegment(&newseg, 100, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, 100, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT, 0, 0, 0, 1, 1, 1},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("add segment at start:\n");
        SetSegment(&newseg, 100, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, 0, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT, 0, 0, 0, 1, 1, 1},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("add segment at end:\n");
        SetSegment(&newseg, 100, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, PIXEL_COUNT - 100, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2, 0, 0, 0, 1, 0, 0},
            {PIXEL_COUNT / 2, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("overlapping two segments:\n");
        SetSegment(&newseg, 200, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, PIXEL_COUNT / 2 - 100, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2 - 50, 0, 0, 0, 1, 0, 0},
            {100, 0, 0, 0, 0, 0, 1},
            {PIXEL_COUNT / 2 - 50, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("completely overlap a segment\n");
        SetSegment(&newseg, 200, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, PIXEL_COUNT / 2 - 100, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2 - 100, 0, 0, 0, 1, 0, 0},
            {200, 0, 0, 0, 0, 0, 1},
            {PIXEL_COUNT / 2 - 100, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("completely replace a segment\n");
        SetSegment(&newseg, 200, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, PIXEL_COUNT / 2 - 100, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2 - 100, 0, 0, 0, 1, 0, 0},
            {100, 0, 0, 0, 0, 0, 1},
            {100, 0, 0, 0, 0, 0, 1},
            {PIXEL_COUNT / 2 - 100, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("completely replace two segments\n");
        SetSegment(&newseg, 100, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, PIXEL_COUNT / 2 - 100, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2, 0, 0, 0, 1, 0, 0},
            {PIXEL_COUNT / 2, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("completely replace first segment:\n");
        SetSegment(&newseg, PIXEL_COUNT / 2, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, 0, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }

    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2, 0, 0, 0, 1, 0, 0},
            {PIXEL_COUNT / 2, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("completely replace last segment:\n");
        SetSegment(&newseg, PIXEL_COUNT / 2, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, PIXEL_COUNT/2, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }

    if(1){
        VideoSegmentedScanlineSegment segs1[] = {
            {PIXEL_COUNT / 2 - 100, 0, 0, 0, 1, 0, 0},
            {200, 0, 0, 0, 0, 0, 1},
            {PIXEL_COUNT / 2 - 100, 0, 0, 0, 0, 1, 0},
        };
        VideoSegmentedScanlineSegment segs2[SEGMENT_MAX];
        printf("completely replace all segments:\n");
        SetSegment(&newseg, PIXEL_COUNT, 1, 1, 1, 0, 0, 0);
        result = TestMerge(&newseg, 0, segs1, sizeof(segs1) / sizeof(segs1[0]), PIXEL_COUNT, segs2, SEGMENT_MAX, 4);
        if(result == 0) {
            DumpSegments(segs2, PIXEL_COUNT, 4);
        } else {
            printf("    test failed with %d\n", result);
            exit(EXIT_FAILURE);
        }
    }

    printf("spheres:\n");
    result = SpheresTest();
    if(result == 0) {
        printf("    test passed\n");
    } else {
        printf("    test failed with %d\n", result);
        exit(EXIT_FAILURE);
    }
}