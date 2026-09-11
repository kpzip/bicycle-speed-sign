/* ***************************************************************************
** File name: radar_processing.h
**
** Description: This is the data processing header file for PSOC6 Radar Static Distance Code Example.
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
#ifndef RADAR_PROCESSING_H
#define RADAR_PROCESSING_H
#include <stdint.h> // Provides fixed-width integer types like int8_t, uint32_t, etc.
// #include "ifx_sensor_dsp.h"
// #include "cyhal.h"
// #include "cybsp.h"
// #include <resource_map.h>
#include "radar.h"
#include "arm_math.h"

/*******************************************************************************
* Types
*******************************************************************************/
/* Static distance context structure */
typedef struct
{
    /* Parameters */
    int32_t num_samples_zeroPadded;
    int32_t fft_size;
    float32_t bin_len;
    uint16_t skip;
    uint16_t max_range_bin;

    /* Buffer Arrays */
    float32_t frame[XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME][XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP];
    float32_t *zero_padded_samples; /* [XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME][num_samples_zeroPadded] */
    cfloat32_t *fft_buffer;         /* [XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME][fft_size] */
    float32_t *fft_buffer_real;     /* [XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME][fft_size] */
    float32_t *fft_win;             /* [num_samples_zeroPadded] */
    float32_t *integrated_chirp;    /* [fft_size] */

    /* Arm FFT Instance */
    arm_rfft_fast_instance_f32 rfft;
    
} static_distance_context_t;

/*******************************************************************************
* External Variables
*******************************************************************************/
extern static_distance_context_t context;
extern int16_t zeroPadding_factor;
extern float32_t max_range_m;
extern float32_t min_range_m;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
cy_rslt_t init_static_distance(static_distance_context_t *ctx);
float32_t get_static_distance(static_distance_context_t *ctx, const uint16_t *frame_data);

#endif /* RADAR_PROCESSING_H */
