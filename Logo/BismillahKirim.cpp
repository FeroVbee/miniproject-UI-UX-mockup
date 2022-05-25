#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/i2c-dev.h>
#include <time.h>
#include <mosquitto.h>
#include <jansson.h>

#include <wiringPi.h>
#include <wiringPiI2C.h>

#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <thread>

#include "Include/aes256/aes256.hpp"

using namespace std;

// DEFINING ADDRESS for GY-521 =========================================
#define MPU6050_GYRO_XOUT_H        0x43   // R
#define MPU6050_GYRO_YOUT_H        0x45   // R
#define MPU6050_GYRO_ZOUT_H        0x47   // R
 
#define MPU6050_PWR_MGMT_1         0x6B   // R/W
#define MPU6050_I2C_ADDRESS        0x68   // I2C

// I2C =================================================================
int file, length, res;
int buff[2] = {0};

// TIMER ===============================================================
//static uint64_t epochMicro, epochMilli;

// MQTT ================================================================
struct mosquitto *mosq = NULL;
char *topic = NULL;

// SECURITY PROGRAM ====================================================
#define KEY_LEN    11
const unsigned char *symetry_key = (unsigned char*)"26321323426";
unsigned char* chipper_aes;
unsigned char* plain_aes;
ByteArray key;
size_t aesplain_len = 0;
size_t aeschipper_len = 0;

// Secure Comm Function ================================================
void init_key(ByteArray& key){
    for (unsigned char i = 0; i < KEY_LEN;i++)
        key.push_back(symetry_key[i]);
}

// M2M Communicate Function ============================================
void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
  /* Pring all log messages regardless of level. */
  switch(level){
    //case MOSQ_LOG_DEBUG:
    //case MOSQ_LOG_INFO:
    //case MOSQ_LOG_NOTICE:
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR: {
      printf("%i:%s\n", level, str);
    }
  }
}

void mqtt_setup(){

  char *host = (char*)"localhost";//"103.24.56.218";
  //char *username = (char*)"pensbl";
  //char *password = (char*)"1sampai0";
  int port = 1883;
  int keepalive = 60;
  bool clean_session = true;
  topic = (char*)"drone1/djidata";
   
  mosquitto_lib_init();
  mosq = mosquitto_new(NULL, clean_session, NULL);
  if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
	}
  
  mosquitto_log_callback_set(mosq, mosq_log_callback);
  //mosquitto_username_pw_set(mosq, username, password);
  if(mosquitto_connect(mosq, host, port, keepalive)){
		fprintf(stderr, "Unable to connect.\n");
	}
  int loop = mosquitto_loop_start(mosq);
  if(loop != MOSQ_ERR_SUCCESS){
    fprintf(stderr, "Unable to start loop: %i\n", loop);
  }
}

int mqtt_send(char *msg){
  return mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, 0);
}

float x,y,z;
 
void mqt()
{    
    int snd = 0;
    char *buf = (char*)malloc(500); // plain
    ByteArray enc;;//, dec;
    
    mqtt_setup();
    
    //json_t *root = json_object();
    while(1)
    {            
        //json_object_set_new( root, "Gyro X", json_real( x ));
        //json_object_set_new( root, "Gyro Y", json_real( y ));
        //json_object_set_new( root, "Gyro Z", json_real( z ));        
        
        //buf = json_dumps(root, 0);
	//aesplain_len = strlen(buf);
	sprintf(buf, "#%0.2f,%0.2f,%0.2f",x,y,z);
	//printf("%s\n",buf);
	aesplain_len = 0;
	//aeschipper_len = 0;
	plain_aes = (unsigned char*) buf;
	//plain_aes = (unsigned char*)"Hello world my name is febby";
	while(plain_aes[aesplain_len] != '\0') aesplain_len++;
	enc.clear();
	ByteArray::size_type enc_len = Aes256::encrypt(key, plain_aes , aesplain_len, enc);
	chipper_aes = enc.data();
	//printf("%s\n", enc.data());
	//printf("%s\n", chipper_aes);
	
	//while(chipper_aes[aeschipper_len] != '\0') aeschipper_len++;
	//printf("length %d\n", aeschipper_len);
	//dec.clear();
	//ByteArray::size_type dec_len = Aes256::decrypt(key, chipper_aes, aeschipper_len, dec);
	//printf("%s\n", dec.data());
	

        topic = (char*)"drone1/imu";
        snd = mqtt_send((char*)enc.data());
        
        if(snd != 0) printf("mqtt_send error=%i\n", snd);
        else printf("data sending oke\n");
        sleep(1);
    }
}

// THREAD Function =====================================================
void LED_Test(){	
	wiringPiSetup () ;
	pinMode (15, OUTPUT);
	while(1){
		digitalWrite (15, HIGH) ;
		delay(1000);
		digitalWrite (15, LOW) ;
		delay(1000);
	}
}

void IMU_Test(){
    int fd = wiringPiI2CSetup(MPU6050_I2C_ADDRESS);
    if (fd == -1)
        printf("Failed to setting up I2C");
 
    wiringPiI2CReadReg8 (fd, MPU6050_PWR_MGMT_1);
    wiringPiI2CWriteReg16(fd, MPU6050_PWR_MGMT_1, 0);
    
    while(true)
    {
        x = wiringPiI2CReadReg8(fd, MPU6050_GYRO_XOUT_H);
        y = wiringPiI2CReadReg8(fd, MPU6050_GYRO_YOUT_H);
        z = wiringPiI2CReadReg8(fd, MPU6050_GYRO_ZOUT_H);
 
       // printf("x=%f   y=%f   z=%f\n", x,y,z); 
    }
}

// MAIN PROGRAM ========================================================
int main(int argc, char *argv[])
{
	init_key(key);
	cout << "Key initilized..." << endl;
    
	thread LEDtest(LED_Test);
	thread IMUtest(IMU_Test);
	thread kirimdata(mqt);

	kirimdata.join();
	LEDtest.join();
	IMUtest.join();
}
