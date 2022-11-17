#ifndef AIR_QUALITY_DATA_PUBLISH_INTERFACE_H_
#define AIR_QUALITY_DATA_PUBLISH_INTERFACE_H_

typedef struct {
	float ozone;
	int airQuality;
	int fastAirQuality;
	int year;
	uint8_t month;
	uint8_t date;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} zmod4510_air_quality_data;

int publish_air_quality(zmod4510_air_quality_data* air_quality);

#endif
