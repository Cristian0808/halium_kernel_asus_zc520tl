#ifndef TEE_FP_H
#define TEE_FP_H

#define SPI_MAX_BUS_NUM 16
#define TEE_FP_SPI_BUS_NUM	0

void tee_fp_enable_spi_clk(void);

void tee_fp_disable_spi_clk(void);

int tee_fp_init(void);

void tee_fp_exit(void);

#endif
