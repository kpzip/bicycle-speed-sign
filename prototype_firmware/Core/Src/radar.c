#include "main.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_def.h"
#include "xensiv_bgt60trxx.h"
#include "radar.h"

#include <stdint.h>
#include <stdbool.h>

xensiv_bgt60trxx_t radar;

void radar_setup() {
    int32_t status = xensiv_bgt60trxx_init(&radar, &hspi1, false);
    if (status != XENSIV_BGT60TRXX_STATUS_OK) {
        Error_Handler();
    }

    status = xensiv_bgt60trxx_set_fifo_limit(&radar, NUM_SAMPLES_PER_FRAME);

    

    status = xensiv_bgt60trxx_config(&radar, register_list, sizeof(register_list)/sizeof(uint32_t));
    if (status != XENSIV_BGT60TRXX_STATUS_OK) {
        Error_Handler();
    }

}

void radar_loop() {

}

void radar_irq() {

}

void switch_spi_word_size(int bits) {
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);

    __HAL_SPI_DISABLE(&hspi1);

    hspi1.Init.DataSize = bits;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/* Platform-specific function that sets the output value of the RST pin. */
void xensiv_bgt60trxx_platform_rst_set(const void* iface, bool val) {
    HAL_GPIO_WritePin(FMCW_RST_GPIO_Port, FMCW_RST_Pin, val ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
 
/* Platform-specific function that that sets the output value of the SPI CS pin. */
void xensiv_bgt60trxx_platform_spi_cs_set(const void* iface, bool val) {
    HAL_GPIO_WritePin(FMCW_CS_GPIO_Port, FMCW_CS_Pin, val ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
 
/* Platform-specific function that performs a SPI write/read transfer to
 * the register file of the sensor. */
int32_t xensiv_bgt60trxx_platform_spi_transfer(void* iface, uint8_t* tx_data, uint8_t* rx_data, uint32_t len) {
    switch_spi_word_size(8);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, len, HAL_MAX_DELAY);
    if (status == HAL_OK) {
        return XENSIV_BGT60TRXX_STATUS_OK;
    }
    else {
        return XENSIV_BGT60TRXX_STATUS_COM_ERROR;
    }
}
 
/* Platform-specific function that performs a SPI burst read transfer to
 * receive a block of data from sensor FIFO. */
int32_t xensiv_bgt60trxx_platform_spi_fifo_read(void* iface, uint16_t* rx_data, uint32_t len) {
    switch_spi_word_size(12);
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi1, (uint8_t*)rx_data, len, HAL_MAX_DELAY);
    if (status == HAL_OK) {
        return XENSIV_BGT60TRXX_STATUS_OK;
    }
    else {
        return XENSIV_BGT60TRXX_STATUS_COM_ERROR;
    }
}
 
/* Platform-specific function that waits for a specified time period in milliseconds. */
void xensiv_bgt60trxx_platform_delay(uint32_t ms) {
    HAL_Delay(ms);
}
 
/* Platform-specific function to reverse the byte order (32 bits). */
uint32_t xensiv_bgt60trxx_platform_word_reverse(uint32_t x) {
    return __REV(x);
}
 
/* Platform-specific function that implements a runtime assertion. */
void xensiv_bgt60trxx_platform_assert(int expr) {
    if (!expr) {
        Error_Handler();
    }
}