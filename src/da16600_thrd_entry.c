#include "da16600_thrd.h"

void da16600_thrd_entry(void* pvParameters) {
	FSP_PARAMETER_NOT_USED(pvParameters);

	while (1) {
		vTaskDelay(1);
	}
}
