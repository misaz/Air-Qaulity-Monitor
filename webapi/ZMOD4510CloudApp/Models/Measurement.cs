using Azure;
using Azure.Data.Tables;

namespace ZMOD4510CloudApp.Models {
	public class Measurement {
		public string DeviceId { get; set; }
		public DateTime MeasurementTimeStamp { get; set; }
		public float OzoneLevel { get; set; }
		public int AirQualityIndex { get; set; }
		public int FastAirQualityIndex { get; set; }
	}
}
