//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//


// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <bcm2835.h>
#include <unistd.h>
#include <iostream>
#include <curl/curl.h>

#define MAXTIMINGS 100

//#define DEBUG

#define DHT11 11
#define DHT22 22
#define AM2302 22

#define PIN_DHT 4 //GPIO Mapping DHT Sensor
#define PIN_LED RPI_GPIO_P1_12 //GPIO Mapping LED

#define FEEDID YOUR_ID
#define KEY "YOUR_KEY"

using namespace std;

int readDHT(int type, int pin, float *humid0, float *temp0);

int cosimput(float *humid0, float *temp0);

int main(int argc, char **argv) {

	int type = DHT22;
	int dhtpin = PIN_DHT;
	int ledpin = PIN_LED;
	int j = 0;
	float humid0, temp0;

	if (!bcm2835_init())
		return 1;

	bcm2835_gpio_fsel(ledpin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(ledpin, HIGH); //LED an
	
	printf("Using pin #%d\n", dhtpin);
	
	while(j < 10) {
		cout << j+1 << endl;
		readDHT(type, dhtpin, &humid0, &temp0);
		cosimput(&humid0, &temp0);
		printf("Temp: %0.1f Humid: %0.1f\n", temp0, humid0);
		sleep(5);
		temp0 = humid0 = 0.0;
		j++;
	}
	
	bcm2835_gpio_fsel(ledpin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(ledpin, LOW); //LED aus
	
	return 0;

} // main


int bits[250], data[100];
int bitidx = 0;

int readDHT(int type, int pin, float *humid0, float *temp0) {
  int counter = 0;
  int laststate = HIGH;
  int j=0;

  // Set GPIO pin to output
  bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

  bcm2835_gpio_write(pin, HIGH);
  usleep(500000);  // 500 ms
  bcm2835_gpio_write(pin, LOW);
  usleep(20000);

  bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  // wait for pin to drop?
  while (bcm2835_gpio_lev(pin) == 1) {
    usleep(1);
  }

  // read data!
  for (int i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while ( bcm2835_gpio_lev(pin) == laststate) {
	counter++;
	//nanosleep(1);		// overclocking might change this?
        if (counter == 1000)
	  break;
    }
    laststate = bcm2835_gpio_lev(pin);
    if (counter == 1000) break;
    bits[bitidx++] = counter;

    if ((i>3) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      data[j/8] <<= 1;
      if (counter > 200)
        data[j/8] |= 1;
      j++;
    }
  }


#ifdef DEBUG
  for (int i=3; i<bitidx; i+=2) {
    printf("bit %d: %d\n", i-3, bits[i]);
    printf("bit %d: %d (%d)\n", i-2, bits[i+1], bits[i+1] > 200);
  }
#endif

  printf("Data (%d): 0x%x 0x%x 0x%x 0x%x 0x%x\n", j, data[0], data[1], data[2], data[3], data[4]);

  if ((j >= 39) &&
      (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) ) {
     // yay!
     if (type == DHT11)
	printf("Temp = %d *C, Hum = %d \%\n", data[2], data[0]);
     if (type == DHT22) {
	float f, h;
	h = data[0] * 256 + data[1];
	h /= 10;

	f = (data[2] & 0x7F)* 256 + data[3];
        f /= 10.0;
        if (data[2] & 0x80)  f *= -1;
	printf("Temp =  %.1f *C, Hum = %.1f \%\n", f, h);
	
	*humid0 = h;
	*temp0 = f;
    }
    
    return 1;
  }

  return 0;
}

int cosimput(float *humid0, float *temp0) {

	CURL *curl;

	int feedid = FEEDID;
	char key[] = KEY;
	char 

	char xapikey[60];
	sprintf(xapikey, "X-ApiKey: %s",key);
	char url[50];
	sprintf(url, "http://api.cosm.com/v2/feeds/%d.json", feedid);
	
	char payload[160];
	sprintf(payload, "{\"title\":\"tehu_logger\",\"version\":\"1.0.0\",\"datastreams\":[{\"id\":\"humid0\",\"current_value\":%0.1f},{\"id\":\"temp0\",\"current_value\":%0.1f}]}", *humid0, *temp0);

	struct curl_slist *header=NULL;
	header = curl_slist_append(header, xapikey);

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

	CURLcode res = curl_easy_perform(curl);
	if (res != 0) {
		cout << "result " << res << endl;
	}

	curl_slist_free_all(header);
	curl_global_cleanup();
	return 0;
}
