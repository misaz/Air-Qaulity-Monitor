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

/*******************************************************************************************************************//**
 * @addtogroup DA16600 DA16600
 * @{
 **********************************************************************************************************************/

#ifndef RM_DA16600_H
#define RM_DA16600_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "r_uart_api.h"
#include "r_ioport_api.h"

#include "rm_fifo.h"
#include "rm_da16600_cfg.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#ifndef DA16600_CFG_RX_BUF_LEN
 #define DA16600_CFG_RX_BUF_LEN           (1024)
#endif

#ifndef DA16600_CFG_RX_BUF_POOL_LEN
 #define DA16600_CFG_RX_BUF_POOL_LEN      (4)
#endif

#ifndef DA16600_CFG_TCP_TX_PKT_LEN_MAX
  #define DA16600_CFG_TCP_TX_PKT_LEN_MAX  (2048)
#endif

#ifndef DA16600_CFG_MAX_SSID_SIZE
  #define DA16600_CFG_MAX_SSID_SIZE       (32)
#endif

#ifndef DA16600_CFG_MAX_PASSWORD_SIZE
  #define DA16600_CFG_MAX_PASSWORD_SIZE   (32)
#endif

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

typedef enum
{
    DA16600_WIFI_CONN_STATE_DISCONNECTED = 0,
    DA16600_WIFI_CONN_STATE_CONNECTED,
} da16600_wifi_conn_status_t;

/** Error codes */
typedef enum e_da16600_err
{
    DA16600_SUCCESS          = 0,
    DA16600_BUSY             = 0x50000, ///< Operation currently in progress
    DA16600_QUEUE_FULL       = 0x50001, ///< Attempt to add item to a queue failed
    DA16600_QUEUE_EMPTY      = 0x50002, ///< Attempt to get item from a queue failed
    DA16600_RESPONSE_INVALID = 0x50003, ///< Command generated an unknown response
    DA16600_RESPONSE_TIMEOUT = 0x50004, ///< Command response not received within expected time
    DA16600_INVALID_PARAM    = 0x50005, ///< Invalid parameter passed to function
    DA16600_UART_ERROR       = 0x50006, ///< UART operation failed
    DA16600_MODULE_ERRROR    = 0x50010, ///< Error from module, received error code is added to this value
} da16600_err_t;

typedef enum e_da16600_event
{
    DA16600_EVENT_NONE = 0,
    DA16600_EVENT_INIT,
    DA16600_EVENT_JOINED_ACCESS_POINT,
    DA16600_EVENT_WIFI_DISCONNECTED,
    DA16600_EVENT_TCP_CLIENT_CONNECTED,
    DA16600_EVENT_TCP_CLIENT_DISCONNECTED,
    DA16600_EVENT_TCP_CLIENT_DATA_RECEIVED,
} da16600_event_t;

typedef struct
{
    uint8_t ssid[DA16600_CFG_MAX_SSID_SIZE];
    uint8_t sec_prot;
    uint8_t enc;
    uint8_t password[DA16600_CFG_MAX_PASSWORD_SIZE];
} da16600_prov_info_t;

typedef struct
{
    uint8_t mode;
} da16600_init_param_t;

typedef struct
{
    uint8_t result;
    uint8_t ssid[DA16600_CFG_MAX_SSID_SIZE];
    uint8_t ip_addr[4];
} da16600_joined_ap_param_t;

typedef struct
{
    uint8_t  ip_addr[4];
    uint16_t port;
    uint32_t length;
    uint8_t  data[DA16600_CFG_RX_BUF_LEN];
} da16600_tcp_client_received_data_param_t;

typedef struct
{
    uint8_t  ip_addr[4];
    uint16_t port;
} da16600_tcp_client_connected_param_t;

typedef struct
{
    uint8_t  ip_addr[4];
    uint16_t port;
} da16600_tcp_client_disconnected_param_t;

typedef union
{
    da16600_init_param_t                     init;
    da16600_joined_ap_param_t                joined_ap;
    da16600_tcp_client_connected_param_t     tcp_client_connected;
    da16600_tcp_client_disconnected_param_t  tcp_client_disconnected;
    da16600_tcp_client_received_data_param_t tcp_client_received_data;
} da16600_event_param_t;

/** DA16600 callback parameter definition */
typedef struct st_da16600_callback_args
{
    da16600_event_t       event;
    da16600_event_param_t param;
} da16600_callback_args_t;

/** User configuration structure, used in open function */
typedef struct st_da16600_cfg
{
    const bsp_io_port_pin_t   reset_pin;           ///< Reset pin used for module
    const uart_instance_t   * uart_instance;       ///< SCI UART instance
    const ioport_instance_t * ioport_instance;     ///< IO Port Instance
    void                   (* p_callback)(da16600_callback_args_t * p_args);
    void const              * p_context;           ///< User defined context passed into callback function.
    void const              * p_extend;            ///< Pointer to extended configuration by instance of interface.
} da16600_cfg_t;

/** DA16600 private control block. DO NOT MODIFY. */
typedef struct st_da16600_instance_ctrl
{
    uint32_t              open;                                                     ///< Flag to indicate if DA16600 instance has been initialized
    da16600_cfg_t const * p_da16600_cfg;                                            ///< Pointer to initial configurations.
    bsp_io_port_pin_t     reset_pin;                                                ///< DA16600 module reset pin
    uart_instance_t     * uart_instance;                                            ///< UART instance object
    ioport_instance_t   * ioport_instance;                                          ///< IO Port Instance
    uint8_t               buf_pool[DA16600_CFG_RX_BUF_POOL_LEN][DA16600_CFG_RX_BUF_LEN];
    uint8_t             * queue_buf[DA16600_CFG_RX_BUF_POOL_LEN];
    rm_fifo_t             buf_queue;
    uint8_t             * rsp_buf[DA16600_CFG_RX_BUF_POOL_LEN];
    rm_fifo_t             rsp_queue;
    void               (* p_callback)(da16600_callback_args_t * p_args);
} da16600_instance_ctrl_t;

typedef void (* da16600_callback_fn_t)(void);

/*******************************************************************************************************************//**
 * @} (end addtogroup DA16600)
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/
da16600_err_t rm_da16600_open (da16600_cfg_t const * const p_cfg);
void rm_da16600_task(void);

/* Basic commands */
da16600_err_t rm_da16600_factory_reset (void);
da16600_err_t rm_da16600_restart (void);
void rm_da16600_hw_reset (void);

/* Network commands */

/* WiFi commands */
da16600_err_t rm_da16600_get_wifi_connection_status (da16600_wifi_conn_status_t * p_status);
da16600_err_t rm_da16600_get_prov_info(da16600_prov_info_t * p_prov_info);

/* Socket commands */
da16600_err_t rm_da16600_open_tcp_server_socket (uint16_t port);

/* Data transfer commands */
da16600_err_t rm_da16600_tcp_send (uint8_t * p_ipaddr, uint16_t port, uint8_t * p_data, uint32_t len);

/* Peripheral interrupt handlers */
void rm_da16600_uart_callback (uart_callback_args_t *p_args);

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER
#endif                                 // RM_DA16600_H

/*******************************************************************************************************************//**
 * @} (end addtogroup DA16600)
 **********************************************************************************************************************/
