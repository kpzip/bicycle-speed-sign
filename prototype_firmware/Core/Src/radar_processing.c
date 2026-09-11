/* ***************************************************************************
** File name: radar_processing.c
**
** Description: This is the data processing source file for PSOC6 Radar Static Distance Code Example.
**
*******************************************************************************
* Copyright 2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/
#include <stdlib.h> // Provides functions for memory allocation and other utilities
#include <stdio.h>  // Provides input/output functions like printf and scanf
#include "radar_processing.h"
#include "cyhal.h"
#include "cybsp.h"

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Static distance context instance */
static_distance_context_t context;

/*******************************************************************************
* Function Name: init_static_distance
****************************************************************************//**
* Initializes parameters, buffers, FFT instance, and window function for static
* distance measurement.
*
* \param ctx
* Pointer to the static distance context structure.
*
* \return cy_rslt_t
* Returns CY_RSLT_SUCCESS if initialization is successful, otherwise an error code.
*******************************************************************************/
cy_rslt_t init_static_distance(static_distance_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    
    /* Check for valid algorithm configurations */
	if ((max_range_m < min_range_m) || (zeroPadding_factor != 1 && zeroPadding_factor != 2 && zeroPadding_factor != 4 && zeroPadding_factor != 8)) {
		printf("Invalid algorithm configurations!\r\n");
		CY_ASSERT(0);
	}

    /* Calculate parameters */
    ctx->num_samples_zeroPadded = XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP * zeroPadding_factor;
    ctx->fft_size = ctx->num_samples_zeroPadded / 2;
    int64_t bandwidth = (XENSIV_BGT60TRXX_CONF_END_FREQ_HZ) - (XENSIV_BGT60TRXX_CONF_START_FREQ_HZ);
    ctx->bin_len = ifx_range_resolution(bandwidth);
    ctx->bin_len = ctx->bin_len / (ctx->num_samples_zeroPadded / XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP); //compensate for zero padding
    ctx->skip = (int)(min_range_m / ctx->bin_len)+1;
    ctx->max_range_bin = (int)(max_range_m / ctx->bin_len);

    /* Allocate dynamic buffers */
    ctx->zero_padded_samples = (float32_t *)malloc(XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME * ctx->num_samples_zeroPadded * sizeof(float32_t));
    ctx->fft_buffer = (cfloat32_t *)malloc(XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME * ctx->fft_size * sizeof(cfloat32_t));
    ctx->fft_buffer_real = (float32_t *)malloc(XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME * ctx->fft_size * sizeof(float32_t));
    ctx->fft_win = (float32_t *)malloc(ctx->num_samples_zeroPadded * sizeof(float32_t));
    ctx->integrated_chirp = (float32_t *)malloc(ctx->fft_size * sizeof(float32_t));

    /* Check for allocation failures */
    if (!ctx->zero_padded_samples || !ctx->fft_buffer || !ctx->fft_buffer_real || !ctx->fft_win || !ctx->integrated_chirp)
    {
        /* Free any allocated memory to prevent leaks */
        free(ctx->zero_padded_samples);
        free(ctx->fft_buffer);
        free(ctx->fft_buffer_real);
        free(ctx->fft_win);
        free(ctx->integrated_chirp);
        return CY_RSLT_TYPE_ERROR;
    }

    /* Initialize Arm FFT instance */
    result = arm_rfft_fast_init_f32(&ctx->rfft, ctx->num_samples_zeroPadded);
    if (result != ARM_MATH_SUCCESS)
    {
        free(ctx->zero_padded_samples);
        free(ctx->fft_buffer);
        free(ctx->fft_buffer_real);
        free(ctx->fft_win);
        free(ctx->integrated_chirp);
        return CY_RSLT_TYPE_ERROR;
    }

    /* Hamming Window function */
    ifx_window_hamming_f32(ctx->fft_win, ctx->num_samples_zeroPadded);

    printf("Range Accuracy=%.2f CMS | Minimum Range: %.1f CMS | Maximum Range: %.1f CMS \r\n\n", (double)(ctx->bin_len * 100), (double)(ctx->skip * ctx->bin_len * 100), (double)(ctx->max_range_bin *ctx->bin_len* 100));

    return CY_RSLT_SUCCESS;
}

/*******************************************************************************
* Function Name: get_static_distance
****************************************************************************//**
* Processes radar data frame to calculate the static distance.
*
* \param ctx
* Pointer to the static distance context structure.
*
* \param frame_data
* Pointer to the radar frame data (samples).
*
* \return float32_t
* Returns the calculated distance in meters. Returns -1.0f if processing fails.
*******************************************************************************/
float32_t get_static_distance(static_distance_context_t *ctx, const uint16_t *frame_data)
{
    /* Reset the zero pad buffer */
    for (int chirp_idx = 0; chirp_idx < XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME; chirp_idx++)
    {
        for (int sample_idx = 0; sample_idx < ctx->num_samples_zeroPadded; sample_idx++)
        {
            ctx->zero_padded_samples[chirp_idx * ctx->num_samples_zeroPadded + sample_idx] = 0.0f;
        }
    }

    /* Convert 1D frame to 2D frame matrix and normalize */
    for (int chirp_idx = 0; chirp_idx < XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME; chirp_idx++)
    {
        for (int sample_idx = 0; sample_idx < XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP; sample_idx++)
        {
            ctx->frame[chirp_idx][sample_idx] = (float32_t)(frame_data[(sample_idx + (chirp_idx * XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP))]) / 4096.0f;
        }
    }

    /* De-Bias Signal */
    for (int chirp_idx = 0; chirp_idx < XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME; chirp_idx++)
    {
        float32_t avg_sum = 0, avg = 0;
        for (int sample_idx = 0; sample_idx < XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP; sample_idx++)
        {
            avg_sum += ctx->frame[chirp_idx][sample_idx];
        }
        avg = avg_sum / XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP;
        for (int sample_idx = 0; sample_idx < XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP; sample_idx++)
        {
            ctx->frame[chirp_idx][sample_idx] -= avg;
        }
    }

    /* Copy frame contents into zero padding buffer */
    for (int chirp_idx = 0; chirp_idx < XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME; chirp_idx++)
    {
        for (int sample_idx = 0; sample_idx < XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP; sample_idx++)
        {
            ctx->zero_padded_samples[chirp_idx * ctx->num_samples_zeroPadded + sample_idx] = ctx->frame[chirp_idx][sample_idx];
        }
    }

    /* Perform Range FFT */
    if (ifx_range_fft_f32(ctx->zero_padded_samples, ctx->fft_buffer, true, ctx->fft_win,
                          ctx->num_samples_zeroPadded, XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME) != IFX_SENSOR_DSP_STATUS_OK)
    {
        printf("Range FFT Failed!\r\n");
        return -1.0f;
    }

    /* Complex to Real */
    for (int chirp_idx = 0; chirp_idx < XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME; chirp_idx++)
    {
        for (int sample_idx = 0; sample_idx < ctx->fft_size; sample_idx++)
        {
            ctx->fft_buffer_real[chirp_idx * ctx->fft_size + sample_idx] = cabsf(ctx->fft_buffer[chirp_idx * ctx->fft_size + sample_idx]);
        }
    }

    /* Coherent integration of chirps */
    for (int sample_idx = 0; sample_idx < ctx->fft_size; sample_idx++)
    {
        ctx->integrated_chirp[sample_idx] = 0.0f;
    }
    for (int sample_idx = 0; sample_idx < ctx->fft_size; sample_idx++)
    {
        for (int chirp_idx = 0; chirp_idx < XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME; chirp_idx++)
        {
            ctx->integrated_chirp[sample_idx] += ctx->fft_buffer_real[chirp_idx * ctx->fft_size + sample_idx];
        }
        ctx->integrated_chirp[sample_idx] /= XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME;
    }

    /* Peak Search */
    float32_t max_magnitude = -1;
    uint16_t max_idx = -1;
    for (int32_t sample_idx = ctx->skip; sample_idx <= ctx->max_range_bin; ++sample_idx)
    {
        if (ctx->integrated_chirp[sample_idx] > max_magnitude)
        {
            max_magnitude = ctx->integrated_chirp[sample_idx];
            max_idx = sample_idx;
        }
    }

    /* Calculate Distance */
    float32_t distance_m = max_idx * ctx->bin_len;

    return distance_m;
}




