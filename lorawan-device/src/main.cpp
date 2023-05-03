#include "LoRaWan_APP.h"
#include <Arduino.h>
#include "driver/temp_sensor.h"   
#include "../compress_buffer.h"

void initTempSensor(){
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor.dac_offset = TSENS_DAC_L2; //TSENS_DAC_L2 is default, range: -10 to 80 degree celcius
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();
}

union ftoi_t {
    float f;
    unsigned int i;
};

RTC_DATA_ATTR uint8_t buffer_contents[47];
RTC_DATA_ATTR uint16_t buffer_nwb = 3;
RTC_DATA_ATTR unsigned int current_time = 0; // seconds
RTC_DATA_ATTR unsigned int n_measurements = 0;

RTC_DATA_ATTR unsigned int times[30];
RTC_DATA_ATTR ftoi_t temperatures[30];
RTC_DATA_ATTR ftoi_t sent_temp_msgs[30];

CompressBuffer buf;

struct time_temp {
	// 4 bytes, 32 bits. Millisecones since device start. Resets around 50 days
	unsigned long time;
	ftoi_t temp; // 4 bytes, 32 bits
};

int leading_zeros(unsigned int val) {
    int ct = 0;
    for (int i = 0; i < sizeof(int)*8; i++){
        if (val & (0x01 << (sizeof(int)*8 - i - 1)))
            return ct;
        else
            ct++;
    }
    return ct;
}

int trailing_zeros(unsigned int val) {
    int ct = 0;
    for (int i = 0; i < sizeof(int)*8; i++){
        if (val & (0x01 << (i)))
            return ct;
        else
            ct++;
    }
    return ct;
}

bool save_current_temp = false;

void print_temps() {
	printf("temperatures: [");
	for (int i = 0; i < 29; i++) {
		printf("%5.2f, ", temperatures[i].f);
	}
	printf("%5.2f]\n", temperatures[29].f);
}

bool collect_data_and_compress() { // return true if ready to send data
	memcpy(buf.buf, buffer_contents, sizeof(buffer_contents));
	buf.next_write_bit = buffer_nwb;
	buf.set_print_status(true);

	// printf("Before adding\n");
	// buf.print_buffer();

	// current_time += appTxDutyCycle / 1000;
	// measurement.time = current_time; // TODO
	// temp_sensor_read_celsius(&measurement.temp.f);
	// measurement_buffer[n_measurements] = measurement;
	// Serial.printf("measurement_buffer[%d] = {%d, %5.2f}\n", n_measurements, measurement.time, measurement.temp.f);
	
	// buf.add_int_val(measurement.temp.i, 32);
	// buf.add_int_val(measurement.time, 32);

	temp_sensor_read_celsius(&temperatures[n_measurements].f);
	current_time += appTxDutyCycle / 1000;
	times[n_measurements] = current_time;
	Serial.printf("Measurement %d: %d seconds, %5.2f deg C\n", n_measurements, times[n_measurements], temperatures[n_measurements].f);

	print_temps();

    // compress into buffer
    // buf.set_print_status(true);
    // bool full = false;
    // for (int meas_n = 0; meas_n < 6 && !full; meas_n++){
	unsigned int meas_n = n_measurements;
	if (meas_n == 0) {
		sent_temp_msgs[0] = temperatures[0];
		printf("Add temp %f: ", temperatures[0].f);
		buf.add_int_val(sent_temp_msgs[0].i, 32); // temp

		printf("Add time %d: ", times[0]);
		buf.add_int_val(times[0], 32); // time
	} else {
		// check timestamp bits
		unsigned int timestamp_n_bits = 0;
		int dd;
		if (meas_n == 1) {
			timestamp_n_bits = 20;
		} else {
			dd = (times[meas_n] - times[meas_n - 1]) - (times[meas_n - 1] - times[meas_n - 2]);
			if (dd == 0) {
				timestamp_n_bits = 1;
			} else if (dd >= -63 && dd <= 64) {
				timestamp_n_bits = 2 + 7;
			} else if (dd >= -255 && dd <= 256) {
				timestamp_n_bits = 3 + 9;
			} else if (dd >= -2047 && dd <= 2048) {
				timestamp_n_bits = 4 + 12;
			} else {
				timestamp_n_bits = 4 + 32; // this shouldn't happen often
			}
		}

		// compute sent temperature
		sent_temp_msgs[meas_n].i = temperatures[meas_n].i^temperatures[meas_n-1].i;
		printf("Add temp %f, encoded as %x:\n", temperatures[meas_n].f, sent_temp_msgs[meas_n].i);

		// check if can add temp, then add if so
		unsigned int leading_i = leading_zeros(sent_temp_msgs[meas_n].i);
		unsigned int trailing_i = trailing_zeros(sent_temp_msgs[meas_n].i);
		unsigned int leading_i_1 = leading_zeros(sent_temp_msgs[meas_n-1].i);
		unsigned int trailing_i_1 = trailing_zeros(sent_temp_msgs[meas_n-1].i);

		if (sent_temp_msgs[meas_n].i == 0) {
			if (!buf.check_add_bits(timestamp_n_bits + 1)) {
				save_current_temp = true;
				return true;
			}

			buf.add_bit(0);
		} else if ((leading_i >= leading_i_1) && (trailing_i == trailing_i_1) && meas_n != 1) { // same leading & trailing
			// check if can add
			unsigned int n_meaningful_bits_prev = sizeof(float)*8 - leading_i_1 - trailing_i_1;
			if (!buf.check_add_bits(timestamp_n_bits + 2 + n_meaningful_bits_prev)) {
				save_current_temp = true;
				return true;
			}

			// add temperature to buffer
			buf.add_bit(1); // store
			buf.add_bit(0); // control
			buf.add_int_range(sent_temp_msgs[meas_n].i, trailing_i_1, n_meaningful_bits_prev);
		} else { // different leading & traling, or it's the second entry
			// check if can add
			unsigned int n_meaningful_bits = sizeof(float)*8 - leading_i - trailing_i;
			if (!buf.check_add_bits(timestamp_n_bits + 2 + 5 + 6 + n_meaningful_bits)) {
				save_current_temp = true;
				return true;
			}

			// add temperature to buffer
			buf.add_bit(1); // store
			buf.add_bit(1); // control
			buf.add_int_val(leading_i, 5);
			buf.add_int_val(n_meaningful_bits, 6);
			buf.add_int_range(sent_temp_msgs[meas_n].i, trailing_i, n_meaningful_bits);
		}

		// add time
		if (meas_n == 1) {
			int diff = times[1] - times[0];
			printf("Adding time %d, encoded as %d:\n", times[1], diff);
			buf.add_int_val(diff, 20); // time. 20 is big enough for 17+ min interval
		} else {
			printf("Adding time %d, encoded as %d:\n", times[meas_n], dd);
			if (dd == 0) {
				buf.add_int_val(0, 1);
			} else if (dd >= -63 && dd <= 64) {
				buf.add_int_val(2, 2);
				buf.add_int_val(dd, 7);
			} else if (dd >= -255 && dd <= 256) {
				buf.add_int_val(6, 3);
				buf.add_int_val(dd, 9);
			} else if (dd >= -2047 && dd <= 2048) {
				buf.add_int_val(14, 4);
				buf.add_int_val(dd, 12);
			} else {
				buf.add_int_val(15, 4);
				buf.add_int_val(dd, 32);
			}
		}
		
	}


	memcpy(buffer_contents, buf.buf, sizeof(buf.buf));
	buffer_nwb = buf.next_write_bit;

	// printf("After adding\n");
	// buf.print_buffer();

	// buf.print_buffer();

	n_measurements++;
	if (n_measurements >= 30) {
		return true;
	} else {
		return false;
	}
}

// RTC_DATA_ATTR time_temp measurement_buffer[30];

// uint32_t temp_collect_period_ms = 3000;
// uint32_t prev_time;

uint8_t devEui[] = {0x75, 0x76, 0x98, 0x78, 0x76, 0x58, 0x76, 0x87};  
bool overTheAirActivation = true;
uint8_t appEui[] = {0xfe, 0xed, 0x13, 0x13, 0xbe, 0xef, 0x13, 0x13};  // you should set whatever your TTN generates. TTN calls this the joinEUI, they are the same thing. 
uint8_t appKey[] = {0xBA, 0x82, 0x40, 0x48, 0x94, 0x04, 0x48, 0x7D, 0x87, 0xF7, 0xAF, 0x69, 0x42, 0x51, 0x02, 0x08};  // you should set whatever your TTN generates 

//These are only used for ABP, for OTAA, these values are generated on the Nwk Server, you should not have to change these values
uint8_t nwkSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t appSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint32_t devAddr =  (uint32_t)0x00000000;  

/*LoraWan channelsmask*/
uint16_t userChannelsMask[6]={ 0xFF00,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;  // we define this as a user flag in the .ini file. 

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 1000;

/*ADR enable*/
bool loraWanAdr = true;

// uint32_t license[4] = {};

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 1;
/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment. The MAC performs a datarate adaptation,
* according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
* to the following table:
*
* Transmission nb | Data Rate
* ----------------|-----------
* 1 (first)       | DR
* 2               | DR
* 3               | max(DR-1,0)
* 4               | max(DR-1,0)
* 5               | max(DR-2,0)
* 6               | max(DR-2,0)
* 7               | max(DR-3,0)
* 8               | max(DR-3,0)
*
* Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
* the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 8;

/* Prepares the payload of the frame */
static void prepareTxFrame( uint8_t port )
{
	/*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
	*appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
	*if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
	*if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
	*for example, if use REGION_CN470, 
	*the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
	*/
    // This data can be changed, just make sure to change the datasize as well. 
	// appDataSize = 1;
	// appData[0] = 1;
	// Serial.printf("Sending packet\n");

	buf.finish();
    appDataSize = buf.size();

	Serial.printf("Sending buffer\n");
	buf.print_buffer();

    for (int i = 0; i < appDataSize; i++) {
        appData[i] = buf.buf[i];
    }

    // return ret;
	// for (int i = 0; i < 4; i++) {
	// 	appData[i] = ((uint8_t*)&measurement_buffer[1].time)[i];
	// }

	ftoi_t extra_temp;
	unsigned int extra_time;
	if (save_current_temp) {
		extra_temp = temperatures[n_measurements];
		extra_time = times[n_measurements];
	} 

	ftoi_t zero;
	zero.f = 0;
	for (int i = 1; i < 30; i++) {
		times[i] = 0;
		temperatures[i] = zero;
		sent_temp_msgs[i] = zero;
	}

	n_measurements = 0;
	buf = CompressBuffer();
	if (save_current_temp) {
		temperatures[0] = extra_temp;
		times[0] = extra_time;
		buf.add_int_val(temperatures[0].i, 32);
		buf.add_int_val(times[0], 32);
		n_measurements++;
	}

	buffer_nwb = buf.next_write_bit;
	memcpy(buffer_contents, buf.buf, sizeof(buf.buf));
}

RTC_DATA_ATTR bool firstrun = true;

void setup() {
  Serial.begin(115200);
  initTempSensor();
//   prev_time = millis();
  Mcu.begin();
  if(firstrun)
  {
    LoRaWAN.displayMcuInit();
    firstrun = false;
  }
  deviceState = DEVICE_STATE_INIT;
//   current_time = 0;
}

void loop()
{
	switch( deviceState )
	{
		case DEVICE_STATE_INIT:
		{
#if(LORAWAN_DEVEUI_AUTO)
			LoRaWAN.generateDeveuiByChipID();
#endif
			LoRaWAN.init(loraWanClass,loraWanRegion);
			break;
		}
		case DEVICE_STATE_JOIN:
		{
      		LoRaWAN.displayJoining();
			LoRaWAN.join();
			if (deviceState == DEVICE_STATE_SEND){
			 	LoRaWAN.displayJoined();
			}
			break;
		}
		case DEVICE_STATE_SEND:
		{
			bool ready = collect_data_and_compress();
			if (ready) {
				printf("Going to send!\n");
				LoRaWAN.displaySending();
				prepareTxFrame( appPort );
				LoRaWAN.send();
			} else {
				printf("Not sending\n");
				
			}
			deviceState = DEVICE_STATE_CYCLE;
			break;
		}
		case DEVICE_STATE_CYCLE:
		{
			// Schedule next packet transmission
			txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
			LoRaWAN.cycle(txDutyCycleTime);
			deviceState = DEVICE_STATE_SLEEP;
			// vish: don't sleep, use millis()
			break;
		}
		case DEVICE_STATE_SLEEP:
		{
			//printf("sleeping");
      		LoRaWAN.displayAck();
			// delay(1);
			// deviceState = DEVICE_STATE_CYCLE;
			LoRaWAN.sleep(loraWanClass);
			// vish: don't sleep, use millis()
			break;
		}
		default:
		{
			deviceState = DEVICE_STATE_INIT;
			break;
		}
	}
}