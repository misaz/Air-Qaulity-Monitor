/***********************************************************************************************************************
 * Copyright [2020-2021] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "r_sci_uart.h"
#include "r_ioport.h"
#include "rm_da16600.h"
#include "rm_fifo.h"
#include "rm_time.h"
#include "rm_circ_buf.h"

//#define DA16600_PRINTF_DEBUG_ENABLED

#ifdef DA16600_PRINTF_DEBUG_ENABLED
#include "common_utils.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define DA16600_DEBUG_STATS_ENABLED              (1)

/* Predefined timeout values */
#define DA16600_TIMEOUT_NONE                     (0)
#define DA16600_TIMEOUT_10MS                     (10)
#define DA16600_TIMEOUT_25MS                     (25)
#define DA16600_TIMEOUT_30MS                     (30)
#define DA16600_TIMEOUT_100MS                    (100)
#define DA16600_TIMEOUT_200MS                    (200)
#define DA16600_TIMEOUT_300MS                    (300)
#define DA16600_TIMEOUT_400MS                    (400)
#define DA16600_TIMEOUT_500MS                    (500)
#define DA16600_TIMEOUT_1SEC                     (1000)
#define DA16600_TIMEOUT_2SEC                     (2000)
#define DA16600_TIMEOUT_3SEC                     (3000)
#define DA16600_TIMEOUT_4SEC                     (4000)
#define DA16600_TIMEOUT_5SEC                     (5000)
#define DA16600_TIMEOUT_8SEC                     (8000)
#define DA16600_TIMEOUT_10SEC                    (10000)
#define DA16600_TIMEOUT_15SEC                    (15000)
#define DA16600_TIMEOUT_20SEC                    (20000)

/* Text full versions of AT command returns */
#define DA16600_RETURN_TEXT_OK                   "OK"
#define DA16600_RETURN_TEXT_ERROR                "ERROR"

/* Pin or port invalid definition */
#define DA16600_BSP_PIN_PORT_INVALID             (UINT16_MAX)

/* Initial DA16600 module UART settings */
#define DA16600_DEFAULT_BAUDRATE                 115200
#define DA16600_DEFAULT_MODULATION               false
#define DA16600_DEFAULT_ERROR                    9000

/* Maximum length of an AT command (including prefix and suffix) that can be transmitted */
#define DA16600_COMMAND_LEN_MAX                  128

/* Unique number for DA16600 Open status */
#define DA16600_OPEN                             (0x31363630UL) // "1660" in ASCII

/***********************************************************************************************************************
 * Private constants
 **********************************************************************************************************************/
const uint8_t g_da16600_return_text_ok[]          = DA16600_RETURN_TEXT_OK;
const uint8_t g_da16600_return_text_error[]       = DA16600_RETURN_TEXT_ERROR;

/***********************************************************************************************************************
 * Enumerations
 **********************************************************************************************************************/
typedef enum e_da16600_g_rx_state
{
    DA16600_RX_STATE_IDLE = 0,
    DA16600_RX_STATE_PREFIX,
    DA16600_RX_STATE_DATA,
    DA16600_RX_STATE_SUFFIX,
} da16600_g_rx_state_t;

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef void (* rm_da16600_rsp_handler_func_t)(uint8_t * p_buf);

typedef struct
{
    char                        * p_rsp;
    rm_da16600_rsp_handler_func_t handler;
} rm_da16600_rsp_handler_t;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static void rm_da16600_process_rxd_char(uint8_t ch);
static void rm_da16600_process_rxd_message(uint8_t * p_buf);
static da16600_err_t rm_da16600_send_write_cmd(da16600_instance_ctrl_t * p_instance_ctrl,
                                               const uint8_t           * p_textstring,
                                               uint32_t                  timeout_ms);
static da16600_err_t rm_da16600_send_read_cmd(da16600_instance_ctrl_t *  p_instance_ctrl,
                                              const uint8_t           *  p_textstring,
                                              uint32_t                   timeout_ms,
                                              uint8_t                 ** p_rsp_buf);
static bool rm_da16600_is_str_ok(const char * s);
static bool rm_da16600_is_str_err(const char * s);
static bool rm_da16600_is_str_rsp(const char * s);
static void rm_da16600_handle_init(uint8_t * p_buf);
static void rm_da16600_handle_wfdap(uint8_t * p_buf);
static void rm_da16600_handle_wfjap(uint8_t * p_buf);
static void rm_da16600_handle_trcts(uint8_t * p_buf);
static void rm_da16600_handle_trxts(uint8_t * p_buf);
static void rm_da16600_handle_trdts(uint8_t * p_buf);
static void rm_da16600_handle_data_chunk(uint8_t * p_buf);
static char * rm_da16600_findnchr(const char *str, char chr, int occ);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static da16600_instance_ctrl_t g_rm_da16600_instance;
static da16600_callback_args_t g_rm16600_callback_args;
static da16600_g_rx_state_t g_rx_state;
static char g_da16600_cmd_in_progress[DA16600_COMMAND_LEN_MAX];
static bool g_rx_data_in_progress = false;
static uint32_t g_rx_data_len = 0;
static rm_circ_buf_t rx_circ_buf;
static uint8_t uart_rx_buffer[1024];

static const rm_da16600_rsp_handler_t rsp_handlers[] =
{
    { "+INIT",                 rm_da16600_handle_init  },
    { "+WFJAP",                rm_da16600_handle_wfjap },
    { "+WFDAP",                rm_da16600_handle_wfdap },
    { "+TRCTS",                rm_da16600_handle_trcts },
    { "+TRXTS",                rm_da16600_handle_trxts },
    { "+TRDTS",                rm_da16600_handle_trdts },
    { NULL,                    NULL                    },
};

static baud_setting_t g_baud_setting_115200 =
{.brme = 0, .abcse = 0, .abcs = 0, .bgdm = 0, .brr = 0, .mddr = 0, };

#if (DA16600_DEBUG_STATS_ENABLED == 1)
static volatile uint32_t g_rx_msg_count;
static volatile uint32_t g_tx_msg_count;
#endif

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 *  Opens and configures the DA16600 Middleware module.
 *
 *  @retval DA16600_SUCCESS          DA16600 successfully configured.
 *  @retval DA16600_UART_ERROR       Error occured when configuring/opening the UART.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_open (da16600_cfg_t const * const p_cfg)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
    uart_instance_t         * p_uart = NULL;
    da16600_err_t             err = DA16600_UART_ERROR;

    static sci_uart_extended_cfg_t   uart0_cfg_extended_115200;
    static uart_cfg_t                uart0_cfg_115200;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_cfg);
    FSP_ERROR_RETURN(DA16600_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif

    /* Clear the control structure*/
    memset(p_instance_ctrl, 0, sizeof(da16600_instance_ctrl_t));

    /* Initialize queue containing (pointers to) buffers used to store received messages */
    rm_fifo_init(&p_instance_ctrl->buf_queue, p_instance_ctrl->queue_buf, DA16600_CFG_RX_BUF_POOL_LEN);
    for (int i = 0; i < DA16600_CFG_RX_BUF_POOL_LEN; i++)
    {
        /* Add (pointer to) buffer to queue */
        uint8_t * p_buf = &p_instance_ctrl->buf_pool[i][0];
        bool added = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_buf);
        FSP_ASSERT(true == added);
    }

    /* Initialize queue used to hold command responses */
    rm_fifo_init(&p_instance_ctrl->rsp_queue, p_instance_ctrl->rsp_buf, DA16600_CFG_RX_BUF_POOL_LEN);

    /* Update control structure from configuration values */
    p_instance_ctrl->p_da16600_cfg   = p_cfg;
    p_instance_ctrl->uart_instance   = (uart_instance_t *) p_cfg->uart_instance;
    p_instance_ctrl->ioport_instance = (ioport_instance_t *) p_cfg->ioport_instance;
    p_instance_ctrl->reset_pin       = p_cfg->reset_pin;
    p_instance_ctrl->p_callback      = p_cfg->p_callback;

    /* Enable timer used for generic timing routines */
    rm_time_init();

    /* Reset receive character state machine */
    g_rx_state = DA16600_RX_STATE_IDLE;
    g_da16600_cmd_in_progress[0] = '\0';

    /* Create memory copy of uart extended configuration and then copy new configuration values in. */
    memcpy((void *) &uart0_cfg_extended_115200,
           (void *) p_instance_ctrl->uart_instance->p_cfg->p_extend,
           sizeof(sci_uart_extended_cfg_t));

    /* Create memory copy of uart configuration and update with new extended configuration structure. */
    memcpy((void *) &uart0_cfg_115200, p_instance_ctrl->uart_instance->p_cfg, sizeof(uart_cfg_t));

    R_SCI_UART_BaudCalculate(DA16600_DEFAULT_BAUDRATE,
                             DA16600_DEFAULT_MODULATION,
                             DA16600_DEFAULT_ERROR,
                             &g_baud_setting_115200);

    uart0_cfg_extended_115200.p_baud_setting   = &g_baud_setting_115200;
    uart0_cfg_extended_115200.flow_control     = SCI_UART_FLOW_CONTROL_RTS;
    uart0_cfg_extended_115200.flow_control_pin =
        (bsp_io_port_pin_t) DA16600_BSP_PIN_PORT_INVALID;

    uart0_cfg_115200.p_extend = (void *) &uart0_cfg_extended_115200;

    /* Open uart with module default values for baud. 115200 and no hardware flow control. */
    p_uart  = p_instance_ctrl->uart_instance;
    fsp_err_t fsp_err = p_uart->p_api->open(p_uart->p_ctrl, &uart0_cfg_115200);
    FSP_ASSERT(fsp_err == FSP_SUCCESS);

    /* Simple ring/circular buffer is used to receive characters via the UART */
    rm_circ_buf_init(&rx_circ_buf, &uart_rx_buffer[0], sizeof(uart_rx_buffer));

#if (DA16600_DEBUG_STATS_ENABLED == 1)
    g_rx_msg_count = 0;
    g_tx_msg_count = 0;
#endif

    /* Put module into a known state */
    rm_da16600_hw_reset();

    /* Test basic communications with an AT command, module takes time to reboot so try this a few times... */
    uint8_t retries = 0;
    do
    {
        err = rm_da16600_send_write_cmd(p_instance_ctrl, (uint8_t *)"AT\r\n", DA16600_TIMEOUT_500MS);
    }
    while ((retries++ < 10) && (err != DA16600_SUCCESS));

    if (DA16600_SUCCESS == err)
    {
        p_instance_ctrl->open = DA16600_OPEN;
    }

    return err;
}

/**********************************************************************************************************************
 *  Called periodically by application to process any characters received from the DA16600 module.
 **********************************************************************************************************************/
void rm_da16600_task(void)
{
    uint8_t ch;
    if (true == rm_circ_buf_get(&rx_circ_buf, &ch))
    {
        rm_da16600_process_rxd_char(ch);
    }
}

/**********************************************************************************************************************
 *  Perform a hardware reset of the DA16600 module using a GPIO.
 *
 *  @param[in]  p_instance_ctrl       DA16600 instance.
 **********************************************************************************************************************/
void rm_da16600_hw_reset (void)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

    /* Reset the wifi module. */
    p_instance_ctrl->ioport_instance->p_api->pinWrite(p_instance_ctrl->ioport_instance->p_ctrl,
                                                      p_instance_ctrl->reset_pin, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
    p_instance_ctrl->ioport_instance->p_api->pinWrite(p_instance_ctrl->ioport_instance->p_ctrl,
                                                      p_instance_ctrl->reset_pin, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
}


/**********************************************************************************************************************
 *  Send a TCP packet to a client.
 *
 *  @retval DA16600_SUCCESS          Packet transmitted successfully.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_tcp_send (uint8_t * p_ipaddr, uint16_t port, uint8_t * p_data, uint32_t len)
{
    da16600_err_t err;
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
    uint8_t tx_buff[DA16600_CFG_TCP_TX_PKT_LEN_MAX];

    uint32_t hdr_len = (uint32_t)sprintf((char *)tx_buff, "\eS0%d,%d.%d.%d.%d,%d,",
                       (int)len, p_ipaddr[0], p_ipaddr[1], p_ipaddr[2], p_ipaddr[3], port);
    if ((hdr_len + len) <= sizeof(tx_buff))
    {
        memcpy(&tx_buff[hdr_len], p_data, len);

        /* Command needs to be NULL terminated */
        tx_buff[hdr_len + len] = '\0';

        err = rm_da16600_send_write_cmd(p_instance_ctrl, tx_buff, DA16600_TIMEOUT_500MS);
    }
    else
    {
        err = DA16600_INVALID_PARAM;
    }

    return err;
}

/**********************************************************************************************************************
 *  Send factory reset command to the module.
 *
 *  @retval DA16600_SUCCESS          DA16600 successfully configured.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_factory_reset(void)
{
    da16600_err_t err;
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ERROR_RETURN(DA16600_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    err = rm_da16600_send_write_cmd(p_instance_ctrl, (uint8_t *)"ATF\r\n", DA16600_TIMEOUT_500MS);

    return err;
}

/**********************************************************************************************************************
 *  Send restart command to the module.
 *
 *  @retval DA16600_SUCCESS          DA16600 successfully configured.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_restart(void)
{
    da16600_err_t err;
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

    err = rm_da16600_send_write_cmd(p_instance_ctrl, (uint8_t *)"AT+RESTART\r\n", DA16600_TIMEOUT_500MS);

    return err;
}

/**********************************************************************************************************************
 *  Open a TCP server socket.
 *
 *  @param[in]  port                 Port number to use.
 *
 *  @retval DA16600_SUCCESS          Port opened, otherwise error.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_open_tcp_server_socket (uint16_t port)
{
    da16600_err_t err;
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ERROR_RETURN(DA16600_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    uint8_t cmd_buf[DA16600_COMMAND_LEN_MAX];
    int written = snprintf((char *) cmd_buf, DA16600_COMMAND_LEN_MAX, "AT+TRTS=%d\r\n", port);

    if ((written > 0) && (written < DA16600_COMMAND_LEN_MAX))
    {
        err = rm_da16600_send_write_cmd(p_instance_ctrl, cmd_buf, DA16600_TIMEOUT_500MS);
    }
    else
    {
        err = DA16600_INVALID_PARAM;
    }

    return err;
}

/**********************************************************************************************************************
 *  Read WiFi provisioning information from the module.
 *
 *  @param[in]  parameter name       Parameter description.
 *
 *  @retval DA16600_SUCCESS          DA16600 successfully configured.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_get_prov_info(da16600_prov_info_t * p_prov_info)
{
    da16600_err_t err;
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ERROR_RETURN(DA16600_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    uint8_t * p_rsp_buf;
    err = rm_da16600_send_read_cmd(p_instance_ctrl, (uint8_t *)"AT+WFJAP=?\r\n", DA16600_TIMEOUT_1SEC, &p_rsp_buf);

    if (DA16600_SUCCESS == err)
    {
        err = DA16600_RESPONSE_INVALID;

        /* Extract the first parameter from the response, the SSID */
        char * param;
        param = strtok((char *)p_rsp_buf, "'");
        param = strtok(NULL, "'");
        if (param != NULL)
        {
            if ((strlen(param) + 1) <= sizeof(p_prov_info->ssid))
            {
                memcpy(p_prov_info->ssid, param, strlen(param) + 1);

                /* Extract password */
                param = strtok(NULL, "'");
                param = strtok(NULL, "'");

                if (param != NULL)
                {
                    if ((strlen(param) + 1) <= sizeof(p_prov_info->password))
                    {
                        memcpy(p_prov_info->password, param, strlen(param) + 1);
                        err = DA16600_SUCCESS;
                    }
                }
            }
        }

        /* Free the buffer */
        bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rsp_buf);
        FSP_ASSERT(true == freed);
    }
    return err;
}

/**********************************************************************************************************************
 *  Get WiFi connection status from module.
 *
 *  @param[in]  p_status             DA16600_WIFI_CONN_STATE_DISCONNECTED or DA16600_WIFI_CONN_STATE_CONNECTED
 *
 *  @retval DA16600_SUCCESS          Status read successfully, otherwise error.
 **********************************************************************************************************************/
da16600_err_t rm_da16600_get_wifi_connection_status (da16600_wifi_conn_status_t * p_status)
{
    da16600_err_t err;
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ERROR_RETURN(DA16600_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    uint8_t * p_rsp_buf;
    err = rm_da16600_send_read_cmd(p_instance_ctrl, (uint8_t *)"AT+WFSTA\r\n", DA16600_TIMEOUT_1SEC, &p_rsp_buf);

    if (DA16600_SUCCESS == err)
    {
        int status;
        if (sscanf((const char *) p_rsp_buf, "\r\n+WFSTA:%d", &status) == 1)
        {
            if ((status == DA16600_WIFI_CONN_STATE_DISCONNECTED) ||
                (status == DA16600_WIFI_CONN_STATE_CONNECTED))
            {
                *p_status = (da16600_wifi_conn_status_t)status;
            }
            else
            {
                err = DA16600_RESPONSE_INVALID;
            }
        }
        else
        {
            err = DA16600_RESPONSE_INVALID;
        }

        /* Free the buffer */
        bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rsp_buf);
        FSP_ASSERT(true == freed);
    }
    return err;
}

/**********************************************************************************************************************
 *  Send a read command to the module, expects both an OK/ERROR response and (if OK) that requested data.
 *
 *  @param[in]  parameter name       Parameter description.
 *
 *  @retval DA16600_SUCCESS          DA16600 successfully configured.
 **********************************************************************************************************************/
static da16600_err_t rm_da16600_send_read_cmd(da16600_instance_ctrl_t *  p_instance_ctrl,
                                              const uint8_t           *  p_textstring,
                                              uint32_t                   timeout_ms,
                                              uint8_t                 ** p_rsp_buf)
{
    da16600_err_t err = DA16600_SUCCESS;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_textstring);
#endif

#ifdef DA16600_PRINTF_DEBUG_ENABLED
    APP_PRINT("\r\nTX: %s", p_textstring);
#endif

    /* Transmit command */
    fsp_err_t fsp_err;
    fsp_err = p_instance_ctrl->uart_instance->p_api->write(p_instance_ctrl->uart_instance->p_ctrl,
                                                          (uint8_t *) &p_textstring[0],
                                                          (uint32_t)strlen((char *)p_textstring));
    /* Wait for response */
    if (FSP_SUCCESS == fsp_err)
    {
        /* Store copy of command so it can be matched to response (if required), do not include "AT"
         * as this is not present in the response. */
        memcpy(g_da16600_cmd_in_progress, &p_textstring[2], sizeof(g_da16600_cmd_in_progress));
        /* Remove trailing \r\n or =? as they are not included in the response */
        for (int i = 0; g_da16600_cmd_in_progress[i] != '\0'; i++)
        {
            if (g_da16600_cmd_in_progress[i] == '\r' || g_da16600_cmd_in_progress[i] == '=')
            {
                g_da16600_cmd_in_progress[i] = '\0';
                break;
            }
        }

#if (DA16600_DEBUG_STATS_ENABLED == 1)
        g_tx_msg_count++;
#endif
        bool response_valid = false;
        uint16_t tx_time = rm_time_now_ms();

        uint8_t * p_rsp_buffer = NULL;
        uint8_t * p_cmd_rsp_buf = NULL;
        bool free_buffer = false;

        do
        {
            uint8_t ch;
            if (true == rm_circ_buf_get(&rx_circ_buf, &ch))
            {
                rm_da16600_process_rxd_char(ch);
            }

            if (true == rm_fifo_get(&p_instance_ctrl->rsp_queue, &p_rsp_buffer))
            {
                uint8_t * p_msg;
                /* Strip out leading \n\r, if present */
                if (p_rsp_buffer[0] == '\r' && p_rsp_buffer[1] == '\n')
                {
                    p_msg = &p_rsp_buffer[2];
                }
                else
                {
                    p_msg = p_rsp_buffer;
                }

                /* Is it an "OK" response */
                if (true == rm_da16600_is_str_ok((const char *)p_msg))
                {
                    /* Success */
                    if (true == response_valid)
                    {
                        *p_rsp_buf = p_cmd_rsp_buf;
                        err = DA16600_SUCCESS;
                    }
                    else
                    {
                        err = DA16600_RESPONSE_INVALID;
                    }
                    free_buffer = true;
                    break;
                }
                /* Is it an "ERROR" response */
                else if (true == rm_da16600_is_str_err((const char *)p_msg))
                {
                    int error_code;
                    if (sscanf((const char *) p_msg, "ERROR:%d", &error_code) == 1)
                    {
                        err = DA16600_MODULE_ERRROR + abs(error_code);
                    }
                    else
                    {
                        err = DA16600_RESPONSE_INVALID;
                    }
                    free_buffer = true;
                    break;
                }
                /* Is it the expected response */
                else if (true == rm_da16600_is_str_rsp((const char *)p_msg))
                {
                    /* Received response matching pending request, wait for OK before returning success to user */
                    p_cmd_rsp_buf = p_rsp_buffer;
                    response_valid = true;
                }
                else
                {
                    err = DA16600_RESPONSE_INVALID;
                    free_buffer = true;
                    break;
                }
            }
            else
            {
                if (rm_time_elapsed_since_ms(tx_time) > timeout_ms)
                {
                    err = DA16600_RESPONSE_TIMEOUT;
                    break;
                }
            }
        } while(1);

        if (true == free_buffer)
        {
            /* Free the buffer */
            bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rsp_buffer);
            FSP_ASSERT(true == freed);
        }

        /* Command execution complete */
        g_da16600_cmd_in_progress[0] = '\0';
    }

    return err;
}

/**********************************************************************************************************************
 *  Send a "write" command to the module, only expects an OK or ERROR response.
 *
 *  @param[in]  parameter name       Parameter description.
 *
 *  @retval DA16600_SUCCESS          Write command accepted by module
 **********************************************************************************************************************/
static da16600_err_t rm_da16600_send_write_cmd(da16600_instance_ctrl_t * p_instance_ctrl,
                                               const uint8_t           * p_textstring,
                                               uint32_t                  timeout_ms)
{
    da16600_err_t err = DA16600_UART_ERROR;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_textstring);
#endif

#ifdef DA16600_PRINTF_DEBUG_ENABLED
    APP_PRINT("\r\nTX: %s", p_textstring);
#endif

    /* Transmit command */
    fsp_err_t fsp_err;
    fsp_err = p_instance_ctrl->uart_instance->p_api->write(p_instance_ctrl->uart_instance->p_ctrl,
                                                          &p_textstring[0],
                                                 (uint32_t)strlen((char *)p_textstring));
    /* Wait for response */
    if (FSP_SUCCESS == fsp_err)
    {
        /* Store copy of command so it can be matched to response (if required) */
        memcpy(g_da16600_cmd_in_progress, p_textstring, sizeof(g_da16600_cmd_in_progress));

#if (DA16600_DEBUG_STATS_ENABLED == 1)
        g_tx_msg_count++;
#endif
        uint16_t tx_time = rm_time_now_ms();

        do
        {
            uint8_t ch;
            if (true == rm_circ_buf_get(&rx_circ_buf, &ch))
            {
                rm_da16600_process_rxd_char(ch);
            }

            uint8_t * p_rsp_buffer;
            if (true == rm_fifo_get(&p_instance_ctrl->rsp_queue, &p_rsp_buffer))
            {
                uint8_t * p_msg;
                /* Strip out leading \n\r, if present */
                if (p_rsp_buffer[0] == '\r' && p_rsp_buffer[1] == '\n')
                {
                    p_msg = &p_rsp_buffer[2];
                }
                else
                {
                    p_msg = p_rsp_buffer;
                }

                if (true == rm_da16600_is_str_ok((const char *)p_msg))
                {
                    /* Success */
                    err = DA16600_SUCCESS;
                }
                else if (true == rm_da16600_is_str_err((const char *)p_msg))
                {
                    int error_code;
                    if (sscanf((const char *)p_msg, "ERROR:%d", &error_code) == 1)
                    {
                        err = DA16600_MODULE_ERRROR + abs(error_code);
                    }
                    else
                    {
                        err = DA16600_RESPONSE_INVALID;
                    }
                }
                else
                {
                    err = DA16600_RESPONSE_INVALID;
                }

                /* Free the buffer */
                bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rsp_buffer);
                FSP_ASSERT(true == freed);
                break;
            }
            else
            {
                if (rm_time_elapsed_since_ms(tx_time) > timeout_ms)
                {
                    err = DA16600_RESPONSE_TIMEOUT;
                    break;
                }
            }
        } while(1);

        /* Command execution complete */
        g_da16600_cmd_in_progress[0] = '\0';
    }

    return err;
}

/**********************************************************************************************************************
 *  UART interrupt callback.
 *
 *  @param[in]  p_args               Callback parameters.
 **********************************************************************************************************************/
void rm_da16600_uart_callback(uart_callback_args_t *p_args)
{
    if (UART_EVENT_RX_CHAR == p_args->event)
    {
        (void)rm_circ_buf_put(&rx_circ_buf, (uint8_t)p_args->data);
    }
}

/**********************************************************************************************************************
 *  Handle character received via UART.
 *
 *  @param[in]  ch           Received character.
 **********************************************************************************************************************/
static void rm_da16600_process_rxd_char(uint8_t ch)
{
    static uint16_t  ix;
    static uint8_t * p_rx_buffer;

    switch (g_rx_state)
    {
        case DA16600_RX_STATE_IDLE:
        {
            da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

            if (ch == '\r')
            {
                bool buf_available;
                /* Start of message, get buffer to store it in, if no buffers available then silently discard.. */
                if ((buf_available = rm_fifo_get(&p_instance_ctrl->buf_queue, &p_rx_buffer)) == true)
                {
                    ix = 0;
                    p_rx_buffer[ix++] = ch;
                    g_rx_state = DA16600_RX_STATE_PREFIX;
                }
                FSP_ASSERT(true == buf_available);
            }
            else if (ch == '\n')
            {
                /* Character received out of order, reset state machine */
                FSP_ASSERT(ch != '\n');
                g_rx_state = DA16600_RX_STATE_IDLE;
            }
            else
            {
                bool buf_available;
                /* Start of message that does not contain \n\r prefix, get buffer to store it in, if no buffers available then silently discard.. */
                if ((buf_available = rm_fifo_get(&p_instance_ctrl->buf_queue, &p_rx_buffer)) == true)
                {
                    ix = 0;
                    p_rx_buffer[ix++] = ch;
                    g_rx_state = DA16600_RX_STATE_DATA;
                }
                FSP_ASSERT(true == buf_available);
            }
        }
        break;

        case DA16600_RX_STATE_PREFIX:
        {
            if (ch == '\n')
            {
                p_rx_buffer[ix++] = ch;
                FSP_ASSERT(ix < DA16600_CFG_RX_BUF_LEN);

                if (ix < DA16600_CFG_RX_BUF_LEN)
                {
                    g_rx_state = DA16600_RX_STATE_DATA;
                }
                else
                {
                    /* Buffer overflow */
                    g_rx_state = DA16600_RX_STATE_IDLE;
                }
            }
            else
            {
                /* Character received out of order, free buffer and reset state machine */
                da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
                bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rx_buffer);
                FSP_ASSERT(true == freed);

                g_rx_state = DA16600_RX_STATE_IDLE;
            }
        }
        break;

        case DA16600_RX_STATE_DATA:
        {
            if (ch == '\r')
            {
                p_rx_buffer[ix++] = ch;
                FSP_ASSERT(ix < DA16600_CFG_RX_BUF_LEN);

                if (ix < DA16600_CFG_RX_BUF_LEN)
                {
                    g_rx_state = DA16600_RX_STATE_SUFFIX;
                }
                else
                {
                    /* Buffer overflow */
                    g_rx_state = DA16600_RX_STATE_IDLE;
                }
            }
            else if (ch == '\n')
            {
                /* Character received out of order, free buffer and reset state machine */
                da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
                bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rx_buffer);
                FSP_ASSERT(true == freed);

                g_rx_state = DA16600_RX_STATE_IDLE;
            }
            else
            {
                p_rx_buffer[ix++] = ch;
                FSP_ASSERT(ix < DA16600_CFG_RX_BUF_LEN);

                if (ix >= DA16600_CFG_RX_BUF_LEN)
                {
                    /* Buffer overflow */
                    g_rx_state = DA16600_RX_STATE_IDLE;
                }
            }
        }
        break;

        case DA16600_RX_STATE_SUFFIX:
        {
            if (ch == '\n')
            {
                /* Message reception complete, add NULL so it can be handled as a string. */
                p_rx_buffer[ix++] = ch;
                FSP_ASSERT(ix < DA16600_CFG_RX_BUF_LEN);

                if (ix < DA16600_CFG_RX_BUF_LEN)
                {
                    p_rx_buffer[ix] = '\0';

                    /* Don't pass on messages that are just \r\n */
                    if (ix > 2)
                    {
                        rm_da16600_process_rxd_message(p_rx_buffer);
                    }
                    else
                    {
                        da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
                        bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rx_buffer);
                        FSP_ASSERT(true == freed);
                   }
                }
                else
                {
                    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
                    bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rx_buffer);
                    FSP_ASSERT(true == freed);
                }
                g_rx_state = DA16600_RX_STATE_IDLE;
            }
            else
            {
                /* Character received out of order, free buffer and reset state machine */
                da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;
                bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_rx_buffer);
                FSP_ASSERT(true == freed);

                g_rx_state = DA16600_RX_STATE_IDLE;
            }
        }
        break;

        default:
        {
            /* Unknown state */
            FSP_ASSERT(g_rx_state <= DA16600_RX_STATE_SUFFIX);
        }
        break;
    }
}

/**********************************************************************************************************************
 *  Process a complete message received via the UART.
 *
 *  @param[in]  p_buf       Pointer to buffer containing message.
 **********************************************************************************************************************/
static void rm_da16600_process_rxd_message(uint8_t * p_buf)
{
    bool message_handled = false;

#if (DA16600_DEBUG_STATS_ENABLED == 1)
    g_rx_msg_count++;
#endif

    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

    uint8_t * p_msg;

#if 0
    if ((p_buf != &p_instance_ctrl->buf_pool[0][0]) &&
        (p_buf != &p_instance_ctrl->buf_pool[1][0]) &&
        (p_buf != &p_instance_ctrl->buf_pool[2][0]) &&
        (p_buf != &p_instance_ctrl->buf_pool[3][0]))
    {
        __BKPT(0);
    }
#endif

    /* Strip out leading \n\r, if present */
    if (p_buf[0] == '\r' && p_buf[1] == '\n')
    {
        p_msg = &p_buf[2];
    }
    else
    {
        p_msg = p_buf;
    }

#ifdef DA16600_PRINTF_DEBUG_ENABLED
    APP_PRINT("\r\nRX (%d): %s", (g_da16600_cmd_in_progress[0] != '\0'), p_msg);
#endif

    if (g_da16600_cmd_in_progress[0] != '\0')
    {
        if ((rm_da16600_is_str_ok((const char *)p_msg) == true) ||
            (rm_da16600_is_str_err((const char *)p_msg) == true) ||
            (rm_da16600_is_str_rsp((const char *)p_msg) == true))
        {
            /* Add received message to response queue */
            if (false == rm_fifo_put(&p_instance_ctrl->rsp_queue, &p_buf))
            {
                /* Unable to add to response queue so free the buffer */
                bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_buf);
                FSP_ASSERT(false == freed);
            }
            message_handled = true;
        }
    }
    else if (true == g_rx_data_in_progress)
    {
        rm_da16600_handle_data_chunk(p_buf);
        message_handled = true;

        /* Free the buffer */
        bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_buf);
        FSP_ASSERT(true == freed);
    }

    if (false == message_handled)
    {
        for (uint8_t i = 0; rsp_handlers[i].handler != NULL; i++)
        {
            if (0 == strncmp(rsp_handlers[i].p_rsp, (const char *)p_msg, strlen(rsp_handlers[i].p_rsp)))
            {
              rsp_handlers[i].handler(p_msg);
              break;
            }
        }
        /* Free the buffer */
        bool freed = rm_fifo_put(&p_instance_ctrl->buf_queue, &p_buf);
        FSP_ASSERT(true == freed);
    }
}

/**********************************************************************************************************************
 *  Handle initialization complete message.
 *
 *  @param[in]  p_buf       Pointer to buffer containing message and parameters.
 **********************************************************************************************************************/
static void rm_da16600_handle_init(uint8_t * p_buf)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl->p_callback);
#endif

    int mode;
    if (sscanf((const char *) p_buf, "+INIT:DONE,%d", &mode) == 1)
    {
        g_rm16600_callback_args.event = DA16600_EVENT_INIT;
        g_rm16600_callback_args.param.init.mode = (uint8_t)mode;

        p_instance_ctrl->p_callback(&g_rm16600_callback_args);
    }
}


/**********************************************************************************************************************
 *  Handle joined WiFi access point message.
 *
 *  @param[in]  p_buf       Pointer to buffer containing message and parameters.
 **********************************************************************************************************************/
static void rm_da16600_handle_wfjap(uint8_t * p_buf)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl->p_callback);
#endif

    int result;
    if (sscanf((const char *) p_buf, "+WFJAP:%d", &result) == 1)
    {
        g_rm16600_callback_args.event = DA16600_EVENT_JOINED_ACCESS_POINT,
        g_rm16600_callback_args.param.joined_ap.result = (uint8_t)result;

        if (result == 1)
        {
            /* IP Address follows second comma (+WFJAP:1,'<SSID>',<IP Address>)*/
            char *ip_addr = rm_da16600_findnchr((const char *)p_buf, ',', 2);
            if (ip_addr)
            {
                int ip_add_0, ip_add_1, ip_add_2, ip_add_3;
                if (sscanf((const char *) ip_addr, ",%d.%d.%d.%d",
                           &ip_add_0, &ip_add_1, &ip_add_2, &ip_add_3) == 4)
                {
                    g_rm16600_callback_args.param.joined_ap.ip_addr[0] = (uint8_t)ip_add_0;
                    g_rm16600_callback_args.param.joined_ap.ip_addr[1] = (uint8_t)ip_add_1;
                    g_rm16600_callback_args.param.joined_ap.ip_addr[2] = (uint8_t)ip_add_2;
                    g_rm16600_callback_args.param.joined_ap.ip_addr[3] = (uint8_t)ip_add_3;
                }
            }
        }
        p_instance_ctrl->p_callback(&g_rm16600_callback_args);
    }
}


/**********************************************************************************************************************
 *  Handle WiFi disconnected message.
 *
 *  @param[in]  p_buf       Pointer to buffer containing message and parameters.
 **********************************************************************************************************************/
static void rm_da16600_handle_wfdap(uint8_t * p_buf)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl->p_callback);
#endif

    if (0 == strncmp((const char *) p_buf,
                     (const char *) "+WFDAP:0",
              strlen((const char *) "+WFDAP:0")))
    {
        g_rm16600_callback_args.event = DA16600_EVENT_WIFI_DISCONNECTED;

        p_instance_ctrl->p_callback(&g_rm16600_callback_args);
    }
}


/**********************************************************************************************************************
 *  Handle TCP client connected message.
 *
 *  @param[in]  p_buf       Pointer to buffer containing message and parameters.
 **********************************************************************************************************************/
static void rm_da16600_handle_trcts(uint8_t * p_buf)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl->p_callback);
#endif

    int ip_add_0, ip_add_1, ip_add_2, ip_add_3;
    int port;
    if (sscanf((const char *) p_buf, "+TRCTS:0,%d.%d.%d.%d,%d", &ip_add_0, &ip_add_1, &ip_add_2, &ip_add_3, &port) == 5)
    {
        g_rm16600_callback_args.event = DA16600_EVENT_TCP_CLIENT_CONNECTED;
        g_rm16600_callback_args.param.tcp_client_connected.ip_addr[0] = (uint8_t)ip_add_0;
        g_rm16600_callback_args.param.tcp_client_connected.ip_addr[1] = (uint8_t)ip_add_1;
        g_rm16600_callback_args.param.tcp_client_connected.ip_addr[2] = (uint8_t)ip_add_2;
        g_rm16600_callback_args.param.tcp_client_connected.ip_addr[3] = (uint8_t)ip_add_3;
        g_rm16600_callback_args.param.tcp_client_connected.port = (uint16_t)port;

        p_instance_ctrl->p_callback(&g_rm16600_callback_args);
    }
}


/**********************************************************************************************************************
 *  Handle TCP client disconnected message.
 *
 *  @param[in]  p_buf       Pointer to buffer containing message parameters.
 **********************************************************************************************************************/
static void rm_da16600_handle_trxts(uint8_t * p_buf)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl->p_callback);
#endif

    int ip_add_0, ip_add_1, ip_add_2, ip_add_3;
    int port;
    if (sscanf((const char *) p_buf, "+TRXTS:0,%d.%d.%d.%d,%d", &ip_add_0, &ip_add_1, &ip_add_2, &ip_add_3, &port) == 5)
    {
        g_rm16600_callback_args.event = DA16600_EVENT_TCP_CLIENT_DISCONNECTED;
        g_rm16600_callback_args.param.tcp_client_disconnected.ip_addr[0] = (uint8_t)ip_add_0;
        g_rm16600_callback_args.param.tcp_client_disconnected.ip_addr[1] = (uint8_t)ip_add_1;
        g_rm16600_callback_args.param.tcp_client_disconnected.ip_addr[2] = (uint8_t)ip_add_2;
        g_rm16600_callback_args.param.tcp_client_disconnected.ip_addr[3] = (uint8_t)ip_add_3;
        g_rm16600_callback_args.param.tcp_client_disconnected.port = (uint16_t)port;

        p_instance_ctrl->p_callback(&g_rm16600_callback_args);
    }
}


/**********************************************************************************************************************
 *  Handle chunk of TCP data received from module.
 *
 *  @param[in]  p_buf       Pointer to buffer containing data.
 **********************************************************************************************************************/
static void rm_da16600_handle_data_chunk(uint8_t * p_buf)
{
    /* Add chunk to buffer */
    if ((g_rx_data_len + strlen((const char *)p_buf)) < DA16600_CFG_RX_BUF_LEN)
    {
        memcpy(&g_rm16600_callback_args.param.tcp_client_received_data.data[g_rx_data_len],
               p_buf,
               strlen((const char *)p_buf));

        g_rx_data_len +=  strlen((const char *)p_buf);

        if (g_rx_data_len >= g_rm16600_callback_args.param.tcp_client_received_data.length)
        {
            da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

            /* NULL terminate so data can be handled by string processing functions */
            g_rm16600_callback_args.param.tcp_client_received_data.data[g_rx_data_len] = '\0';
            p_instance_ctrl->p_callback(&g_rm16600_callback_args);
            g_rx_data_in_progress = false;
        }
    }
    else
    {
        FSP_ASSERT((g_rx_data_len + strlen((const char *)p_buf)) < DA16600_CFG_RX_BUF_LEN);
        /* Too much data for buffer... */
        g_rx_data_in_progress = false;
    }
}


/**********************************************************************************************************************
 *  Handle received TCP data command from module.
 *
 *  @param[in]  p_buf       Pointer to buffer containing received TCP data.
 **********************************************************************************************************************/
static void rm_da16600_handle_trdts(uint8_t * p_buf)
{
    da16600_instance_ctrl_t * p_instance_ctrl = &g_rm_da16600_instance;

#if (DA16600_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl->p_callback);
#endif

    int ip_add_0, ip_add_1, ip_add_2, ip_add_3;
    int port;
    int length;
    if (sscanf((const char *) p_buf, "+TRDTS:0,%d.%d.%d.%d,%d,%d",
               &ip_add_0, &ip_add_1, &ip_add_2, &ip_add_3, &port, &length) == 6)
    {
        if (sizeof(g_rm16600_callback_args.param.tcp_client_received_data.data) >=
                g_rm16600_callback_args.param.tcp_client_received_data.length)
        {
            g_rm16600_callback_args.param.tcp_client_received_data.ip_addr[0] = (uint8_t)ip_add_0;
            g_rm16600_callback_args.param.tcp_client_received_data.ip_addr[1] = (uint8_t)ip_add_1;
            g_rm16600_callback_args.param.tcp_client_received_data.ip_addr[2] = (uint8_t)ip_add_2;
            g_rm16600_callback_args.param.tcp_client_received_data.ip_addr[3] = (uint8_t)ip_add_3;
            g_rm16600_callback_args.param.tcp_client_received_data.port = (uint16_t)port;
            g_rm16600_callback_args.param.tcp_client_received_data.length = (uint32_t)length;

            uint8_t * p_data;

            /* Data starts after the 4th comma */
            p_data = (uint8_t *)rm_da16600_findnchr((char *)p_buf, ',', 4);
            p_data = p_data + 1;

            /* Determine how much data (out of the total) was received */
            g_rx_data_len = (uint32_t)strlen((const char *)p_buf) - ((uint32_t)(p_data - p_buf));

            memcpy(g_rm16600_callback_args.param.tcp_client_received_data.data, p_data, g_rx_data_len);

            g_rm16600_callback_args.event = DA16600_EVENT_TCP_CLIENT_DATA_RECEIVED;

            if (g_rx_data_len >= g_rm16600_callback_args.param.tcp_client_received_data.length)
            {
                /* NULL terminate so data can be handled by string processing functions */
                g_rm16600_callback_args.param.tcp_client_received_data.data[g_rx_data_len] = '\0';
                p_instance_ctrl->p_callback(&g_rm16600_callback_args);
                g_rx_data_in_progress = false;
            }
            else
            {
                /* More data to be received... */
                g_rx_data_in_progress = true;
            }
        }
        else
        {
            FSP_ASSERT(sizeof(g_rm16600_callback_args.param.tcp_client_received_data.data) >=
                       g_rm16600_callback_args.param.tcp_client_received_data.length);
        }
    }
}


/**********************************************************************************************************************
 *  Compare string (s) to ok response.
 *
 *  @param[in]  s     String containing response received.
 *
 *  @retval true if string matches ok response, otherwise false.
 **********************************************************************************************************************/
static bool rm_da16600_is_str_ok(const char * s)
{
    bool ret = false;
    if (0 == strncmp((const char *) s,
                     (const char *) g_da16600_return_text_ok,
              strlen((const char *) g_da16600_return_text_ok)))
    {
        ret = true;
    }
    return ret;
}

/**********************************************************************************************************************
 *  Compare string (s) to error response.
 *
 *  @param[in]  s     String containing response received.
 *
 *  @retval true if string matches error response, otherwise false.
 **********************************************************************************************************************/
static bool rm_da16600_is_str_err(const char * s)
{
    bool ret = false;
    if (0 == strncmp((const char *) s,
                     (const char *) g_da16600_return_text_error,
              strlen((const char *) g_da16600_return_text_error)))
    {
        ret = true;
    }
    return ret;
}

/**********************************************************************************************************************
 *  Compare string (s) to the expected response (g_da16600_cmd_in_progress).
 *
 *  @param[in]  s     String containing response received.
 *
 *  @retval true if string matches expected response, otherwise false.
 **********************************************************************************************************************/
static bool rm_da16600_is_str_rsp(const char * s)
{
    bool ret = false;
    if (0 == strncmp((const char *) s,
                     (const char *) g_da16600_cmd_in_progress,
              strlen((const char *) g_da16600_cmd_in_progress)))
    {
        ret = true;
    }
    return ret;
}

/**********************************************************************************************************************
 *  Returns the position of the nth occurrence (occ) of a character (chr) in a string (str).
 *
 *  @param[in]  str       String to search.
 *  @param[in]  chr       Character to search for.
 *  @param[in]  occ       nth occurrence of character.
 *
 *  @retval Pointer to position of nth occurrence of character or NULL if not found.
 **********************************************************************************************************************/
static char * rm_da16600_findnchr(const char *str, char chr, int occ)
{
    int matches = 0;
    char *location = NULL;

    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == chr)
        {
            if (++matches == occ)
            {
                location = (char *)&str[i];
                break;
            }
        }
    }
    return location;
}
