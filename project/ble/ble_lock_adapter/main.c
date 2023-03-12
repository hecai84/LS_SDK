#include "main.h"
#include "ls_ble.h"
#include "platform.h"
#include "prf_diss.h"
#include "parser.h"
#include "flash.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "lsuart.h"
#include "builtin_timer.h"
#include <string.h>
#include "co_math.h"
#include "io_config.h"
#include "reg_base_addr.h"
#define UART_SVC_ADV_NAME "YILA ADAPTER"
#define UART_SERVER_MAX_MTU 517
#define UART_SERVER_MTU_DFT 23
#define UART_SVC_RX_MAX_LEN (UART_SERVER_MAX_MTU - 3)
#define UART_SVC_TX_MAX_LEN (UART_SERVER_MAX_MTU - 3)
#define UART_SERVER_MAX_DATA_LEN(i) (uart_server_mtu_array[i] - 3)

#define UART_CLIENT_MAX_DATA_LEN(i) (uart_client_mtu_array[i] - 3)

#define UART_SERVER_MASTER_NUM 2 // SDK_MAX_CONN_NUM
#define UART_CLIENT_NUM (SDK_MAX_CONN_NUM - UART_SERVER_MASTER_NUM)
#define UART_SERVER_TIMEOUT 50 // timer units: ms

#define CON_IDX_INVALID_VAL 0xff
#define UART_SYNC_BYTE 0xA5
#define UART_SYNC_BYTE_LEN 1
#define UART_LEN_LEN 2
#define UART_LINK_ID_LEN 1
#define UART_HEADER_LEN (UART_SYNC_BYTE_LEN + UART_LEN_LEN + UART_LINK_ID_LEN)
#define UART_SVC_BUFFER_SIZE (UART_SERVER_MAX_MTU + UART_HEADER_LEN)
#define UART_RX_PAYLOAD_LEN_MAX (UART_SVC_BUFFER_SIZE - UART_HEADER_LEN)
#define UART_TX_PAYLOAD_LEN_MAX (UART_SVC_BUFFER_SIZE - UART_HEADER_LEN)

#define CONNECTED_MSG_PATTERN 0x5ce5
#define CONNECTED_MSG_PATTERN_LEN 2
#define DISCONNECTED_MSG_PATTERN 0xdead
#define DISCONNECTED_MSG_PATTERN_LEN 2


static const uint8_t ls_svc_uuid_128[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e};
static const uint8_t ls_rx_char_uuid_128[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e};
static const uint8_t ls_tx_char_uuid_128[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e};
static const uint8_t att_decl_char_array[] = {0x03, 0x28};
static const uint8_t att_desc_client_char_cfg_array[] = {0x02, 0x29};
extern u8 buff[];
u16 curHandle=0;
u8 curConid = 0;

enum uart_svc_att_db_handles
{
    UART_SVC_IDX_RX_CHAR,
    UART_SVC_IDX_RX_VAL,
    UART_SVC_IDX_TX_CHAR,
    UART_SVC_IDX_TX_VAL,
    UART_SVC_IDX_TX_NTF_CFG,
    UART_SVC_ATT_NUM
};

static const struct att_decl ls_uart_server_att_decl[UART_SVC_ATT_NUM] =
    {
        [UART_SVC_IDX_RX_CHAR] = {
            .uuid = att_decl_char_array,
            .s.max_len = 0,
            .s.uuid_len = UUID_LEN_16BIT,
            .s.read_indication = 1,
            .char_prop.rd_en = 1,
        },
        [UART_SVC_IDX_RX_VAL] = {
            .uuid = ls_rx_char_uuid_128,
            .s.max_len = UART_SVC_RX_MAX_LEN,
            .s.uuid_len = UUID_LEN_128BIT,
            .s.read_indication = 1,
            .char_prop.wr_cmd = 1,
            .char_prop.wr_req = 1,
        },
        [UART_SVC_IDX_TX_CHAR] = {
            .uuid = att_decl_char_array,
            .s.max_len = 0,
            .s.uuid_len = UUID_LEN_16BIT,
            .s.read_indication = 1,
            .char_prop.rd_en = 1,
        },
        [UART_SVC_IDX_TX_VAL] = {
            .uuid = ls_tx_char_uuid_128,
            .s.max_len = UART_SVC_TX_MAX_LEN,
            .s.uuid_len = UUID_LEN_128BIT,
            .s.read_indication = 1,
            .char_prop.ntf_en = 1,
        },
        [UART_SVC_IDX_TX_NTF_CFG] = {
            .uuid = att_desc_client_char_cfg_array,
            .s.max_len = 0,
            .s.uuid_len = UUID_LEN_16BIT,
            .s.read_indication = 1,
            .char_prop.rd_en = 1,
            .char_prop.wr_req = 1,
        },
};
static const struct svc_decl ls_uart_server_svc =
    {
        .uuid = ls_svc_uuid_128,
        .att = (struct att_decl *)ls_uart_server_att_decl,
        .nb_att = UART_SVC_ATT_NUM,
        .uuid_len = UUID_LEN_128BIT,
};
enum uart_rx_status
{
    UART_IDLE,
    UART_SYNC,
    UART_LEN_REV,
    UART_LINK_ID,
    UART_RECEIVING,
};
static bool uart_tx_busy;
static uint8_t dev_addr_type = 0; /*  0:Public, 1:Private */
static struct gatt_svc_env ls_uart_server_svc_env;
// static uint8_t connected_num = 0;
static uint8_t uart_server_connected_num = 0;
static uint8_t uart_server_ble_buf_array[UART_SERVER_MASTER_NUM][UART_SVC_BUFFER_SIZE];
static uint16_t uart_server_recv_data_length_array[UART_SERVER_MASTER_NUM];
static UART_HandleTypeDef UART_Server_Config;
// static bool uart_server_tx_busy;
static bool uart_server_ntf_done_array[UART_SERVER_MASTER_NUM];
static uint16_t uart_server_mtu_array[UART_SERVER_MASTER_NUM];
static uint8_t con_idx_array[UART_SERVER_MASTER_NUM];
static uint16_t cccd_config_array[UART_SERVER_MASTER_NUM];

SLAVE_DATA slave_array[MAX_SLAVE];
uint8_t slaveCount;
SLAVE_DATA * curSlave;
/************************************************data for client*****************************************************/
enum initiator_status
{
    INIT_IDLE,
    INIT_BUSY,
};
enum scan_status
{
    SCAN_IDLE,
    SCAN_BUSY,
};
static uint8_t uart_client_connected_num = 0;
static uint8_t con_idx_client_array[UART_CLIENT_NUM];
static bool uart_client_wr_cmd_done_array[UART_CLIENT_NUM];
static uint16_t uart_client_mtu_array[UART_CLIENT_NUM];
static uint8_t uart_client_tx_buf[UART_CLIENT_NUM][UART_SVC_BUFFER_SIZE];

static uint16_t uart_client_svc_attribute_handle[UART_CLIENT_NUM]; // handle for primary service attribute handle
static uint16_t uart_client_svc_end_handle[UART_CLIENT_NUM];

static uint16_t uart_client_tx_attribute_handle[UART_CLIENT_NUM];
static uint16_t uart_client_tx_pointer_handle[UART_CLIENT_NUM];

static uint16_t uart_client_rx_attribute_handle[UART_CLIENT_NUM];
static uint16_t uart_client_rx_pointer_handle[UART_CLIENT_NUM];

static uint16_t uart_client_cccd_handle[UART_CLIENT_NUM];
/********************************************************************************************************************/
static struct builtin_timer *uart_server_timer_inst = NULL;

static uint8_t adv_obj_hdl;
static uint8_t advertising_data[28];
static uint8_t scan_response_data[31];
static uint8_t scan_obj_hdl = 0xff;
static uint8_t init_obj_hdl = 0xff;
static uint8_t init_status = INIT_IDLE;

static void ls_uart_server_init(uint8_t idx);
static void ls_uart_server_timer_cb(void *param);
static void ls_uart_server_read_req_ind(uint8_t att_idx, uint8_t con_idx);
static void ls_uart_server_write_req_ind(uint8_t att_idx, uint8_t con_idx, uint16_t length,const uint8_t *value);
static void ls_uart_server_send_notification(void);

static void start_scan(void);

static void ls_uart_client_service_dis(uint8_t con_idx);
static void ls_uart_client_char_tx_dis(uint8_t con_idx);
static void ls_uart_client_char_rx_dis(uint8_t con_idx);
static void ls_uart_client_char_desc_dis(uint8_t con_idx);
static void ls_uart_client_send_write_req(void);


static uint8_t search_conidx(uint8_t con_idx)
{
    uint8_t index = 0xff;
    for (uint8_t i = 0; i < UART_SERVER_MASTER_NUM; i++)
    {
        if (con_idx_array[i] == con_idx)
        {
            index = i;
            break;
        }
    }
    return index;
}
static SLAVE_DATA *search_slave(uint8_t *addr)
{
    uint8_t i;
    for (i = 0; i < slaveCount; i++)
    {
        if (slave_array[i].flag != 0xff &&
            memcmp(slave_array[i].mac, addr, BLE_ADDR_LEN)==0)
        {
            return &slave_array[i];
        }
    }
    return NULL;
}

static uint8_t search_client_conidx(uint8_t con_idx)
{
    uint8_t index = 0xff;
    for (uint8_t i = 0; i < UART_CLIENT_NUM; i++)
    {
        if (con_idx_client_array[i] == con_idx)
        {
            index = i;
            break;
        }
    }
    return index;
}
static void ls_uart_server_init(uint8_t idx)
{
    if (idx < UART_SERVER_MASTER_NUM)
    {
        con_idx_array[idx] = CON_IDX_INVALID_VAL;
        uart_server_ntf_done_array[idx] = true;
        uart_server_mtu_array[idx] = UART_SERVER_MTU_DFT;
        uart_server_recv_data_length_array[idx] = 0;
    }
    else
    {
        if (NULL == uart_server_timer_inst)
        {
            uart_server_timer_inst = builtin_timer_create(ls_uart_server_timer_cb);
            builtin_timer_start(uart_server_timer_inst, UART_SERVER_TIMEOUT, NULL);
        }
        for (uint8_t i = 0; i < UART_SERVER_MASTER_NUM; i++)
        {
            con_idx_array[i] = CON_IDX_INVALID_VAL;
            uart_server_ntf_done_array[i] = true;
            uart_server_mtu_array[i] = UART_SERVER_MTU_DFT;
            uart_server_recv_data_length_array[i] = 0;
        }
    }
}

static void ls_uart_server_timer_cb(void *param)
{
    // ls_uart_server_send_notification();
    //ls_uart_client_send_write_req();
    if (uart_server_timer_inst)
    {
        builtin_timer_start(uart_server_timer_inst, UART_SERVER_TIMEOUT, NULL);
    }
}

static void ls_uart_server_read_req_ind(uint8_t att_idx, uint8_t con_idx)
{
    uint16_t handle = 0;
    uint8_t array_idx = search_conidx(con_idx);
    LS_ASSERT(array_idx != 0xff);
    if (att_idx == UART_SVC_IDX_TX_NTF_CFG)
    {
        handle = gatt_manager_get_svc_att_handle(&ls_uart_server_svc_env, att_idx);
        gatt_manager_server_read_req_reply(con_idx, handle, 0, (void *)&cccd_config_array[array_idx], 2);
    }
}
/**
 * @description: 接收主机发送的消息
 * @param {uint8_t} att_idx
 * @param {uint8_t} con_idx
 * @param {uint16_t} length
 * @return {*}
 */
static void ls_uart_server_write_req_ind(uint8_t att_idx, uint8_t con_idx, uint16_t length,const uint8_t *value)
{
    LOG_I("rec %d", length);
    //一个锁的相关数据总共21个字节
    if(length>0 && length%21==0&& length/21 <=MAX_SLAVE)
    {
        saveSlave(value, length);
    }
}
static void ls_uart_server_send_notification(void)
{
    for (uint8_t idx = 0; idx < UART_SERVER_MASTER_NUM; idx++)
    {
        uint8_t con_idx = con_idx_array[idx];
        uint32_t cpu_stat = enter_critical();
        if (con_idx != CON_IDX_INVALID_VAL && uart_server_recv_data_length_array[idx] != 0 && uart_server_ntf_done_array[idx])
        {
            uart_server_ntf_done_array[idx] = false;
            uint16_t handle = gatt_manager_get_svc_att_handle(&ls_uart_server_svc_env, UART_SVC_IDX_TX_VAL);
            uint16_t tx_len = uart_server_recv_data_length_array[idx] > co_min(UART_SERVER_MAX_DATA_LEN(idx), UART_SVC_TX_MAX_LEN) ? co_min(UART_SERVER_MAX_DATA_LEN(idx), UART_SVC_TX_MAX_LEN) : uart_server_recv_data_length_array[idx];
            uart_server_recv_data_length_array[idx] -= tx_len;
            gatt_manager_server_send_notification(con_idx, handle, &uart_server_ble_buf_array[idx][0], tx_len, NULL);
            memcpy((void *)&uart_server_ble_buf_array[idx][0], (void *)&uart_server_ble_buf_array[idx][tx_len], uart_server_recv_data_length_array[idx]);
        }
        exit_critical(cpu_stat);
    }
}
static void ls_uart_client_recv_ntf_ind(uint8_t handle, uint8_t con_idx, uint16_t length, uint8_t const *value)
{
    uint8_t array_idx = search_client_conidx(con_idx);
    LS_ASSERT(array_idx != 0xff);
    LOG_I("ls_uart_client_recv_ntf_ind, uart_client_tx_buf[%d][0] = %d, uart_tx_busy = %d", array_idx, uart_client_tx_buf[array_idx][0], uart_tx_busy);
    if (uart_client_tx_buf[array_idx][0] != UART_SYNC_BYTE)
    {
        LS_ASSERT(length <= UART_TX_PAYLOAD_LEN_MAX);
        uart_client_tx_buf[array_idx][0] = UART_SYNC_BYTE;
        uart_client_tx_buf[array_idx][1] = length & 0xff;
        uart_client_tx_buf[array_idx][2] = (length >> 8) & 0xff;
        uart_client_tx_buf[array_idx][3] = con_idx; // what uart will receive should be the real connection index. array_idx is internal.
        memcpy((void *)&uart_client_tx_buf[array_idx][UART_HEADER_LEN], value, length);
        uint32_t cpu_stat = enter_critical();
        if (!uart_tx_busy)
        {
            uart_tx_busy = true;
            HAL_UART_Transmit_IT(&UART_Server_Config, &uart_client_tx_buf[array_idx][0], length + UART_HEADER_LEN);
        }
        exit_critical(cpu_stat);
    }
}
static void ls_uart_client_send_write_req(void)
{
    u8 * data;
    u8 len;
    // u8 test[] = {0x24,0x60,0x44,0x0C,0xD9,0xD1,0xDE,0xD5,0xFB,0xAD,0xDD,0xDD,0x4F,0x31,0x0A,0xAC,0x3E,0x75,0xD8,0x4A,0x7B,0x8B,0x89,0xEA,0xBA,0x10,0xDC,0x1D,0x6A,0x31,0xF4,0xAD,0xE2,0x70,0x2F,0xF8,0x48,0xDB,0xD3,0xC5,0x21,0x86,0xF3,0x1E,0x93,0xB7,0xB0,0xDB,0xB8,0xE1,0xF0,0xF0,0x75,0x60,0x8B,0x1B,0xB0,0x1D,0xAC,0x0E,0xC3,0x48,0xEB,0xDB};

    if(curHandle && curSlave!=NULL)
    {
        LOG_I("ls_uart_client_send_write_req");
        uint32_t cpu_stat = enter_critical();
        //gatt_manager_client_write_no_rsp(con_idx, uart_client_rx_pointer_handle[idx], (uint8_t *)sendData, strlen(sendData));
        encodeSlave(curSlave, &data, &len);
        curSlave->flag = 0xff;
        gatt_manager_client_write_no_rsp(curConid, curHandle, data, len);
        curHandle = 0;
        exit_critical(cpu_stat);
    }
}

void updateAdv(void)
{
    // dev_manager_stop_adv(adv_obj_hdl);
    // update_adv_intv_flag = true;
}

static void gap_manager_callback(enum gap_evt_type type, union gap_evt_u *evt, uint8_t con_idx)
{
    uint8_t search_idx = 0xff;
    switch (type)
    {
    case CONNECTED:
        if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_SLAVE)
        {
            uart_server_connected_num++;
            search_idx = search_conidx(con_idx);
            LS_ASSERT(search_idx == 0xff);                   // new con_idx should be not found
            search_idx = search_conidx(CON_IDX_INVALID_VAL); // search the first idle idx
            LS_ASSERT(search_idx != 0xff);
            con_idx_array[search_idx] = con_idx;
        }
        else
        {
            uart_client_connected_num++;
            // search_idx = search_client_conidx(con_idx);
            // LS_ASSERT(search_idx == 0xff); // new con_idx should be not found
            // search_idx = search_client_conidx(CON_IDX_INVALID_VAL); // search the first idle idx
            // LS_ASSERT(search_idx != 0xff);
            con_idx_client_array[1] = con_idx;

            // ls_uart_client_service_dis(con_idx);
            gatt_manager_client_mtu_exch_send(con_idx);
        }
        LOG_I("connected! new con_idx = %d", con_idx);
        break;
    case DISCONNECTED:
        LOG_I("disconnected! delete con_idx = %d", con_idx);
        if ((search_idx = search_conidx(con_idx)) != 0xff)
        {
            uart_server_connected_num--;
            ls_uart_server_init(search_idx);
            if (uart_server_connected_num == UART_SERVER_MASTER_NUM - 1)
            {
                start_adv();
            }
        }
        else if ((search_idx = search_client_conidx(con_idx)) != 0xff)
        {
            uart_client_connected_num--;
            if (uart_client_connected_num < UART_CLIENT_NUM)
            {
                init_status = INIT_IDLE;
                start_scan();
            }
        }
        else
        {
            LS_ASSERT(0);
        }
        break;
    case CONN_PARAM_REQ:

        break;
    case CONN_PARAM_UPDATED:

        break;
    default:

        break;
    }
}

static void gatt_manager_callback(enum gatt_evt_type type, union gatt_evt_u *evt, uint8_t con_idx)
{
    uint8_t array_idx;
    if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_SLAVE)
    {
        array_idx = search_conidx(con_idx);
    }
    else
    {
        array_idx = search_client_conidx(con_idx);
    }
    LS_ASSERT(array_idx != 0xff);
    LOG_I("con_idx = %d", con_idx);
    switch (type)
    {
    case SERVER_READ_REQ:
        LOG_I("read req");
        ls_uart_server_read_req_ind(evt->server_read_req.att_idx, con_idx);
        break;
    case SERVER_WRITE_REQ:
        LOG_I("write req");
        ls_uart_server_write_req_ind(evt->server_write_req.att_idx, con_idx, evt->server_write_req.length, evt->server_write_req.value);
        break;
    case SERVER_NOTIFICATION_DONE:
        uart_server_ntf_done_array[array_idx] = true;
        LOG_I("ntf done");
        break;
    case MTU_CHANGED_INDICATION:
        if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_SLAVE)
        {
            uart_server_mtu_array[array_idx] = evt->mtu_changed_ind.mtu;
        }
        else
        {
            uart_client_mtu_array[array_idx] = evt->mtu_changed_ind.mtu;
            ls_uart_client_service_dis(con_idx);
        }

        LOG_I("mtu exch ind, mtu = %d", evt->mtu_changed_ind.mtu);
        break;
    case CLIENT_RECV_NOTIFICATION:
        //ls_uart_client_recv_ntf_ind(evt->client_recv_notify_indicate.handle, con_idx, evt->client_recv_notify_indicate.length, evt->client_recv_notify_indicate.value);
        LOG_I("svc dis notification, length = %d", evt->client_recv_notify_indicate.length);
        break;
    case CLIENT_PRIMARY_SVC_DIS_IND:
        if (!memcmp(evt->client_svc_disc_indicate.uuid, ls_svc_uuid_128, sizeof(ls_svc_uuid_128)))
        {
            uart_client_svc_attribute_handle[array_idx] = evt->client_svc_disc_indicate.handle_range.begin_handle;
            uart_client_svc_end_handle[array_idx] = evt->client_svc_disc_indicate.handle_range.end_handle;
            ls_uart_client_char_tx_dis(con_idx);
            LOG_I("svc dis success, attribute_handle = %d, end_handle = %d", uart_client_svc_attribute_handle[array_idx], uart_client_svc_end_handle[array_idx]);
        }
        else
        {
            LOG_I("unexpected svc uuid");
        }
        break;
    case CLIENT_CHAR_DIS_BY_UUID_IND:
        if (!memcmp(evt->client_disc_char_indicate.uuid, ls_tx_char_uuid_128, sizeof(ls_tx_char_uuid_128)))
        {
            uart_client_tx_attribute_handle[array_idx] = evt->client_disc_char_indicate.attr_handle;
            uart_client_tx_pointer_handle[array_idx] = evt->client_disc_char_indicate.pointer_handle;
            ls_uart_client_char_rx_dis(con_idx);
            LOG_I("tx dis success, attribute handle = %d, pointer handler = %d", uart_client_tx_attribute_handle[array_idx], uart_client_tx_pointer_handle[array_idx]);
            
        }
        else if (!memcmp(evt->client_disc_char_indicate.uuid, ls_rx_char_uuid_128, sizeof(ls_rx_char_uuid_128)))
        {
            uart_client_rx_attribute_handle[array_idx] = evt->client_disc_char_indicate.attr_handle;
            uart_client_rx_pointer_handle[array_idx] = evt->client_disc_char_indicate.pointer_handle;
            // ls_uart_client_char_desc_dis(con_idx);
            curHandle = uart_client_rx_pointer_handle[array_idx];
            curConid = con_idx;
            LOG_I("rx dis success, attribute handle = %d, pointer handler = %d,slave=%d", uart_client_rx_attribute_handle[array_idx], uart_client_rx_pointer_handle[array_idx],curSlave);
            ls_uart_client_send_write_req();
        }
        else
        {
            LOG_I("unexpected char uuid");
        }
        break;
    case CLIENT_CHAR_DESC_DIS_BY_UUID_IND:
        if (!memcmp(evt->client_disc_char_desc_indicate.uuid, att_desc_client_char_cfg_array, sizeof(att_desc_client_char_cfg_array)))
        {
            uart_client_cccd_handle[array_idx] = evt->client_disc_char_desc_indicate.attr_handle;
            LOG_I("cccd dis success, cccd handle = %d", uart_client_cccd_handle[array_idx]);
            gatt_manager_client_cccd_enable(con_idx, uart_client_cccd_handle[array_idx], 1, 0);
        }
        else
        {
            LOG_I("unexpected desc uuid");
        }
        break;
    case CLIENT_WRITE_WITH_RSP_DONE:
        if (evt->client_write_rsp.status == 0)
        {
            LOG_I("write success");
        }
        else
        {
            LOG_I("write fail, status = %d", evt->client_write_rsp.status);
        }
        break;
    case CLIENT_WRITE_NO_RSP_DONE:
        if (evt->client_write_no_rsp.status == 0)
        {
            LS_ASSERT(gap_manager_get_role(con_idx) == LS_BLE_ROLE_MASTER);
            uart_client_wr_cmd_done_array[array_idx] = true;
            LOG_I("write no rsp success");
            curSlave = NULL;
            // dev_manager_stop_init(init_obj_hdl);
        }
        else
        {
            LOG_I("write fail, status = %d", evt->client_write_rsp.status);
        }
        break;
    default:
        LOG_I("Event not handled!");
        break;
    }
}

static void ls_uart_client_service_dis(uint8_t con_idx)
{
    gatt_manager_client_svc_discover_by_uuid(con_idx, (uint8_t *)&ls_svc_uuid_128[0], UUID_LEN_128BIT, 1, 0xffff);
}

static void ls_uart_client_char_tx_dis(uint8_t con_idx)
{
    uint8_t array_idx = search_client_conidx(con_idx);
    LS_ASSERT(array_idx != 0xff);
    gatt_manager_client_char_discover_by_uuid(con_idx, (uint8_t *)&ls_tx_char_uuid_128[0], UUID_LEN_128BIT, uart_client_svc_attribute_handle[array_idx], uart_client_svc_end_handle[array_idx]);
}

static void ls_uart_client_char_rx_dis(uint8_t con_idx)
{
    uint8_t array_idx = search_client_conidx(con_idx);
    LS_ASSERT(array_idx != 0xff);
    gatt_manager_client_char_discover_by_uuid(con_idx, (uint8_t *)&ls_rx_char_uuid_128[0], UUID_LEN_128BIT, uart_client_svc_attribute_handle[array_idx], uart_client_svc_end_handle[array_idx]);
}

static void ls_uart_client_char_desc_dis(uint8_t con_idx)
{
    uint8_t array_idx = search_client_conidx(con_idx);
    LS_ASSERT(array_idx != 0xff);
    gatt_manager_client_desc_char_discover(con_idx, uart_client_svc_attribute_handle[array_idx], uart_client_svc_end_handle[array_idx]);
}

static void create_adv_obj()
{
    struct legacy_adv_obj_param adv_param = {
        .adv_intv_min = 0x80,
        .adv_intv_max = 0x80,
        .own_addr_type = PUBLIC_OR_RANDOM_STATIC_ADDR,
        .filter_policy = 0,
        .ch_map = 0x7,
        .disc_mode = ADV_MODE_GEN_DISC,
        .prop = {
            .connectable = 1,
            .scannable = 1,
            .directed = 0,
            .high_duty_cycle = 0,
        },
    };
    dev_manager_create_legacy_adv_object(&adv_param);
}
void start_adv(void)
{
    LS_ASSERT(adv_obj_hdl != 0xff);
    uint8_t adv_data_length = ADV_DATA_PACK(advertising_data, 1, GAP_ADV_TYPE_SHORTENED_NAME, UART_SVC_ADV_NAME, sizeof(UART_SVC_ADV_NAME));
    dev_manager_start_adv(adv_obj_hdl, advertising_data, adv_data_length, scan_response_data, 0);
    LOG_I("adv start");
    SetLedBlue(LED_OPEN);
}
static void create_scan_obj(void)
{
    dev_manager_create_scan_object(PUBLIC_OR_RANDOM_STATIC_ADDR);
}
static void create_init_obj(void)
{
    dev_manager_create_init_object(PUBLIC_OR_RANDOM_STATIC_ADDR);
}
static void start_init(uint8_t *peer_addr)
{
    struct dev_addr peer_dev_addr_str;
    memcpy(peer_dev_addr_str.addr, peer_addr, BLE_ADDR_LEN);
    struct start_init_param init_param = {
        .scan_intv = 64,
        .scan_window = 48,
        .conn_to = 0,
        .conn_intv_min = 16,
        .conn_intv_max = 16,
        .conn_latency = 0,
        .supervision_to = 200,

        .peer_addr = &peer_dev_addr_str,
        .peer_addr_type = dev_addr_type,
        .type = DIRECT_CONNECTION,
    };
    dev_manager_start_init(init_obj_hdl, &init_param);
}
static void start_scan(void)
{
    if(slaveCount>0)
    {
        struct start_scan_param scan_param = {
            .scan_intv = 0x4000,
            .scan_window = 0x4000,
            .duration = 0,
            .period = 0,
            .type = OBSERVER,
            .active = 0,
            .filter_duplicates = 0,
        };
        dev_manager_start_scan(scan_obj_hdl, &scan_param);
        LOG_I("start scan");
    }
}
static void dev_manager_callback(enum dev_evt_type type, union dev_evt_u *evt)
{
    switch (type)
    {
    case STACK_INIT:
    {
        struct ble_stack_cfg cfg = {
            .private_addr = false,
            .controller_privacy = false,
        };
        dev_manager_stack_init(&cfg);
    }
    break;
    case STACK_READY:
    {
        uint8_t addr[6];
        bool type;
        dev_manager_get_identity_bdaddr(addr, &type);
        LOG_I("type:%d,addr:", type);
        LOG_HEX(addr, sizeof(addr));
        dev_manager_add_service((struct svc_decl *)&ls_uart_server_svc);
        ls_uart_server_init(0xff);
        //encodeInfo("163948687749ba59abbe56e057A:OPEN;P:+2000;", strlen("163948687749ba59abbe56e057A:OPEN;P:+2000;"));
    }
    break;
    case SERVICE_ADDED:
        gatt_manager_svc_register(evt->service_added.start_hdl, UART_SVC_ATT_NUM, &ls_uart_server_svc_env);
        create_adv_obj();
        break;
    case ADV_OBJ_CREATED:
        LOG_I("ADV_OBJ_CREATED");
        LS_ASSERT(evt->obj_created.status == 0);
        adv_obj_hdl = evt->obj_created.handle;
        //没有设备的时候默认打开adv
        if(slaveCount==0)
            start_adv();
        if (scan_obj_hdl == 0xff)
        {
            create_scan_obj();
        }
        break;
    case ADV_STOPPED:
        LOG_I("ADV_STOPPED");
        // LOG_I("adv stopped, uart_server_connected_num = %d", uart_server_connected_num);
        // if (uart_server_connected_num < UART_SERVER_MASTER_NUM)
        // {
        //     start_adv();
        // }
        break;
    case SCAN_OBJ_CREATED:
        LOG_I("SCAN_OBJ_CREATED");
        LS_ASSERT(evt->obj_created.status == 0);
        scan_obj_hdl = evt->obj_created.handle;
        start_scan();
        create_init_obj();
        break;
    case SCAN_STOPPED:
        if (curSlave)
        {
            LOG_I("scan stopped, connect");
            start_init(curSlave->mac);
            init_status = INIT_BUSY;
        }
        break;
    case ADV_REPORT:
        if(curSlave==NULL)
        {
            curSlave = search_slave(evt->adv_report.adv_addr->addr);
            if(curSlave)
            {
                LOG_I("adv received, addr: %02X:%02X:%02X:%02X:%02X:%02X", evt->adv_report.adv_addr->addr[5],
                        evt->adv_report.adv_addr->addr[4],
                        evt->adv_report.adv_addr->addr[3],
                        evt->adv_report.adv_addr->addr[2],
                        evt->adv_report.adv_addr->addr[1],
                        evt->adv_report.adv_addr->addr[0]);            
                if (init_obj_hdl != 0xff && init_status == INIT_IDLE )
                {
                    dev_addr_type = evt->adv_report.adv_addr_type;
                    dev_manager_stop_scan(scan_obj_hdl);
                }
            }
        }
        break;
    case INIT_OBJ_CREATED:
        LS_ASSERT(evt->obj_created.status == 0);
        init_obj_hdl = evt->obj_created.handle;
        break;
    case INIT_STOPPED:
        init_status = INIT_IDLE;
        LOG_I("init stopped");
        // start_scan();
        break;
    default:

        break;
    }
}

int main()
{
    sys_init_app();
    initLed();
    initExti();
    initFlash();
    loadSlave(slave_array,&slaveCount);
    ble_init();
    dev_manager_init(dev_manager_callback);
    gap_manager_init(gap_manager_callback);
    gatt_manager_init(gatt_manager_callback);
    ble_loop();
}
