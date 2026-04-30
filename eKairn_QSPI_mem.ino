//
// XIAO Acces to the on board QSPI Memrory
//
//  Author: fbd38
//  Release: 1.0.0
//  Date: 17/11/2023
//
//  Basic routines using a "MagicNumber" to check the block validity.
//  each configuration uses a single 4KByte block memory (not optimal for size)
//
//  !! warning !!:  size must of write be a multiple of 4
//
// QSPI Pin assignation as per the XIAO Seeed nRF52840 (sense) Module
static nrfx_qspi_config_t QSPI_mem_config = NRFX_QSPI_DEFAULT_CONFIG(21, 25, 20, 24, 22, 23);
static const uint32_t magicValue = 0xCAFE3501;

bool QSPI_mem_connect() {
  return (nrfx_qspi_init(&QSPI_mem_config, NULL, NULL) == NRFX_SUCCESS);
}

void QSPI_mem_disconnect() {
  nrfx_qspi_uninit();
}

// bool QSPI_mem_ready() {
//   return (nrfx_qspi_mem_busy_check() == NRFX_SUCCESS);
// }

// bool QSPI_mem_wait_ready() {
//   while (!QSPI_mem_ready()) {
//     /* Wait */
//   }
//   return (true);
// }

bool QSPI_mem_erase_all() {
  return (nrfx_qspi_chip_erase() == NRFX_SUCCESS);
}

bool QSPI_mem_erase_4KB(uint32_t QSPI_addr) {
  return (nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, QSPI_addr) == NRFX_SUCCESS);
}

// bool QSPI_mem_erase_64KB(uint32_t QSPI_addr) {
//   return (nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_64KB, QSPI_addr) == NRFX_SUCCESS);
// }

bool QSPI_mem_read(uint32_t QSPI_addr, void *p_buffer, size_t byte_size) {
  return (nrfx_qspi_read(p_buffer, byte_size, QSPI_addr) == NRFX_SUCCESS);
}

bool QSPI_mem_write(uint32_t QSPI_addr, void const *p_buffer, size_t byte_size) {
  return (nrfx_qspi_write(p_buffer, byte_size, QSPI_addr) == NRFX_SUCCESS);
}

bool QSPI_mem_writeConfig(int16_t numBlock4K, void *current, size_t bsize) {
  uint32_t magicNumber = magicValue;
  /* Compute Addresses */
  uint32_t qaddr = numBlock4K * 4096;
  uint32_t daddr = qaddr + sizeof(magicNumber);
  /* first erase the 4K block */
  if (QSPI_mem_erase_4KB(qaddr)) {
    /* then write the data */
    if (QSPI_mem_write(daddr, current, bsize)) {
      /* then the magic number */
      return (QSPI_mem_write(qaddr, &magicNumber, sizeof(magicNumber)));
    }
  }
  /* in all other cases */
  Serial.println("[ERR] QSP Memory write config");
  return (false);
}

// Basic routine to read the configuration
bool QSPI_mem_readConfig(int16_t numBlock4K, void *current, void *factory, size_t bsize) {
  memcpy(current, factory, bsize);  // in case of current an memory get corrupted
  if (bsize > 4000) return (false);
  uint32_t magicNumber;
  uint32_t qaddr = numBlock4K * 4096;
  uint32_t daddr = qaddr + sizeof(magicNumber);
  /* Read magic number */
  if (QSPI_mem_read(qaddr, &magicNumber, sizeof(magicNumber))) {
    /* Test value of magicNumber */
    if (magicNumber == magicValue) {
      /* Already correct, so reload the value */
      return (QSPI_mem_read(daddr, current, bsize));
    } else {
      /* need to write the factory values */
      return (QSPI_mem_writeConfig(numBlock4K, factory, bsize));
    }
  }
  /* in all other cases */
  Serial.println("[ERR] QSPI Memory read config");
  return (false);
}

void QSPI_mem_dump(int16_t numBlock4K, int16_t line) {
  static char qval[4096];
  char str[128], st[16];
  uint32_t qaddr = numBlock4K * 4096;
  //  Serial.println("QSPI_mem_dump");
  QSPI_mem_connect();
  QSPI_mem_read(qaddr, &qval, sizeof(qval));
  QSPI_mem_disconnect();
  // print values
  for (int16_t ii = 0; ii < line; ii++) {
    // Display in HEX
    sprintf(str, "[%06X] ", qaddr + ii * 8);
    for (int16_t iii = 0; iii < 8; iii++) {
      sprintf(st, "%02X ", qval[ii * 8 + iii]);
      strcat(str, st);
    }
    // Display as char
    strcat(str, "  [");
    for (int16_t iii = 0; iii < 8; iii++) {
      char q = qval[ii * 8 + iii];
      int16_t iq = (int16_t)q;
      q = (iq >= 32) && (iq <= 126)
            ? q
            : '.';
      sprintf(st, "%c", q);
      strcat(str, st);
    }
    strcat(str, "]");
    // Display as int16_t
    strcat(str, "  (");
    for (int16_t iii = 0; iii < 8; iii++) {
      int16_t iv;
      if ((iii % 2) == 1) {
        iv = (((int16_t)qval[ii * 8 + iii - 1]) & 0x00FF) | (((int16_t)qval[ii * 8 + iii]) << 8);
        sprintf(st, "%6d, ", iv);
        strcat(str, st);
      }
    }
    strcat(str, ")");
    Serial.println(str);
  }
}