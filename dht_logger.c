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

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define MAXTIMINGS 100
#define PIN_DHT 4 //GPIO Mapping DHT Sensor
#define PIN_LED RPI_GPIO_P1_12 //GPIO Mapping LED
//#define DEBUG

using namespace std;

int readDHT(int pin, float *humid0, float *temp0);

int cosmput(float humid, float temp, int *feedid, char *key, char *feed_name, char *field0_name, char *field1_name);

int readconfig(char *pFileName, int *feedid, char *key, char *feed_name, char *field0_name, char *field1_name);

int main(int argc, char **argv) {
	
	int dhtpin = PIN_DHT;
	int ledpin = PIN_LED;
	float humid0, temp0, ahumid, atemp;
	
	int feedid = 0;
	char key[100];
	char feed_name[100];
	char field0_name[100]; 
	char field1_name[100];

	char pFileName[]="config.ini";
	readconfig(pFileName, &feedid, key, feed_name, field0_name, field1_name);

	if (!bcm2835_init())
		return 1;

	bcm2835_gpio_fsel(ledpin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(ledpin, HIGH); //LED an
	
	printf("Using pin #%d\n", dhtpin);
	
	while(1) {
		ahumid = atemp = 0.0;
		for (int i=0; i<5; i++) {	// Mittelwert bilden, um "zittern" der Kurve zu minimieren
			readDHT(dhtpin, &humid0, &temp0);
			ahumid = ahumid + humid0;
			atemp = atemp + temp0;
			sleep(1);			
		}
		ahumid = ahumid / 5;
		atemp = atemp / 5;
		printf("Temp: %0.1f Humid: %0.1f\n", atemp, ahumid);				
		cosmput(ahumid, atemp, &feedid, key, feed_name, field0_name, field1_name);
	}
	
	bcm2835_gpio_fsel(ledpin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(ledpin, LOW); //LED aus
	
	return 0;
} // main

int readDHT(int pin, float *humid0, float *temp0) {
	int counter = 0;
	int laststate = HIGH;
	int j=0;

	int bits[250], data[100];
	int bitidx = 0;

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
  	printf("Data (%d): 0x%x 0x%x 0x%x 0x%x 0x%x\n", j, data[0], data[1], data[2], data[3], data[4]);
#endif

  	if ((j >= 39) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) ) {	// yay!
		float f, h;
		h = data[0] * 256 + data[1];
		h /= 10;

		f = (data[2] & 0x7F)* 256 + data[3];
        	f /= 10.0;
        	if (data[2] & 0x80) {
			f *= -1;
		}
		//printf("Temp =  %.1f *C, Hum = %.1f \%\n", f, h);
	
		*humid0 = h;
		*temp0 = f;
	}	
  	return 0;
}

int cosmput(float humid, float temp, int *feedid, char *key, char *feed_name, char *field0_name, char *field1_name) {

	CURL *curl;
	CURLcode res;

	char xapikey[60];
	sprintf(xapikey, "X-ApiKey: %s",key);
	char url[50];
	sprintf(url, "http://api.cosm.com/v2/feeds/%d.json", *feedid);
	
	char payload[200];
	sprintf(payload, "{\"title\":\"%s\",\"version\":\"1.0.0\",\"datastreams\":[{\"id\":\"%s\",\"current_value\":%0.1f},{\"id\":\"%s\",\"current_value\":%0.1f}]}", feed_name, field0_name, humid, field1_name, temp);
	struct curl_slist *header=NULL;
	header = curl_slist_append(header, xapikey);

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
      		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}

	curl_easy_cleanup(curl);
	curl_slist_free_all(header);
	curl_global_cleanup();
	return 0;
}

int readconfig(char *pFileName, int *feedid, char *key, char *feed_name, char *field0_name, char *field1_name) {
	char buffer[1024];
	char label[120];
	char value[100];
	int allread = 0;

	FILE* fp;
	fp = fopen(pFileName, "r");
	if (!fp) {
		printf("Error opening config_file %s!\n", pFileName);
		return 1;
	}
	printf("Opening config file: %s\n", pFileName);
	fflush(stdout);
	while (feof(fp) == 0) {
		fgets(buffer, 1024, fp);
		if ((buffer[0] != '#')) // && (no>2))
		{
			if (sscanf(buffer, "%[^'=']=%[^'\n']%s", &label, &value) >= 2){
				if (strcmp(label, "FEEDID") == 0)
					*feedid = atoi(value);
					
				if (strcmp(label, "KEY") == 0)
					sprintf(key, "%s", value);
					
				if (strcmp(label, "FEED_NAME") == 0) 
					sprintf(feed_name, "%s", value);
					
				if (strcmp(label, "FIELD0_NAME") == 0) 
					sprintf(field0_name, "%s", value);
					
				if (strcmp(label, "FIELD1_NAME") == 0) 
					sprintf(field1_name, "%s", value);
			}
		}
	}
	fclose(fp);
	return 0;
}
