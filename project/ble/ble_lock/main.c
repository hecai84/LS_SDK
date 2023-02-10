#include "Tools.h"
#include "ls_ble.h"
#include "platform.h"
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "builtin_timer.h"
#include <string.h>
#include "co_math.h"
#include "io_config.h"
#include "SEGGER_RTT.h"
#include "Motor.h"
#include "lsrtc.h"
#include "sleep.h"
#include "flash.h"
#include "parser.h"
#include "lscrypt.h"
#include "main.h"

#define UART_SVC_ADV_NAME "YILA"
#define UART_SERVER_MAX_MTU 247
#define UART_SERVER_MTU_DFT 23
#define UART_SERVER_MAX_DATA_LEN (uart_server_mtu - 3)
#define UART_SVC_RX_MAX_LEN (UART_SERVER_MAX_MTU - 3)
#define UART_SVC_TX_MAX_LEN (UART_SERVER_MAX_MTU - 3)
#define UART_SVC_BUFFER_SIZE (1024)

#define UART_SERVER_TIMEOUT 100 // timer units: ms
static const uint8_t ls_svc_uuid_128[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e};
static const uint8_t ls_rx_char_uuid_128[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e};
static const uint8_t ls_tx_char_uuid_128[] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e};
static const uint8_t att_decl_char_array[] = {0x03, 0x28};
static const uint8_t att_desc_client_char_cfg_array[] = {0x02, 0x29};
uint8_t LogBuf[30];
uint8_t dev_addr[9] = {0x77, 0x77};

extern uint8_t ciphertext_buff[];
extern uint8_t plaintext_buff[];
extern u8 pw[];
extern u8 battery;
extern bool isWorking;

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
static struct gatt_svc_env ls_uart_server_svc_env;
static uint8_t connect_id = 0xff;
// static uint8_t uart_server_buf[UART_SVC_BUFFER_SIZE];
// static uint16_t uart_server_rx_index = 0;
//static bool uart_server_ntf_done = true;
static uint16_t uart_server_mtu = UART_SERVER_MTU_DFT;
//static struct builtin_timer *uart_server_timer_inst = NULL;
static bool update_adv_intv_flag = false;
static uint16_t cccd_config = 0;

static uint8_t adv_obj_hdl;
static uint8_t advertising_data[31] = {0x05, 0x09, 'u', 'a', 'r', 't'};
static uint8_t scan_response_data[31];

// static void ls_uart_server_init(void);
// static void ls_uart_server_timer_cb(void *param);
static void ls_uart_server_read_req_ind(uint8_t att_idx, uint8_t con_idx);
static void ls_uart_server_write_req_ind(uint8_t att_idx, uint8_t con_idx, uint16_t length, uint8_t const *value);
// static void ls_uart_server_send_notification(void);
static void start_adv(void);
static void ls_uart_server_data_length_update(uint8_t con_idx);

// static void ls_uart_server_init(void)
//{
//     uart_server_timer_inst = builtin_timer_create(ls_uart_server_timer_cb);
//     builtin_timer_start(uart_server_timer_inst, UART_SERVER_TIMEOUT, NULL);
// }

// static void ls_uart_server_update_adv_interval(uint8_t input_intv)
//{
//     LOG_I("input_char: %d",input_intv);
//     uint32_t new_intv = (input_intv - '0')*160;
//     dev_manager_update_adv_interval(adv_obj_hdl, new_intv, new_intv);
//     dev_manager_stop_adv(adv_obj_hdl);
//     update_adv_intv_flag = true;
// }

//static void ls_uart_server_timer_cb(void *param)
//{
//    u16 handle;
//    if (isWorking == false)
//    {
//        // handle = gatt_manager_get_svc_att_handle(&ls_uart_server_svc_env, UART_SVC_IDX_TX_VAL);
//        if (uart_server_timer_inst)
//        {
//            builtin_timer_stop(uart_server_timer_inst);
//            builtin_timer_delete(uart_server_timer_inst);
//        }
//        // gatt_manager_server_send_notification(connect_id, handle, "OK", strlen("OK"), NULL);
//    }
//    if (uart_server_timer_inst)
//    {
//        builtin_timer_start(uart_server_timer_inst, UART_SERVER_TIMEOUT, NULL);
//    }
//}

static void ls_uart_server_read_req_ind(uint8_t att_idx, uint8_t con_idx)
{
    uint16_t handle = 0;
    if (att_idx == UART_SVC_IDX_TX_NTF_CFG)
    {
        handle = gatt_manager_get_svc_att_handle(&ls_uart_server_svc_env, att_idx);
        gatt_manager_server_read_req_reply(con_idx, handle, 0, (void *)&cccd_config, 2);
    }
}
static void ls_uart_server_write_req_ind(uint8_t att_idx, uint8_t con_idx, uint16_t length, uint8_t const *value)
{
    if (att_idx == UART_SVC_IDX_TX_NTF_CFG)
    {
        LS_ASSERT(length == 2);
        memcpy(&cccd_config, value, length);
    }
}

static void ls_uart_server_data_length_update(uint8_t con_idx)
{
    struct gap_set_pkt_size dlu_param =
        {
            .pkt_size = 251,
        };
    gap_manager_set_pkt_size(con_idx, &dlu_param);
}

static void gap_manager_callback(enum gap_evt_type type, union gap_evt_u *evt, uint8_t con_idx)
{
    switch (type)
    {
    case CONNECTED:
        connect_id = con_idx;
        SetLedBlue(LED_OPEN);
        UpdateBattery();
        LOG_I("connected!");
        break;
    case DISCONNECTED:
        connect_id = 0xff;
        uart_server_mtu = UART_SERVER_MTU_DFT;
        SetLedBlue(LED_CLOSE);
        LOG_I("disconnected!");
        start_adv();
        break;
    case CONN_PARAM_REQ:
        // LOG_I
        break;
    case CONN_PARAM_UPDATED:

        break;
    default:

        break;
    }
}

void updateAdv(void)
{
    dev_manager_stop_adv(adv_obj_hdl);
    update_adv_intv_flag = true;
}

static void gatt_manager_callback(enum gatt_evt_type type, union gatt_evt_u *evt, uint8_t con_idx)
{
    CMD_TYPE cmd;
    uint16_t handle;
    u8 para[20] = {0};
    u8 *pos;
    int procTime, waitTime,returnTime;
    switch (type)
    {
    case SERVER_READ_REQ:
        LOG_I("read req");
        ls_uart_server_read_req_ind(evt->server_read_req.att_idx, con_idx);
        break;
    case SERVER_WRITE_REQ:
        ls_uart_server_write_req_ind(evt->server_write_req.att_idx, con_idx, evt->server_write_req.length, evt->server_write_req.value);
        handle = gatt_manager_get_svc_att_handle(&ls_uart_server_svc_env, UART_SVC_IDX_TX_VAL);
        if (evt->server_write_req.att_idx == UART_SVC_IDX_TX_NTF_CFG)
            break;

        cmd = ParseCmd(evt->server_write_req.value, evt->server_write_req.length, para);
        if (cmd == CMD_OPEN)
        {
            LOG_I("start open %s", para);
            pos = (u8 *)strstr((const char *)para, ",");
            if (pos != NULL)
            {
                procTime = atoi((const char *)(para + 1));
                if(procTime>10000)
                    procTime = 10000;
                waitTime = atoi((const char *)(pos + 1));  
                if(waitTime>10000)
                    waitTime = 10000;
                pos = (u8 *)strstr((const char *)(pos + 1), ",");
                if(pos != NULL)
                    returnTime = atoi((const char *)(pos + 1));
                else
                    returnTime = procTime;
                
                if(returnTime>10000)
                    returnTime = 10000;
                

                if (para[0] == '+')
                {
                    gatt_manager_server_send_notification(connect_id, handle, "OK", strlen("OK"), NULL);
                    openDoor(procTime, waitTime,returnTime); //
                }
                else if (para[0] == '-')
                {
                    gatt_manager_server_send_notification(connect_id, handle, "OK", strlen("OK"), NULL);
                    openDoor(-procTime, waitTime,returnTime); //
                }
                else
                {
                    gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
                }
            }
            else
            {
                gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
            }
            break;
            // uart_server_timer_inst = builtin_timer_create(ls_uart_server_timer_cb);
            // builtin_timer_start(uart_server_timer_inst, UART_SERVER_TIMEOUT, NULL);
        }
        else if (cmd == CMD_PW)
        {
            if (strlen((const char *)para) == 16)
            {
                LOG_I("new pw:%s", para);
                if (strcmp((const char *)para, (const char *)pw) != 0)
                {
                    memcpy(pw, para, 16);
                    writeFlash(RECODE_PW, pw, 16);
                }
                gatt_manager_server_send_notification(connect_id, handle, "OK", strlen("OK"), NULL);
            }
            else
            {
                gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
            }
        }
        else if (cmd == CMD_BUSY)
        {
            gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
        }
        else if (cmd == CMD_OVERTIME)
        {
            gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
        }
        else if (cmd == CMD_PWERROR)
        {
            gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
        }
        else if (cmd == CMD_UNDEFINE)
        {
            gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
        }
        else
        {
            gatt_manager_server_send_notification(connect_id, handle, "ERROR", strlen("ERROR"), NULL);
        }
        break;
    case SERVER_NOTIFICATION_DONE:
        //uart_server_ntf_done = true;
        LOG_I("ntf done");
        break;
    case MTU_CHANGED_INDICATION:
        uart_server_mtu = evt->mtu_changed_ind.mtu;
        LOG_I("mtu: %d", uart_server_mtu);
        ls_uart_server_data_length_update(con_idx);
        break;
    default:
        LOG_I("Event not handled!");
        break;
    }
}

static void create_adv_obj()
{
    struct legacy_adv_obj_param adv_param = {
        .adv_intv_min = 1000,
        .adv_intv_max = 1000,
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
static void start_adv(void)
{
    LS_ASSERT(adv_obj_hdl != 0xff);
    //启动广播时更新电量
    dev_addr[sizeof(dev_addr) - 1] = battery;
    uint8_t adv_data_length = ADV_DATA_PACK(advertising_data, 2,
                                            GAP_ADV_TYPE_SHORTENED_NAME, UART_SVC_ADV_NAME, sizeof(UART_SVC_ADV_NAME),
                                            0xff, dev_addr, sizeof(dev_addr));

    dev_manager_start_adv(adv_obj_hdl, advertising_data, adv_data_length, scan_response_data, 0);
    LOG_I("adv start");
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
        bool type;
        uint16_t len = 16;
        dev_manager_get_identity_bdaddr(dev_addr + 2, &type);
        LOG_I("type:%d,addr:", type);
        LOG_HEX(dev_addr + 2, sizeof(dev_addr) - 3);
        dev_manager_add_service((struct svc_decl *)&ls_uart_server_svc);
        //初始化密码
        readFlash(RECODE_PW, pw, &len);
        if (strlen((const char *)pw) != 16)
        {
            memcpy(pw, DEFAULT_PW, 16);
        }
        LOG_I("pw:%s", pw);
        UpdateBattery();
        // ls_uart_init();
        // HAL_UART_Receive_IT(&UART_Server_Config, &uart_server_rx_byte, 1);
        // ls_uart_server_init();
    }
    break;
    case SERVICE_ADDED:
        gatt_manager_svc_register(evt->service_added.start_hdl, UART_SVC_ATT_NUM, &ls_uart_server_svc_env);
        create_adv_obj();
        break;
    case ADV_OBJ_CREATED:
        LS_ASSERT(evt->obj_created.status == 0);
        adv_obj_hdl = evt->obj_created.handle;
        start_adv();
        break;
    case ADV_STOPPED:
        if (update_adv_intv_flag)
        {
            update_adv_intv_flag = false;
            start_adv();
        }
        break;
    case SCAN_STOPPED:

        break;
    default:

        break;
    }
}

int main()
{	
    //上电启动增加延时，否则开机读取电量不准
    int i,j;
    for(i=0;i<5000;i++)
    {
        for(j=0;j<1000;j++)
        ;
    }
    sys_init_app();
    initFlash();
    lsadc_init();
    initMotor();
    initLed();
    initExti();
    ble_init();
    dev_manager_init(dev_manager_callback);
    gap_manager_init(gap_manager_callback);
    gatt_manager_init(gatt_manager_callback);
    
    ble_loop();
}
