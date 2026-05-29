#include "drvs/max_m10s.h"

#include "pico/stdlib.h"

#define MAX_M10S_REG_BYTES_AVAILABLE_MSB 0xFD
#define MAX_M10S_REG_DATA_STREAM 0xFF

#define UBX_SYNC_1 0xB5
#define UBX_SYNC_2 0x62

#define UBX_CLASS_NAV 0x01
#define UBX_ID_NAV_PVT 0x07
#define UBX_NAV_PVT_LEN 92

typedef enum {
  UBX_WAIT_SYNC_1 = 0,
  UBX_WAIT_SYNC_2,
  UBX_READ_CLASS,
  UBX_READ_ID,
  UBX_READ_LEN_1,
  UBX_READ_LEN_2,
  UBX_READ_PAYLOAD,
  UBX_READ_CK_A,
  UBX_READ_CK_B,
} ubx_parser_state_t;

static ubx_parser_state_t parser_state = UBX_WAIT_SYNC_1;

static uint8_t ubx_class;
static uint8_t ubx_id;
static uint16_t ubx_len;
static uint16_t ubx_payload_index;
static uint8_t ubx_payload[UBX_NAV_PVT_LEN];

static uint8_t ck_a;
static uint8_t ck_b;

static void ubx_ck_add(uint8_t b) {
  ck_a += b;
  ck_b += ck_a;
}

static uint32_t rd_u32_le(const uint8_t *p) {
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static int32_t rd_i32_le(const uint8_t *p) { return (int32_t)rd_u32_le(p); }

static void parser_reset(void) {
  parser_state = UBX_WAIT_SYNC_1;
  ubx_class = 0;
  ubx_id = 0;
  ubx_len = 0;
  ubx_payload_index = 0;
  ck_a = 0;
  ck_b = 0;
}

static void parse_nav_pvt_payload(const uint8_t *p, gnss_sample_t *sample) {
  sample->iTOW_ms = rd_u32_le(&p[0]);

  sample->fixType = p[20];
  sample->flags = p[21];
  sample->numSV = p[23];

  sample->lon_deg_e7 = rd_i32_le(&p[24]);
  sample->lat_deg_e7 = rd_i32_le(&p[28]);

  sample->height_mm = rd_i32_le(&p[32]);
  sample->hMSL_mm = rd_i32_le(&p[36]);

  sample->hAcc_mm = rd_u32_le(&p[40]);
  sample->vAcc_mm = rd_u32_le(&p[44]);

  sample->velN_mm_s = rd_i32_le(&p[48]);
  sample->velE_mm_s = rd_i32_le(&p[52]);
  sample->velD_mm_s = rd_i32_le(&p[56]);
  sample->gSpeed_mm_s = rd_i32_le(&p[60]);

  sample->sAcc_mm_s = rd_u32_le(&p[68]);
}

static bool ubx_parse_byte(uint8_t b, gnss_sample_t *sample) {
  switch (parser_state) {
  case UBX_WAIT_SYNC_1:
    if (b == UBX_SYNC_1) {
      parser_state = UBX_WAIT_SYNC_2;
    }
    break;

  case UBX_WAIT_SYNC_2:
    if (b == UBX_SYNC_2) {
      ck_a = 0;
      ck_b = 0;
      parser_state = UBX_READ_CLASS;
    } else {
      parser_state = UBX_WAIT_SYNC_1;
    }
    break;

  case UBX_READ_CLASS:
    ubx_class = b;
    ubx_ck_add(b);
    parser_state = UBX_READ_ID;
    break;

  case UBX_READ_ID:
    ubx_id = b;
    ubx_ck_add(b);
    parser_state = UBX_READ_LEN_1;
    break;

  case UBX_READ_LEN_1:
    ubx_len = b;
    ubx_ck_add(b);
    parser_state = UBX_READ_LEN_2;
    break;

  case UBX_READ_LEN_2:
    ubx_len |= ((uint16_t)b << 8);
    ubx_ck_add(b);
    ubx_payload_index = 0;

    if (ubx_len > UBX_NAV_PVT_LEN) {
      parser_reset();
    } else if (ubx_len == 0) {
      parser_state = UBX_READ_CK_A;
    } else {
      parser_state = UBX_READ_PAYLOAD;
    }
    break;

  case UBX_READ_PAYLOAD:
    ubx_payload[ubx_payload_index++] = b;
    ubx_ck_add(b);

    if (ubx_payload_index >= ubx_len) {
      parser_state = UBX_READ_CK_A;
    }
    break;

  case UBX_READ_CK_A:
    if (b == ck_a) {
      parser_state = UBX_READ_CK_B;
    } else {
      parser_reset();
    }
    break;

  case UBX_READ_CK_B:
    if (b == ck_b && ubx_class == UBX_CLASS_NAV && ubx_id == UBX_ID_NAV_PVT &&
        ubx_len == UBX_NAV_PVT_LEN) {

      parse_nav_pvt_payload(ubx_payload, sample);
      parser_reset();
      return true;
    }

    parser_reset();
    break;

  default:
    parser_reset();
    break;
  }

  return false;
}

static bool max_m10s_get_available(uint16_t *available) {
  uint8_t reg = MAX_M10S_REG_BYTES_AVAILABLE_MSB;
  uint8_t buf[2];

  int ret = i2c_write_timeout_us(MAX_M10S_I2C_PORT, MAX_M10S_I2C_ADDR, &reg, 1,
                                 true, MAX_M10S_I2C_TIMEOUT_US);

  if (ret != 1) {
    return false;
  }

  ret = i2c_read_timeout_us(MAX_M10S_I2C_PORT, MAX_M10S_I2C_ADDR, buf, 2, false,
                            MAX_M10S_I2C_TIMEOUT_US);

  if (ret != 2) {
    return false;
  }

  *available = ((uint16_t)buf[0] << 8) | buf[1];
  return true;
}

static int max_m10s_read_stream(uint8_t *buf, uint16_t len) {
  uint8_t reg = MAX_M10S_REG_DATA_STREAM;

  int ret = i2c_write_timeout_us(MAX_M10S_I2C_PORT, MAX_M10S_I2C_ADDR, &reg, 1,
                                 true, MAX_M10S_I2C_TIMEOUT_US);

  if (ret != 1) {
    return ret;
  }

  return i2c_read_timeout_us(MAX_M10S_I2C_PORT, MAX_M10S_I2C_ADDR, buf, len,
                             false, MAX_M10S_I2C_TIMEOUT_US);
}

bool max_m10s_init(void) {
  i2c_init(MAX_M10S_I2C_PORT, MAX_M10S_I2C_BAUDRATE_HZ);

  gpio_set_function(MAX_M10S_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(MAX_M10S_I2C_SCL_PIN, GPIO_FUNC_I2C);

  gpio_pull_up(MAX_M10S_I2C_SDA_PIN);
  gpio_pull_up(MAX_M10S_I2C_SCL_PIN);

  parser_reset();

  uint16_t available = 0;
  return max_m10s_get_available(&available);
}

max_m10s_poll_status_t max_m10s_poll_sample(gnss_sample_t *sample) {
  if (sample == NULL) {
    return MAX_M10S_POLL_ERROR;
  }

  uint16_t available = 0;

  if (!max_m10s_get_available(&available)) {
    return MAX_M10S_POLL_ERROR;
  }

  if (available == 0) {
    return MAX_M10S_POLL_NO_DATA;
  }

  if (available > MAX_M10S_READ_CHUNK_SIZE) {
    available = MAX_M10S_READ_CHUNK_SIZE;
  }

  uint8_t buf[MAX_M10S_READ_CHUNK_SIZE];

  int n = max_m10s_read_stream(buf, available);
  if (n != available) {
    return MAX_M10S_POLL_ERROR;
  }

  for (int i = 0; i < n; i++) {
    if (buf[i] == 0xFF) {
      continue;
    }

    if (ubx_parse_byte(buf[i], sample)) {
      return MAX_M10S_POLL_OK;
    }
  }

  return MAX_M10S_POLL_NO_DATA;
}
