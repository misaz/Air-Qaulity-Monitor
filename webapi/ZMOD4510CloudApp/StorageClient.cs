using Azure.Data.Tables;
using ZMOD4510CloudApp.Models;

namespace ZMOD4510CloudApp {
	public class StorageClient {

		const string measurementsTableName = "measurements";
		const string devicesTableName = "devices";

		private TableClient measurementsTableClient;
		private TableClient devicesTableClient;

		public StorageClient(string connectionString) {
			TableServiceClient tsc = new TableServiceClient(connectionString);
			tsc.CreateTableIfNotExists(measurementsTableName);
			tsc.CreateTableIfNotExists(devicesTableName);

			measurementsTableClient = tsc.GetTableClient(measurementsTableName);
			devicesTableClient = tsc.GetTableClient(devicesTableName);
		}

		public async Task AddMeasurement(Measurement measurement) {
			string partKey = measurement.DeviceId;
			string rowKey = measurement.MeasurementTimeStamp.ToString("yyyy-MM-dd HH:mm:ss");

			var entity = new TableEntity(partKey, rowKey) {
				{ "DeviceId", measurement.DeviceId },
				{ "MeasurementTimeStamp", measurement.MeasurementTimeStamp },
				{ "OzoneLevel", measurement.OzoneLevel },
				{ "AirQualityIndex", measurement.AirQualityIndex },
				{ "FastAirQualityIndex", measurement.FastAirQualityIndex },
			};
			await measurementsTableClient.AddEntityAsync(entity);
		}


	}
}
