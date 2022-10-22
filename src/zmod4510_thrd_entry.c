#include "zmod4510_thrd.h"
#include "SEGGER_RTT/SEGGER_RTT.h"

#define BREAK_IF_ERROR(status) if (status) { __BKPT(0); }

static int eventOccured = 0;
static rm_zmod4xxx_event_t lastEvent = -1;

void zmod4xxx_comms_i2c_callback(rm_zmod4xxx_callback_args_t* p_args) {
	lastEvent = p_args->event;
	eventOccured = 1;
}

void zmod4510_thrd_entry(void* pvParameters) {
	FSP_PARAMETER_NOT_USED(pvParameters);

	SEGGER_RTT_printf(0, "Init OK\r\n");

	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D3_MIKROBUS_PWM, 1);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D3_MIKROBUS_PWM, 0);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D3_MIKROBUS_PWM, 1);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

	fsp_err_t fStatus;

	fStatus = R_IIC_MASTER_Open(&g_i2c_master0_ctrl, &g_i2c_master0_cfg);
	BREAK_IF_ERROR(fStatus);

	fStatus = RM_ZMOD4XXX_Open(&g_zmod4510_ctrl, &g_zmod4510_cfg);
	BREAK_IF_ERROR(fStatus);

	while (1) {
		fStatus = RM_ZMOD4XXX_MeasurementStart(&g_zmod4510_ctrl);
		BREAK_IF_ERROR(fStatus);

		while (eventOccured == 0) {
			__WFI();
		}
		eventOccured = 0;

		if (lastEvent == RM_ZMOD4XXX_EVENT_SUCCESS) {
			// OAQ v2 require 2s delay
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
		} else {
			SEGGER_RTT_printf(0, "Error after RM_ZMOD4XXX_MeasurementStart. Event code: %d.\r\n", lastEvent);
			while (1) {
				__WFI();
			}
		}

		fStatus = RM_ZMOD4XXX_StatusCheck(&g_zmod4510_ctrl);
		BREAK_IF_ERROR(fStatus);

		while (eventOccured == 0) {
			__WFI();
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_MEASUREMENT_COMPLETE) {
			SEGGER_RTT_printf(0, "Error after RM_ZMOD4XXX_StatusCheck. Event code: %d.\r\n", lastEvent);
			while (1) {
				__WFI();
			}
		}

		fStatus = RM_ZMOD4XXX_DeviceErrorCheck(&g_zmod4510_ctrl);
		BREAK_IF_ERROR(fStatus);

		while (eventOccured == 0) {
			__WFI();
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_SUCCESS) {
			SEGGER_RTT_printf(0, "Error after RM_ZMOD4XXX_DeviceErrorCheck. Event code: %d.\r\n", lastEvent);
			while (1) {
				__WFI();
			}
		}

		rm_zmod4xxx_raw_data_t rawData;

		fStatus = RM_ZMOD4XXX_Read(&g_zmod4510_ctrl, &rawData);
		BREAK_IF_ERROR(fStatus);

		while (eventOccured == 0) {
			__WFI();
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_SUCCESS) {
			SEGGER_RTT_printf(0, "Error after RM_ZMOD4XXX_Read. Event code: %d.\r\n", lastEvent);
			while (1) {
				__WFI();
			}
		}

		/*
		 SEGGER_RTT_printf(0, "ADC data: ");
		 for (int i = 0 ; i < 32; i++) {
		 SEGGER_RTT_printf(0, "%02x ", rawData.adc_data[i]);
		 }
		 */

		fStatus = RM_ZMOD4XXX_DeviceErrorCheck(&g_zmod4510_ctrl);
		BREAK_IF_ERROR(fStatus);

		while (eventOccured == 0) {
			__WFI();
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_SUCCESS) {
			SEGGER_RTT_printf(0, "Error after RM_ZMOD4XXX_DeviceErrorCheck. Event code: %d.\r\n", lastEvent);
			while (1) {
				__WFI();
			}
		}

		fStatus = RM_ZMOD4XXX_TemperatureAndHumiditySet(&g_zmod4510_ctrl, 22, 60);
		BREAK_IF_ERROR(fStatus);

		rm_zmod4xxx_oaq_2nd_data_t data;

		fStatus = RM_ZMOD4XXX_Oaq2ndGenDataCalculate(&g_zmod4510_ctrl, &rawData, &data);
		if (fStatus == FSP_ERR_SENSOR_IN_STABILIZATION) {
			SEGGER_RTT_printf(0, "Sensor in stabilisation.\r\n");
			continue;
		}
		BREAK_IF_ERROR(fStatus);

		char buffer[200];

		SEGGER_RTT_printf(0, "Rmox: ");
		for (int i = 0; i < 8; i++) {
			snprintf(buffer, sizeof(buffer), "%.1f ", data.rmox[i]);
			SEGGER_RTT_printf(0, buffer);
		}

		snprintf(buffer, sizeof(buffer), "   Ozone: %.3f   AQI: %d   Fast AQI: %d\r\n", data.ozone_concentration, (int)data.epa_aqi, (int)data.fast_aqi);
		SEGGER_RTT_printf(0, buffer);

	}
}
