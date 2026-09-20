#include "ppg.h"

/* Indirizzo I2C a 7 bit del MAX30102. */
#define MAX30102_ADDRESS          0x57U

/* Registri della FIFO. */
#define MAX30102_REG_FIFO_WR_PTR  0x02U
#define MAX30102_REG_FIFO_OVF     0x03U
#define MAX30102_REG_FIFO_RD_PTR  0x04U
#define MAX30102_REG_FIFO_DATA    0x07U

/* Registri di configurazione. */
#define MAX30102_REG_MODE_CONFIG  0x09U
#define MAX30102_REG_SPO2_CONFIG  0x0AU
#define MAX30102_REG_LED1_PA      0x0CU
#define MAX30102_REG_LED2_PA      0x0DU

/* Pin utilizzati da I2C1. */
#define PPG_I2C_PORT              GPIOB
#define PPG_I2C_SCL_PIN           8U
#define PPG_I2C_SDA_PIN           9U

/*
 * Configurazione I2C1 originale.
 * Il valore è calcolato per un bus a 100 kHz con PCLK1 a 85 MHz.
 */
static const I2CConfig ppg_i2c_config = {
  .timingr = 0x20303E5DU
};

/**
 * Configura PB8 e PB9 in modalità alternativa I2C1.
 */
static void ppg_configure_i2c_pins(void) {
  palSetPadMode(
      PPG_I2C_PORT,
      PPG_I2C_SCL_PIN,
      PAL_MODE_ALTERNATE(4) |
      PAL_STM32_OTYPE_OPENDRAIN |
      PAL_STM32_OSPEED_HIGHEST |
      PAL_STM32_PUPDR_PULLUP);

  palSetPadMode(
      PPG_I2C_PORT,
      PPG_I2C_SDA_PIN,
      PAL_MODE_ALTERNATE(4) |
      PAL_STM32_OTYPE_OPENDRAIN |
      PAL_STM32_OSPEED_HIGHEST |
      PAL_STM32_PUPDR_PULLUP);
}

/**
 * Prova a liberare il bus I2C generando nove impulsi di clock
 * e una condizione di STOP.
 *
 * La procedura deriva direttamente dal progetto già testato.
 */
static void ppg_i2c_bus_recovery(void) {
  /*
   * Porta temporaneamente SCL in modalità GPIO open-drain
   * e genera nove impulsi di clock.
   */
  palSetPadMode(
      PPG_I2C_PORT,
      PPG_I2C_SCL_PIN,
      PAL_MODE_OUTPUT_OPENDRAIN |
      PAL_STM32_OSPEED_HIGHEST |
      PAL_STM32_PUPDR_PULLUP);

  for (unsigned int pulse = 0U; pulse < 9U; ++pulse) {
    palClearPad(PPG_I2C_PORT, PPG_I2C_SCL_PIN);
    chThdSleepMilliseconds(1);

    palSetPad(PPG_I2C_PORT, PPG_I2C_SCL_PIN);
    chThdSleepMilliseconds(1);
  }

  /*
   * Genera una condizione di STOP portando SDA da basso ad alto
   * mentre SCL si trova alto.
   */
  palSetPadMode(
      PPG_I2C_PORT,
      PPG_I2C_SDA_PIN,
      PAL_MODE_OUTPUT_OPENDRAIN |
      PAL_STM32_OSPEED_HIGHEST |
      PAL_STM32_PUPDR_PULLUP);

  palClearPad(PPG_I2C_PORT, PPG_I2C_SDA_PIN);
  chThdSleepMilliseconds(1);

  palSetPad(PPG_I2C_PORT, PPG_I2C_SDA_PIN);
  chThdSleepMilliseconds(1);

  /* Ripristina PB8 e PB9 come linee I2C1. */
  ppg_configure_i2c_pins();
}

/**
 * Riavvia la periferica I2C1 dopo un errore.
 */
static void ppg_i2c_reset_peripheral(void) {
  i2cStop(&I2CD1);
  chThdSleepMilliseconds(10);

  i2cStart(&I2CD1, &ppg_i2c_config);
  chThdSleepMilliseconds(10);
}

/**
 * Esegue la procedura di recupero originale del bus I2C.
 */
static void ppg_i2c_recover(void) {
  ppg_i2c_bus_recovery();
  ppg_i2c_reset_peripheral();
}

/**
 * Scrive un byte in un registro del MAX30102.
 */
static bool max30102_write_register(uint8_t reg, uint8_t value) {
  const uint8_t transmit_buffer[2] = {
    reg,
    value
  };

  i2cAcquireBus(&I2CD1);

  const msg_t status = i2cMasterTransmitTimeout(
      &I2CD1,
      MAX30102_ADDRESS,
      transmit_buffer,
      sizeof(transmit_buffer),
      NULL,
      0U,
      TIME_MS2I(100));

  i2cReleaseBus(&I2CD1);

  if (status != MSG_OK) {
    ppg_i2c_recover();
    return false;
  }

  return true;
}

/**
 * Azzera i puntatori e il contatore di overflow della FIFO.
 */
static bool max30102_reset_fifo(void) {
  bool success = true;

  if (!max30102_write_register(
          MAX30102_REG_FIFO_WR_PTR,
          0x00U)) {
    success = false;
  }

  if (!max30102_write_register(
          MAX30102_REG_FIFO_OVF,
          0x00U)) {
    success = false;
  }

  if (!max30102_write_register(
          MAX30102_REG_FIFO_RD_PTR,
          0x00U)) {
    success = false;
  }

  return success;
}

/**
 * Legge i sei byte di un campione dalla FIFO.
 */
static bool max30102_read_fifo(uint8_t receive_buffer[6]) {
  const uint8_t fifo_register = MAX30102_REG_FIFO_DATA;

  i2cAcquireBus(&I2CD1);

  const msg_t status = i2cMasterTransmitTimeout(
      &I2CD1,
      MAX30102_ADDRESS,
      &fifo_register,
      1U,
      receive_buffer,
      6U,
      TIME_MS2I(50));

  i2cReleaseBus(&I2CD1);

  return status == MSG_OK;
}

bool ppg_init(void) {
  bool success = true;

  /*
   * Configura PB8/PB9 e avvia I2C1.
   */
  ppg_configure_i2c_pins();
  i2cStart(&I2CD1, &ppg_i2c_config);

  /* Attende che bus e sensore siano pronti. */
  chThdSleepMilliseconds(100);

  /*
   * Mantiene gli stessi valori di configurazione
   * utilizzati nel progetto originale.
   */

  /* Modalità SpO2: LED rosso e infrarosso. */
  if (!max30102_write_register(
          MAX30102_REG_MODE_CONFIG,
          0x03U)) {
    success = false;
  }

  chThdSleepMilliseconds(10);

  /* Configurazione ADC/sample rate/pulse width originale. */
  if (!max30102_write_register(
          MAX30102_REG_SPO2_CONFIG,
          0x27U)) {
    success = false;
  }

  chThdSleepMilliseconds(10);

  /* Intensità del LED rosso. */
  if (!max30102_write_register(
          MAX30102_REG_LED1_PA,
          0x24U)) {
    success = false;
  }

  /* Intensità del LED infrarosso. */
  if (!max30102_write_register(
          MAX30102_REG_LED2_PA,
          0x24U)) {
    success = false;
  }

  chThdSleepMilliseconds(10);

  if (!max30102_reset_fifo()) {
    success = false;
  }

  return success;
}

bool ppg_read_sample(ppg_sample_t *sample) {
  if (sample == NULL) {
    return false;
  }

  uint8_t receive_buffer[6] = {0U};

  /*
   * Prima lettura. Se fallisce, ripristina il bus e riprova
   * una sola volta, come nel codice originale.
   */
  if (!max30102_read_fifo(receive_buffer)) {
    ppg_i2c_recover();

    if (!max30102_read_fifo(receive_buffer)) {
      sample->red = 0U;
      sample->infrared = 0U;
      return false;
    }
  }

  /*
   * Ogni misura è contenuta in 18 bit.
   * I bit superiori del primo byte vengono eliminati.
   */
  sample->red =
      ((((uint32_t)receive_buffer[0]) << 16) |
       (((uint32_t)receive_buffer[1]) << 8) |
       ((uint32_t)receive_buffer[2])) &
      0x03FFFFU;

  sample->infrared =
      ((((uint32_t)receive_buffer[3]) << 16) |
       (((uint32_t)receive_buffer[4]) << 8) |
       ((uint32_t)receive_buffer[5])) &
      0x03FFFFU;

  return true;
}
