/**
 * Copyright 2022 Evgeniy Morozov
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
*/

#include "estc_service.h"

#include "app_error.h"
#include "nrf_log.h"

#include "ble.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service);

ret_code_t estc_ble_service_init(ble_estc_service_t *service)
{
  ret_code_t error_code = NRF_SUCCESS;

  ble_uuid_t    service_uuid = { .uuid    = ESTC_SERVICE_UUID };
  ble_uuid128_t base_uuid    = { .uuid128 = ESTC_BASE_UUID };

  error_code = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
  APP_ERROR_CHECK(error_code);

  service->uuid_type = service_uuid.type;

  error_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &service_uuid,
                                        &service->service_handle);

  APP_ERROR_CHECK(error_code);

  return estc_ble_add_characteristics(service);
}

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service)
{
  static uint8_t char_user_desc[] = ESTC_GATT_CHAR_1_USER_DESC;
  
  ret_code_t error_code = NRF_SUCCESS;

  ble_uuid_t char_uuid =
  {
    .uuid = ESTC_GATT_CHAR_1_UUID,
    .type = service->uuid_type,
  };

  ble_gatts_char_md_t char_md =
  {
    .char_props              = { .read = 1, .write = 1 },
    .p_char_user_desc        = char_user_desc,
    .char_user_desc_size     = sizeof(char_user_desc),
    .char_user_desc_max_size = sizeof(char_user_desc),

    /*
     * Even though the User Descriptor is not writable,
     * nrfConnect always shows like it is:
     * https://devzone.nordicsemi.com/f/nordic-q-a/47822/characteristic-user-description-is-always-shown-as-writable
     */
    .char_ext_props.wr_aux   = 0,
  };

  ble_gatts_attr_md_t attr_md =
  {
    .vloc = BLE_GATTS_VLOC_STACK,
  };

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

  ble_gatts_attr_t attr_char_value =
  {
    .p_uuid    = &char_uuid,
    .p_attr_md = &attr_md,
  };

  /*
   * In order to check the read/write functionality,
   * there must be some data to manipulate with,
   * so a raw 2-byte array is used.
   */
  attr_char_value.init_offs = 0;
  attr_char_value.init_len  = 2;
  attr_char_value.max_len   = 2;

  error_code =
    sd_ble_gatts_characteristic_add(service->service_handle, &char_md,
                                    &attr_char_value, &service->char_handles);

  APP_ERROR_CHECK(error_code);

  return NRF_SUCCESS;
}
