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
#include "ble_gatt.h"
#include "nrf_log.h"

#include "ble.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"

#include "uptime-observer.h"

typedef struct estc_ble_char_opts_s
{
  ble_gatts_char_handles_t * p_char_handles;
  ble_uuid_t               * p_uuid;
  uint8_t                  * p_user_desc;
  size_t                     user_desc_len;
  ble_gatts_attr_md_t      * p_cccd;
  ble_gatts_attr_t         * p_attr;
  ble_gatt_char_props_t      char_props;
} estc_ble_char_opts_t;

static ret_code_t estc_ble_chars_add(ble_estc_service_t *service);
static ret_code_t estc_ble_char_add(ble_estc_service_t *service,
                                    const estc_ble_char_opts_t *opts);

void estc_service_ble_evt_handler(const ble_evt_t * p_ble_evt, void * context)
{
  ble_estc_service_t * estc_service = (ble_estc_service_t *) context;

  switch (p_ble_evt->header.evt_id)
  {
    case BLE_GAP_EVT_CONNECTED:
      estc_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      estc_service->conn_handle = BLE_CONN_HANDLE_INVALID;
      break;
  }
}

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

  return estc_ble_chars_add(service);
}

static void indy_char_update(void * context, const uptime_t * seconds)
{
  ble_estc_service_t * estc_service = context;

  if (estc_service->conn_handle != BLE_CONN_HANDLE_INVALID)
  {
    if (*seconds % 2 != 0) return; // ONLY ODD NUMBERS

    uint16_t len = sizeof(uptime_t);

    ble_gatts_hvx_params_t hvx_params;
    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = estc_service->indy_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_INDICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &len;
    hvx_params.p_data = (uint8_t*) seconds;  

    sd_ble_gatts_hvx(estc_service->conn_handle, &hvx_params);
  }
}

static ret_code_t estc_ble_indy_char_add(ble_estc_service_t *service)
{
  static uint8_t user_desc[] = ESTC_GATT_CHAR_INDY_USER_DESC; 

  uptime_observer_subscribe(indy_char_update, service);

  ble_uuid_t char_uuid =
  {
    .uuid = ESTC_GATT_CHAR_RDWR_UUID,
    .type = service->uuid_type,
  };

  ble_gatts_attr_md_t attr_md =
  {
    .vloc = BLE_GATTS_VLOC_STACK,
  };

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);

  ble_gatts_attr_md_t cccd_md =
  {
    .vloc = BLE_GATTS_VLOC_STACK,
  };

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

  ble_gatts_attr_t attr_char_value =
  {
    .p_uuid    = &char_uuid,
    .p_attr_md = &attr_md,
    .max_len   = sizeof(uptime_t),
    .init_len  = sizeof(uptime_t),
  };

  return estc_ble_char_add(service, &(estc_ble_char_opts_t)
  {
    .p_uuid         = &char_uuid,
    .p_user_desc    = user_desc,
    .user_desc_len  = sizeof(user_desc),
    .char_props     = { .indicate = 1 },
    .p_attr         = &attr_char_value,
    .p_cccd         = &cccd_md,
    .p_char_handles = &service->indy_handles,
  });
}

static void ntfy_char_update(void * context, const uptime_t * seconds)
{
  ble_estc_service_t * estc_service = context;

  if (estc_service->conn_handle != BLE_CONN_HANDLE_INVALID)
  {
    if (*seconds % 2 == 0) return; // ONLY EVEN NUMBERS
    
    uint16_t len = sizeof(uptime_t);

    ble_gatts_hvx_params_t hvx_params;
    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = estc_service->ntfy_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &len;
    hvx_params.p_data = (uint8_t*) seconds;  

    sd_ble_gatts_hvx(estc_service->conn_handle, &hvx_params);
  }
}

static ret_code_t estc_ble_ntfy_char_add(ble_estc_service_t *service)
{
  static uint8_t user_desc[] = ESTC_GATT_CHAR_NTFY_USER_DESC; 

  uptime_observer_subscribe(ntfy_char_update, service);

  ble_uuid_t char_uuid =
  {
    .uuid = ESTC_GATT_CHAR_RDWR_UUID,
    .type = service->uuid_type,
  };

  ble_gatts_attr_md_t attr_md =
  {
    .vloc = BLE_GATTS_VLOC_STACK,
  };

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);

  ble_gatts_attr_md_t cccd_md =
  {
    .vloc = BLE_GATTS_VLOC_STACK,
  };

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

  ble_gatts_attr_t attr_char_value =
  {
    .p_uuid    = &char_uuid,
    .p_attr_md = &attr_md,
    .max_len   = sizeof(uptime_t),
    .init_len  = sizeof(uptime_t),
  };

  return estc_ble_char_add(service, &(estc_ble_char_opts_t)
  {
    .char_props     = { .notify = 1 },
    .p_uuid         = &char_uuid,
    .p_user_desc    = user_desc,
    .user_desc_len  = sizeof(user_desc),
    .p_attr         = &attr_char_value,
    .p_cccd         = &cccd_md,
    .p_char_handles = &service->ntfy_handles,
  });
}

static ret_code_t estc_ble_rdwr_char_add(ble_estc_service_t *service)
{
  static uint8_t user_desc[] = ESTC_GATT_CHAR_RDWR_USER_DESC; 

  ble_uuid_t char_uuid =
  {
    .uuid = ESTC_GATT_CHAR_RDWR_UUID,
    .type = service->uuid_type,
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

    /*
     * In order to check the read/write functionality,
     * there must be some data to manipulate with,
     * so a raw 2-byte array is used.
     */
    .init_offs = 0,
    .init_len  = 2,
    .max_len   = 2,
  };

  return estc_ble_char_add(service, &(estc_ble_char_opts_t)
  {
    .char_props     = { .read = 1, .write = 1 },
    .p_uuid         = &char_uuid,
    .p_user_desc    = user_desc,
    .user_desc_len  = sizeof(user_desc),
    .p_attr         = &attr_char_value,
    .p_char_handles = &service->rdwr_handles,
  });
}

static ret_code_t estc_ble_char_add(ble_estc_service_t *service, const estc_ble_char_opts_t *opts)
{
  ret_code_t error_code;

  ble_gatts_char_md_t char_md =
  {
    .char_props              = opts->char_props,
    .p_char_user_desc        = opts->p_user_desc,
    .char_user_desc_size     = opts->user_desc_len,
    .char_user_desc_max_size = opts->user_desc_len,
    .p_cccd_md               = opts->p_cccd,
  };

  error_code =
    sd_ble_gatts_characteristic_add(service->service_handle, &char_md,
                                    opts->p_attr, opts->p_char_handles);

  APP_ERROR_CHECK(error_code);

  return NRF_SUCCESS;
}

static ret_code_t estc_ble_chars_add(ble_estc_service_t *service)
{
  ret_code_t ret;
  
  ret = estc_ble_rdwr_char_add(service);
  APP_ERROR_CHECK(ret);
  
  ret = estc_ble_ntfy_char_add(service);
  APP_ERROR_CHECK(ret);

  ret = estc_ble_indy_char_add(service);
  APP_ERROR_CHECK(ret);

  return NRF_SUCCESS;
}

