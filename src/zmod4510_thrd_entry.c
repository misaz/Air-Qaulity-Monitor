#include "zmod4510_thrd.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include "air_quality_data_publish_interface.h"

static int eventOccured = 0;
static rm_zmod4xxx_event_t lastEvent = -1;

void zmod4xxx_comms_i2c_callback(rm_zmod4xxx_callback_args_t* p_args) {
	lastEvent = p_args->event;
	eventOccured = 1;
}

static zmod4510_air_quality_data workingAirQuality;
static TickType_t lastPublishTime = 0;

static int isInitialized = 0;
static int isI2COpen = 0;
static int isZmodOpen = 0;

static void zmod4510_initialize_sensor() {
	if (isI2COpen) {
		R_IIC_MASTER_Close(&g_i2c_master0_ctrl);
		isI2COpen = 0;
	}

	if (isZmodOpen) {
		RM_ZMOD4XXX_Close(&g_zmod4510_ctrl);
		isZmodOpen = 0;
	}

	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D3_MIKROBUS_PWM, 1);
	vTaskDelay(pdMS_TO_TICKS(10));
	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D3_MIKROBUS_PWM, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	R_IOPORT_PinWrite(&g_ioport_ctrl, ARDUINO_D3_MIKROBUS_PWM, 1);
	vTaskDelay(pdMS_TO_TICKS(10));

	fsp_err_t fStatus;

	fStatus = R_IIC_MASTER_Open(&g_i2c_master0_ctrl, &g_i2c_master0_cfg);
	if (fStatus) {
		return;
	}
	isI2COpen = 1;

	for (int i = 0; i < 20; i++) {
		g_i2c_master0_ctrl.p_reg->ICCR1_b.CLO = 1;
		while (g_i2c_master0_ctrl.p_reg->ICCR1_b.CLO) { __WFI(); }
	}

	fStatus = RM_ZMOD4XXX_Open(&g_zmod4510_ctrl, &g_zmod4510_cfg);
	if (fStatus) {
		return;
	}
	isZmodOpen = 1;

	isInitialized = 1;
}

void zmod4510_thrd_entry(void* pvParameters) {
	FSP_PARAMETER_NOT_USED(pvParameters);

	fsp_err_t fStatus;
	TickType_t operationStart;

	SEGGER_RTT_printf(0, "\r\nInitializing");

	/*
	 vTaskDelay(10);

	 while (1) {
	 workingAirQuality.ozone = 111.23;
	 workingAirQuality.airQuality = 44;
	 workingAirQuality.fastAirQuality = 33;
	 publish_air_quality(&workingAirQuality);
	 vTaskDelay(10000);
	 }
	 */

	while (1) {
		if (!isInitialized) {
			zmod4510_initialize_sensor();
		}

		if (!isInitialized) {
			vTaskDelay(10);
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// step 1: RM_ZMOD4XXX_MeasurementStart
		///////////////////////////////////////////////////////////////////////////////////

		fStatus = RM_ZMOD4XXX_MeasurementStart(&g_zmod4510_ctrl);
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_MeasurementStart failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}

		operationStart = xTaskGetTickCount();
		while (eventOccured == 0 && xTaskGetTickCount() - operationStart < 5000) {
			__WFI();
		}
		if (eventOccured == 0) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_MeasurementStart timed out. Event code: %d.", lastEvent);
		}
		eventOccured = 0;

		if (lastEvent == RM_ZMOD4XXX_EVENT_SUCCESS) {
			// OAQ v2 require 2s delay
			vTaskDelay(pdMS_TO_TICKS(2000));
		} else {
			SEGGER_RTT_printf(0, "\r\nError after RM_ZMOD4XXX_MeasurementStart. Event code: %d.", lastEvent);
			isInitialized = 0;
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// step 2: RM_ZMOD4XXX_StatusCheck
		///////////////////////////////////////////////////////////////////////////////////

		fStatus = RM_ZMOD4XXX_StatusCheck(&g_zmod4510_ctrl);
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_StatusCheck failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}

		operationStart = xTaskGetTickCount();
		while (eventOccured == 0 && xTaskGetTickCount() - operationStart < 5000) {
			__WFI();
		}
		if (eventOccured == 0) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_StatusCheck timed out. Event code: %d.", lastEvent);
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_MEASUREMENT_COMPLETE) {
			SEGGER_RTT_printf(0, "\r\nError after RM_ZMOD4XXX_StatusCheck. Event code: %d.", lastEvent);
			isInitialized = 0;
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// step 3: RM_ZMOD4XXX_DeviceErrorCheck
		///////////////////////////////////////////////////////////////////////////////////

		fStatus = RM_ZMOD4XXX_DeviceErrorCheck(&g_zmod4510_ctrl);
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_DeviceErrorCheck failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}

		operationStart = xTaskGetTickCount();
		while (eventOccured == 0 && xTaskGetTickCount() - operationStart < 5000) {
			__WFI();
		}
		if (eventOccured == 0) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_DeviceErrorCheck timed out. Event code: %d.", lastEvent);
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_SUCCESS) {
			SEGGER_RTT_printf(0, "\r\nError after RM_ZMOD4XXX_DeviceErrorCheck. Event code: %d.", lastEvent);
			isInitialized = 0;
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// step 4: RM_ZMOD4XXX_Read
		///////////////////////////////////////////////////////////////////////////////////

		rm_zmod4xxx_raw_data_t rawData;

		fStatus = RM_ZMOD4XXX_Read(&g_zmod4510_ctrl, &rawData);
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_Read failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}

		operationStart = xTaskGetTickCount();
		while (eventOccured == 0 && xTaskGetTickCount() - operationStart < 5000) {
			__WFI();
		}
		if (eventOccured == 0) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_Read timed out. Event code: %d.", lastEvent);
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_SUCCESS) {
			SEGGER_RTT_printf(0, "\r\nError after RM_ZMOD4XXX_Read. Event code: %d.", lastEvent);
			isInitialized = 0;
			continue;
		}

		/*
		 SEGGER_RTT_printf(0, "ADC data: ");
		 for (int i = 0 ; i < 32; i++) {
		 SEGGER_RTT_printf(0, "%02x ", rawData.adc_data[i]);
		 }
		 */

		///////////////////////////////////////////////////////////////////////////////////
		// step 5: RM_ZMOD4XXX_DeviceErrorCheck
		///////////////////////////////////////////////////////////////////////////////////

		fStatus = RM_ZMOD4XXX_DeviceErrorCheck(&g_zmod4510_ctrl);
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_DeviceErrorCheck failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}

		operationStart = xTaskGetTickCount();
		while (eventOccured == 0 && xTaskGetTickCount() - operationStart < 5000) {
			__WFI();
		}
		if (eventOccured == 0) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_DeviceErrorCheck timed out. Event code: %d.", lastEvent);
		}
		eventOccured = 0;

		if (lastEvent != RM_ZMOD4XXX_EVENT_SUCCESS) {
			SEGGER_RTT_printf(0, "\r\nError after RM_ZMOD4XXX_DeviceErrorCheck. Event code: %d.", lastEvent);
			isInitialized = 0;
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// step 6: RM_ZMOD4XXX_TemperatureAndHumiditySet
		///////////////////////////////////////////////////////////////////////////////////
		/*
		fStatus = RM_ZMOD4XXX_TemperatureAndHumiditySet(&g_zmod4510_ctrl, 22, 60);
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_TemperatureAndHumiditySet failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}
		*/


		///////////////////////////////////////////////////////////////////////////////////
		// step 7: RM_ZMOD4XXX_Oaq2ndGenDataCalculate
		///////////////////////////////////////////////////////////////////////////////////

		rm_zmod4xxx_oaq_2nd_data_t data;

		fStatus = RM_ZMOD4XXX_Oaq2ndGenDataCalculate(&g_zmod4510_ctrl, &rawData, &data);
		if (fStatus == FSP_ERR_SENSOR_IN_STABILIZATION) {
			SEGGER_RTT_printf(0, "\r\nSensor in stabilisation.");
			continue;
		}
		if (fStatus) {
			SEGGER_RTT_printf(0, "\r\nRM_ZMOD4XXX_Oaq2ndGenDataCalculate failed. Event code: %d.", fStatus);
			isInitialized = 0;
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// Retrieved data processing
		///////////////////////////////////////////////////////////////////////////////////

		char buffer[200];

		SEGGER_RTT_printf(0, "Rmox: ");
		for (int i = 0; i < 8; i++) {
			snprintf(buffer, sizeof(buffer), "%.1f ", data.rmox[i]);
			SEGGER_RTT_printf(0, buffer);
		}

		snprintf(buffer, sizeof(buffer), "\r\n   Ozone: %.3f   AQI: %d   Fast AQI: %d", data.ozone_concentration, (int)data.epa_aqi, (int)data.fast_aqi);
		SEGGER_RTT_printf(0, buffer);

		TickType_t now = xTaskGetTickCount();
		if (xTaskGetTickCount() - lastPublishTime > 30000) {
			SEGGER_RTT_printf(0, "\r\nPublishing data");

			workingAirQuality.airQuality = data.epa_aqi;
			workingAirQuality.fastAirQuality = data.fast_aqi;
			workingAirQuality.ozone = data.ozone_concentration;

			publish_air_quality(&workingAirQuality);
			lastPublishTime = now;
		}
	}
}
