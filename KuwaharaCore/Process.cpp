/*******************************************************************/
/*                                                                 */
/*                   Kuwahara Core Algorithm                       */
/*                                                                 */
/*******************************************************************/

#include "API.h"
#include "AE_EffectCB.h"
#include "AEGP_SuiteHandler.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// Helper structure for region statistics (using doubles for normalized values)
typedef struct {
    double sumR, sumG, sumB;
    double sumR2, sumG2, sumB2;
    A_long count;
} RegionStats;

// Calculate sum of channel variances from normalized statistics
static double CalculateVariance(const RegionStats& stats) {
    if (stats.count == 0) return 1e10; // Large value to avoid selection if invalid
    
    double meanR = stats.sumR / stats.count;
    double meanG = stats.sumG / stats.count;
    double meanB = stats.sumB / stats.count;
    
    double varR = (stats.sumR2 / stats.count) - (meanR * meanR);
    double varG = (stats.sumG2 / stats.count) - (meanG * meanG);
    double varB = (stats.sumB2 / stats.count) - (meanB * meanB);
    
    return varR + varG + varB;
}

// Brute-force ProcessKuwaharaPixel8 (normalized doubles, hard selection)
PF_Err ProcessKuwaharaPixel8(
    const PF_EffectWorld* input,
    A_long x,
    A_long y,
    A_long radius,
    PF_Pixel8* outP)
{
    if (!input || !outP) return PF_Err_BAD_CALLBACK_PARAM;

    RegionStats regions[4] = {{0}};

    // Define quadrant starts (inclusive)
    A_long q_starts[4][2] = {
        {x - radius, y - radius}, // Top-left
        {x, y - radius},          // Top-right
        {x - radius, y},          // Bottom-left
        {x, y}                    // Bottom-right
    };

    for (int i = 0; i < 4; i++) {
        A_long startX = q_starts[i][0];
        A_long startY = q_starts[i][1];
        A_long endX = startX + radius;
        A_long endY = startY + radius;

        // Clamp to image bounds
        startX = std::max<A_long>(0, startX);
        startY = std::max<A_long>(0, startY);
        endX = std::min<A_long>(input->width - 1, endX);
        endY = std::min<A_long>(input->height - 1, endY);

        for (A_long py = startY; py <= endY; ++py) {
            const PF_Pixel8* row = (const PF_Pixel8*)((const char*)input->data + py * input->rowbytes);
            for (A_long px = startX; px <= endX; ++px) {
                const PF_Pixel8& pixel = row[px];
                double r = pixel.red / 255.0;
                double g = pixel.green / 255.0;
                double b = pixel.blue / 255.0;

                regions[i].sumR += r;
                regions[i].sumG += g;
                regions[i].sumB += b;
                regions[i].sumR2 += r * r;
                regions[i].sumG2 += g * g;
                regions[i].sumB2 += b * b;
                regions[i].count++;
            }
        }
    }

    // Find region with minimum variance
    double minVariance = 1e10;
    int bestRegion = -1;
    for (int i = 0; i < 4; i++) {
        if (regions[i].count > 0) {
            double variance = CalculateVariance(regions[i]);
            if (variance < minVariance) {
                minVariance = variance;
                bestRegion = i;
            }
        }
    }

    // Set output to mean of best region (denormalize)
    if (bestRegion != -1) {
        double meanR = regions[bestRegion].sumR / regions[bestRegion].count;
        double meanG = regions[bestRegion].sumG / regions[bestRegion].count;
        double meanB = regions[bestRegion].sumB / regions[bestRegion].count;

        outP->red = static_cast<A_u_char>(meanR * 255.0 + 0.5);
        outP->green = static_cast<A_u_char>(meanG * 255.0 + 0.5);
        outP->blue = static_cast<A_u_char>(meanB * 255.0 + 0.5);
    } else {
        // Fallback to original pixel
        const PF_Pixel8* srcP = (const PF_Pixel8*)((const char*)input->data + y * input->rowbytes) + x;
        *outP = *srcP;
        return PF_Err_NONE;
    }

    // Copy alpha from source
    const PF_Pixel8* srcP = (const PF_Pixel8*)((const char*)input->data + y * input->rowbytes) + x;
    outP->alpha = srcP->alpha;

    return PF_Err_NONE;
}

// Brute-force ProcessKuwaharaPixel16 (normalized doubles, hard selection)
PF_Err ProcessKuwaharaPixel16(
    const PF_EffectWorld* input,
    A_long x,
    A_long y,
    A_long radius,
    PF_Pixel16* outP)
{
    if (!input || !outP) return PF_Err_BAD_CALLBACK_PARAM;

    RegionStats regions[4] = {{0}};

    // Define quadrant starts (inclusive)
    A_long q_starts[4][2] = {
        {x - radius, y - radius}, // Top-left
        {x, y - radius},          // Top-right
        {x - radius, y},          // Bottom-left
        {x, y}                    // Bottom-right
    };

    for (int i = 0; i < 4; i++) {
        A_long startX = q_starts[i][0];
        A_long startY = q_starts[i][1];
        A_long endX = startX + radius;
        A_long endY = startY + radius;

        // Clamp to image bounds
        startX = std::max<A_long>(0, startX);
        startY = std::max<A_long>(0, startY);
        endX = std::min<A_long>(input->width - 1, endX);
        endY = std::min<A_long>(input->height - 1, endY);

        for (A_long py = startY; py <= endY; ++py) {
            const PF_Pixel16* row = (const PF_Pixel16*)((const char*)input->data + py * input->rowbytes);
            for (A_long px = startX; px <= endX; ++px) {
                const PF_Pixel16& pixel = row[px];
                double r = pixel.red / 32768.0;
                double g = pixel.green / 32768.0;
                double b = pixel.blue / 32768.0;

                regions[i].sumR += r;
                regions[i].sumG += g;
                regions[i].sumB += b;
                regions[i].sumR2 += r * r;
                regions[i].sumG2 += g * g;
                regions[i].sumB2 += b * b;
                regions[i].count++;
            }
        }
    }

    // Find region with minimum variance
    double minVariance = 1e10;
    int bestRegion = -1;
    for (int i = 0; i < 4; i++) {
        if (regions[i].count > 0) {
            double variance = CalculateVariance(regions[i]);
            if (variance < minVariance) {
                minVariance = variance;
                bestRegion = i;
            }
        }
    }

    // Set output to mean of best region (denormalize)
    if (bestRegion != -1) {
        double meanR = regions[bestRegion].sumR / regions[bestRegion].count;
        double meanG = regions[bestRegion].sumG / regions[bestRegion].count;
        double meanB = regions[bestRegion].sumB / regions[bestRegion].count;

        outP->red = static_cast<A_u_short>(meanR * 32768.0 + 0.5);
        outP->green = static_cast<A_u_short>(meanG * 32768.0 + 0.5);
        outP->blue = static_cast<A_u_short>(meanB * 32768.0 + 0.5);
    } else {
        // Fallback to original pixel
        const PF_Pixel16* srcP = (const PF_Pixel16*)((const char*)input->data + y * input->rowbytes) + x;
        *outP = *srcP;
        return PF_Err_NONE;
    }

    // Copy alpha from source
    const PF_Pixel16* srcP = (const PF_Pixel16*)((const char*)input->data + y * input->rowbytes) + x;
    outP->alpha = srcP->alpha;

    return PF_Err_NONE;
}

// Brute-force ProcessKuwaharaWorld8 (loop over pixels, apply mix)
PF_Err ProcessKuwaharaWorld8(
    PF_InData* in_data,
    PF_EffectWorld* input,
    PF_EffectWorld* output,
    A_long radius,
    PF_FpLong mix)
{
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    PF_FpLong mixFactor = mix / 100.0;

    for (A_long y = 0; y < output->height; ++y) {
        if (PF_ABORT(in_data) != PF_Err_NONE) {
            err = PF_Interrupt_CANCEL;
            break;
        }

        PF_Pixel8* outRow = (PF_Pixel8*)((char*)output->data + y * output->rowbytes);
        const PF_Pixel8* inRow = (const PF_Pixel8*)((const char*)input->data + y * input->rowbytes);

        for (A_long x = 0; x < output->width; ++x) {
            PF_Pixel8 filtered;
            err = ProcessKuwaharaPixel8(input, x, y, radius, &filtered);
            if (err != PF_Err_NONE) break;

            outRow[x].red = static_cast<A_u_char>(filtered.red * mixFactor + inRow[x].red * (1.0 - mixFactor) + 0.5);
            outRow[x].green = static_cast<A_u_char>(filtered.green * mixFactor + inRow[x].green * (1.0 - mixFactor) + 0.5);
            outRow[x].blue = static_cast<A_u_char>(filtered.blue * mixFactor + inRow[x].blue * (1.0 - mixFactor) + 0.5);
            outRow[x].alpha = inRow[x].alpha;
        }
        if (err != PF_Err_NONE) break;
    }

    return err;
}

// Brute-force ProcessKuwaharaWorld16 (loop over pixels, apply mix)
PF_Err ProcessKuwaharaWorld16(
    PF_InData* in_data,
    PF_EffectWorld* input,
    PF_EffectWorld* output,
    A_long radius,
    PF_FpLong mix)
{
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    PF_FpLong mixFactor = mix / 100.0;

    for (A_long y = 0; y < output->height; ++y) {
        if (PF_ABORT(in_data) != PF_Err_NONE) {
            err = PF_Interrupt_CANCEL;
            break;
        }

        PF_Pixel16* outRow = (PF_Pixel16*)((char*)output->data + y * output->rowbytes);
        const PF_Pixel16* inRow = (const PF_Pixel16*)((const char*)input->data + y * input->rowbytes);

        for (A_long x = 0; x < output->width; ++x) {
            PF_Pixel16 filtered;
            err = ProcessKuwaharaPixel16(input, x, y, radius, &filtered);
            if (err != PF_Err_NONE) break;

            outRow[x].red = static_cast<A_u_short>(filtered.red * mixFactor + inRow[x].red * (1.0 - mixFactor) + 0.5);
            outRow[x].green = static_cast<A_u_short>(filtered.green * mixFactor + inRow[x].green * (1.0 - mixFactor) + 0.5);
            outRow[x].blue = static_cast<A_u_short>(filtered.blue * mixFactor + inRow[x].blue * (1.0 - mixFactor) + 0.5);
            outRow[x].alpha = inRow[x].alpha;
        }
        if (err != PF_Err_NONE) break;
    }

    return err;
}
