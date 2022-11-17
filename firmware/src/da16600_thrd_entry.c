#include "da16600_thrd.h"
#include "rm_da16600.h"
#include "air_quality_data_publish_interface.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include "bsp_api.h"
#include <stdio.h>
#include "fsp_common_api.h"

#define EVENT_QUEUE_POOL_LEN   3

typedef enum e_state {
	STATE_IDLE = 0, STATE_RESTARTING, STATE_PROVISIONING, STATE_JOINING, STATE_JOINED, STATE_POWER_OFF
} state_t;

typedef enum {
	EVENT_NONE = 0, EVENT_DA16600, EVENT_POLL, EVENT_ZMOD4510_DATA
} event_id_t;

typedef union {
	da16600_callback_args_t da16600;
	zmod4510_air_quality_data zmod4510;
} event_param_t;

typedef struct {
	event_id_t id;
	event_param_t param;
} event_t;

static uint8_t g_event_buf_pool[EVENT_QUEUE_POOL_LEN][sizeof(event_t)];
static uint8_t* g_free_queue_buf[EVENT_QUEUE_POOL_LEN];
static rm_fifo_t g_free_queue;
static uint8_t* g_used_queue_buf[EVENT_QUEUE_POOL_LEN];
static rm_fifo_t g_used_queue;
static state_t g_state;

static int areQueuesReady = 0;

#define DATA_TO_SEND_CAPACITY 130
#define DATA_FLUSH_THRESHOLD 59
static zmod4510_air_quality_data waiting_data[DATA_TO_SEND_CAPACITY];
static int waiting_data_wr_ptr = 0;
static int waiting_data_rd_ptr = 0;

static int is_time_synced = 0;

static void da16600_callback(da16600_callback_args_t* p_args) {
	/* Silently discard if queue is full... */
	bool added;
	event_t* p_event;
	if (true == (added = rm_fifo_get(&g_free_queue, (uint8_t**)&p_event))) {
		p_event->id = EVENT_DA16600;
		memcpy(&p_event->param.da16600, p_args, sizeof(da16600_callback_args_t));

		if (false == (added = rm_fifo_put(&g_used_queue, (uint8_t**)&p_event))) {
			/* Free buffer as can't be added to the used queue... */
			bool freed = rm_fifo_put(&g_free_queue, (uint8_t**)&p_event);
			assert(true == freed);
		}
		assert(true == added);
	}
	assert(true == added);
}

static void add_timestamp_to_data(zmod4510_air_quality_data* data) {
	rtc_time_t time;
	fsp_err_t err;

	err = R_RTC_CalendarTimeGet(&g_rtc0_ctrl, &time);
	if (err)
		__BKPT();

	data->year = time.tm_year + 1900;
	data->month = (uint8_t)time.tm_mon;
	data->date = (uint8_t)time.tm_mday;
	data->hour = (uint8_t)time.tm_hour;
	data->minute = (uint8_t)time.tm_min;
	data->second = (uint8_t)time.tm_sec;
}

int publish_air_quality(zmod4510_air_quality_data* data) {
	if (!areQueuesReady) {
		return 1;
	}

	xSemaphoreTake(g_waiting_data_mutex, portMAX_DELAY);

	waiting_data[waiting_data_wr_ptr].airQuality = data->airQuality;
	waiting_data[waiting_data_wr_ptr].fastAirQuality = data->fastAirQuality;
	waiting_data[waiting_data_wr_ptr].ozone = data->ozone;
	add_timestamp_to_data(waiting_data + waiting_data_wr_ptr);
	waiting_data_wr_ptr++;
	if (waiting_data_wr_ptr == DATA_TO_SEND_CAPACITY) {
		waiting_data_wr_ptr = 0;
	}

	xSemaphoreGive(g_waiting_data_mutex);

	return 0;
}

static void wifi_power_off() {
	/*
	// set all PMOD pins to Hi-Z
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_00, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_01, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_02, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_03, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_02, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_08, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_05, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_06, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);

	// turn of power transistor
	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D4, 0);
	*/

	// changed to alternative strategy. Hold module in RESET
	R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_08, 0);

	SEGGER_RTT_printf(0, "\r\nWifi module is now powered off.");

	is_time_synced = 0;
}

extern da16600_instance_ctrl_t g_rm_da16600_instance;

static int wifi_power_on() {
	da16600_err_t err;

	/*
	// turn on power transistor
	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D4, 1);

	// restore IO configuration
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_00, ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI0_2_4_6_8));
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_01, ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI0_2_4_6_8));
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_02, ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI0_2_4_6_8));
	//R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_03, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_02, ((uint32_t)IOPORT_CFG_IRQ_ENABLE | (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT | (uint32_t)IOPORT_CFG_PULLUP_ENABLE));
	//R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_08, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	//R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_05, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	//R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_06, (uint32_t)IOPORT_CFG_PORT_DIRECTION_INPUT);
	 */

	// changed to alternative strategy. Module is held in RESET state. Now deassert reset.
	R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_08, 1);

	SEGGER_RTT_printf(0, "\r\nWifi module is now powered on. Testing connectivity.");

	/* Put module into a known state */
	rm_da16600_hw_reset();

	/* Test basic communications with an AT command, module takes time to reboot so try this a few times... */
	uint8_t retries = 0;
	do {
		err = rm_da16600_send_write_cmd(&g_rm_da16600_instance, (uint8_t*)"AT\r\n", 500);
	} while ((retries++ < 100) && (err != DA16600_SUCCESS));

	if (retries == 100) {
		return 1;
	}

	SEGGER_RTT_printf(0, "\r\nWifi module is now active.");

	return 0;
}

static void format_and_publish_data(zmod4510_air_quality_data* publish_request) {
	char* url = "http://misaz.cz:5000/api/publish_air_quality";
	uint8_t message[48];
	da16600_err_t err;

	bsp_unique_id_t const* bsp_id = R_BSP_UniqueIdGet();
	memcpy(message, bsp_id, sizeof(bsp_unique_id_t));

	*((uint16_t*)(message + 32)) = (uint16_t)publish_request->year;
	*((uint8_t*)(message + 34)) = (uint8_t)publish_request->month;
	*((uint8_t*)(message + 35)) = (uint8_t)publish_request->date;
	*((uint8_t*)(message + 36)) = (uint8_t)publish_request->hour;
	*((uint8_t*)(message + 37)) = (uint8_t)publish_request->minute;
	*((uint8_t*)(message + 38)) = (uint8_t)publish_request->second;
	*((uint8_t*)(message + 39)) = 0;
	*((float*)(message + 40)) = publish_request->ozone;
	*((uint8_t*)(message + 44)) = (uint8_t)publish_request->airQuality;
	*((uint8_t*)(message + 45)) = (uint8_t)publish_request->fastAirQuality;
	*((uint8_t*)(message + 46)) = 0;
	*((uint8_t*)(message + 47)) = 0;

	char base64Encoded[65];
	char* base64EncodedWr = base64Encoded;

	for (int i = 0; i < 16; i++) {
		uint32_t byte1 = *(message + i * 3);
		uint32_t byte2 = *(message + i * 3 + 1);
		uint32_t byte3 = *(message + i * 3 + 2);
		uint32_t item = (byte1 << 16) | (byte2 << 8) | byte3;

		for (int j = 0; j < 4; j++) {
			uint8_t source = (uint8_t)((item & 0xFC0000) >> 18);
			item = (item << 6) & 0x00FFFFFF;
			if (source < 26) {
				*base64EncodedWr++ = (char)('A' + source);
			} else if (source < 52) {
				*base64EncodedWr++ = (char)('a' + (source - 26));
			} else if (source < 62) {
				*base64EncodedWr++ = (char)('0' + (source - 52));
			} else if (source == 62) {
				*base64EncodedWr++ = '+';
			} else {
				*base64EncodedWr++ = '/';
			}
		}
	}
	*base64EncodedWr++ = '\0';

	SEGGER_RTT_printf(0, "\r\nPublishing data: %s.", base64Encoded);
	err = rm_da16600_http_client_post(url, base64Encoded);
	SEGGER_RTT_printf(0, "\r\nPublishing data status code: %d", err);
}

static void send_data() {
	int isSendNeeded;

	SEGGER_RTT_printf(0, "\r\nStarting sending all data to cloud.");

	xSemaphoreTake(g_waiting_data_mutex, portMAX_DELAY);
	isSendNeeded = waiting_data_wr_ptr != waiting_data_rd_ptr;
	xSemaphoreGive(g_waiting_data_mutex);

	while (isSendNeeded) {
		format_and_publish_data(waiting_data + waiting_data_rd_ptr);

		xSemaphoreTake(g_waiting_data_mutex, portMAX_DELAY);
		waiting_data_rd_ptr++;
		if (waiting_data_rd_ptr == DATA_TO_SEND_CAPACITY) {
			waiting_data_rd_ptr = 0;
		}
		isSendNeeded = waiting_data_wr_ptr != waiting_data_rd_ptr;
		xSemaphoreGive(g_waiting_data_mutex);
	}

	SEGGER_RTT_printf(0, "\r\nAll data sent to cloud.");
}

static int get_number_of_data_to_send() {
	int wrBackup, rdBackup;

	xSemaphoreTake(g_waiting_data_mutex, portMAX_DELAY);
	wrBackup = waiting_data_wr_ptr;
	rdBackup = waiting_data_rd_ptr;
	xSemaphoreGive(g_waiting_data_mutex);

	int count = 0;

	while (rdBackup != wrBackup) {
		count++;
		rdBackup++;
		if (rdBackup == DATA_TO_SEND_CAPACITY) {
			rdBackup = 0;
		}
	}

	return count;
}

static void sync_time() {
	char time[48];
	da16600_err_t err;
	fsp_err_t fErr;

	SEGGER_RTT_printf(0, "\r\nSyncing time.");

	int year, month, date, hour, minute, second;
	year = 1970;

	do {
		err = rm_da16600_get_time(time, sizeof(time));
		if (err) {
			SEGGER_RTT_printf(0, "\r\nUnable to get time.");
			return;
		}

		if (sscanf(time, "\r\n+TIME:%d-%d-%d,%d:%d:%d", &year, &month, &date, &hour, &minute, &second) != 6) {
			SEGGER_RTT_printf(0, "\r\nUnable to parse time");
			return;
		}

		if (year == 1970) {
			vTaskDelay(10);
		}
	} while (year == 1970);

	rtc_time_t rtcTime;
	rtcTime.tm_year = (year - 1900);
	rtcTime.tm_mon = month;
	rtcTime.tm_mday = date;
	rtcTime.tm_wday = 1;
	rtcTime.tm_hour = hour;
	rtcTime.tm_min = minute;
	rtcTime.tm_sec = second;
	rtcTime.tm_yday = 1;
	rtcTime.tm_isdst = 0;

	fErr = R_RTC_CalendarTimeSet(&g_rtc0_ctrl, &rtcTime);
	if (fErr)
		__BKPT();

	SEGGER_RTT_printf(0, "\r\nTime synced. New time: %d-%02d-%02d %02d:%02d:%02d", year, month, date, hour, minute, second);

	is_time_synced = 1;
}

const da16600_cfg_t g_da16600_cfg = { .uart_instance = &g_wifi_uart, .ioport_instance = &g_ioport, .reset_pin = BSP_IO_PORT_02_PIN_08, .p_callback = da16600_callback };

TickType_t restartingStart;

static void process_event(event_t* p_event) {
	da16600_err_t err;

	if (p_event->id != EVENT_POLL) {
		SEGGER_RTT_printf(0, "\r\n%s - id: %d", __FUNCTION__, p_event->id);
		if (EVENT_DA16600 == p_event->id) {
			SEGGER_RTT_printf(0, "\r\nDA16600_EVENT: %d", p_event->param.da16600.event);
			if (DA16600_EVENT_JOINED_ACCESS_POINT == p_event->param.da16600.event) {
				SEGGER_RTT_printf(0, "\r\nDA16600_EVENT_JOINED_ACCESS_POINT - Result: %d IP: %d.%d.%d.%d", p_event->param.da16600.param.joined_ap.result,
						p_event->param.da16600.param.joined_ap.ip_addr[0], p_event->param.da16600.param.joined_ap.ip_addr[1], p_event->param.da16600.param.joined_ap.ip_addr[2],
						p_event->param.da16600.param.joined_ap.ip_addr[3]);
			}
		}
	}

	switch (g_state) {
		case STATE_IDLE: {
			if (EVENT_POLL == p_event->id) {
				err = rm_da16600_restart();
				SEGGER_RTT_printf(0, "\r\nRestarting DA16600: %d", err);

				if (DA16600_SUCCESS == err) {
					restartingStart = xTaskGetTickCount();
					g_state = STATE_RESTARTING;
				}
			}
		}
			break;

		case STATE_RESTARTING: {
			if (EVENT_DA16600 == p_event->id) {
				if (DA16600_EVENT_INIT == p_event->param.da16600.event) {
					da16600_prov_info_t prov_info;
					if (DA16600_SUCCESS == rm_da16600_get_prov_info(&prov_info)) {
						SEGGER_RTT_printf(0, "\r\nRestarting complete, joining WiFi network...");
						SEGGER_RTT_printf(0, "\r\nSSID: %s", prov_info.ssid);

						/* Device restart complete, already provisioned to network */
						g_state = STATE_JOINING;
					} else {
						SEGGER_RTT_printf(0, "\r\nRestarting complete, provisioning...");

						/* No provisioning info present, indicates device has not been provisioned */
						g_state = STATE_PROVISIONING;
					}
				}
			} else if (p_event->id == EVENT_POLL) {
				if (xTaskGetTickCount() - restartingStart > 10000) {
					rm_da16600_restart();
				}
			}
		}
			break;

		case STATE_PROVISIONING: {
			if (EVENT_DA16600 == p_event->id) {
				/* DA16600 will restart when provisioning is complete... */
				if (DA16600_EVENT_INIT == p_event->param.da16600.event) {
					g_state = STATE_JOINING;
				}
			}
		}
			break;

		case STATE_JOINING: {
			if (EVENT_POLL == p_event->id) {
				da16600_wifi_conn_status_t conn_status;
				if (DA16600_SUCCESS == rm_da16600_get_wifi_connection_status(&conn_status)) {
					if (DA16600_WIFI_CONN_STATE_CONNECTED == conn_status) {
						g_state = STATE_JOINED;
					}
				}
			}
		}
			break;

		case STATE_JOINED: {
			if (EVENT_DA16600 == p_event->id) {
				if (p_event->param.da16600.event == DA16600_EVENT_WIFI_DISCONNECTED) {
					SEGGER_RTT_printf(0, "\r\nDisconnected from WiFi network!");
					g_state = STATE_JOINING;
				}
			} else if (EVENT_POLL == p_event->id) {
				if (is_time_synced) {
					if (get_number_of_data_to_send() > DATA_FLUSH_THRESHOLD) {
						send_data();
					} else {
						wifi_power_off();
						g_state = STATE_POWER_OFF;
					}
				} else {
					sync_time();
				}
			}
		}
			break;

		case STATE_POWER_OFF: {
			if (get_number_of_data_to_send() > DATA_FLUSH_THRESHOLD) {
				wifi_power_on();
				restartingStart = xTaskGetTickCount();
				g_state = STATE_RESTARTING;
			}
		}
			break;

		default:
			break;
	}
}

void da16600_thrd_entry(void* pvParameters) {
	FSP_PARAMETER_NOT_USED(pvParameters);

	da16600_err_t err = DA16600_SUCCESS;
	fsp_err_t fErr;

	/*
	fErr = R_GPT_Open(&g_baudrate_gen_ctrl, &g_baudrate_gen_cfg);
	if (fErr)
		__BKPT();

	fErr = R_GPT_OutputEnable(&g_baudrate_gen_ctrl, GPT_IO_PIN_GTIOCA);
	if (fErr)
		__BKPT();

	fErr = R_GPT_Start(&g_baudrate_gen_ctrl);
	if (fErr)
		__BKPT();
	*/

	fErr = R_RTC_Open(&g_rtc0_ctrl, &g_rtc0_cfg);
	if (fErr)
		__BKPT();

	rm_fifo_init(&g_used_queue, g_used_queue_buf, EVENT_QUEUE_POOL_LEN);
	rm_fifo_init(&g_free_queue, g_free_queue_buf, EVENT_QUEUE_POOL_LEN);
	/* Fill free queue with (pointers to) buffers */
	for (int i = 0; i < EVENT_QUEUE_POOL_LEN; i++) {
		bool added;
		uint8_t* p_buf = (uint8_t*)&g_event_buf_pool[i][0];
		if (false == (added = rm_fifo_put(&g_free_queue, &p_buf))) {
			assert(true == added);
			break;
		}
	}

	areQueuesReady = 1;

	g_state = STATE_RESTARTING;
	restartingStart = xTaskGetTickCount();

	SEGGER_RTT_printf(0, "\r\nLoading DA16600 drivers.");

	do {
		err = rm_da16600_open(&g_da16600_cfg);
		if (err) {
			SEGGER_RTT_printf(0, "\r\nError while opening DA16600 drivers.");
			vTaskDelay(pdMS_TO_TICKS(10000));
		}

	} while (err);

	SEGGER_RTT_printf(0, "\r\nDA16600 driver is ready.");

	while (1) {
		event_t* p_event;
		if (true == rm_fifo_get(&g_used_queue, (uint8_t**)&p_event)) {
			process_event(p_event);

			/* Free the buffer now event has been processed */
			rm_fifo_put(&g_free_queue, (uint8_t**)&p_event);
		} else {
			event_t event = { .id = EVENT_POLL, };
			process_event(&event);
		}

		rm_da16600_task();
	}
}
