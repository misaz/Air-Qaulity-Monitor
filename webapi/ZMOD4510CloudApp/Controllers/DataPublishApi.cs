using Microsoft.AspNetCore.Mvc;
using System.IO;
using System.Text;
using System.Text.Json;
using ZMOD4510CloudApp.Models;

namespace ZMOD4510CloudApp.Controllers {

	[Route("api/")]
	[ApiController]
	public class DataPublishApi : Controller {
		
		private StorageClient _storageClient;

		public DataPublishApi(StorageClient storageClient) {
			_storageClient = storageClient;
		}

		[HttpPost]
		[Route("publish_air_quality")]
		public async Task<ActionResult> PublishMeasurement() {
			string requestPayload;

			using (StreamReader sr = new StreamReader(Request.Body)) {
				requestPayload = await sr.ReadToEndAsync();
			}

			byte[] rawData;
			try {
				rawData = Convert.FromBase64String(requestPayload);
			} catch (Exception) {
				return BadRequest();
			}

			if (rawData.Length != 48) {
				return BadRequest();
			}

			UInt32[] deviceId = new UInt32[] {
				BitConverter.ToUInt32(rawData, 0),
				BitConverter.ToUInt32(rawData, 1),
				BitConverter.ToUInt32(rawData, 2),
				BitConverter.ToUInt32(rawData, 3)
			};

			string deviceIdString = 
				deviceId[0].ToString("X8") + 
				deviceId[1].ToString("X8") + 
				deviceId[2].ToString("X8") + 
				deviceId[3].ToString("X8");

			UInt16 year = BitConverter.ToUInt16(rawData, 32);
			byte month = rawData[34];
			byte date = rawData[35];
			byte hour = rawData[36];
			byte minute = rawData[37];
			byte second = rawData[38];

			float ozoneLevel = BitConverter.ToSingle(rawData, 40);

			byte airQualityIndex = rawData[44];
			byte fastAirQualityIndex = rawData[45];

			if (month < 1 || month > 12 || date < 1 || date > 31 || hour > 23 || minute > 59 || second > 60) {
				return BadRequest();
			}

			Measurement m = new Measurement();
			m.MeasurementTimeStamp = new DateTime(year, month, date, hour, minute, second, DateTimeKind.Utc);	
			m.AirQualityIndex = airQualityIndex;
			m.FastAirQualityIndex = fastAirQualityIndex;
			m.DeviceId = deviceIdString;
			m.OzoneLevel = ozoneLevel;

			try {
				await _storageClient.AddMeasurement(m);
			} catch (Exception) {
				return StatusCode(500);
			}

			return Ok();
		}

	}
}
