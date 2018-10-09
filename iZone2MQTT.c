/* iZone2MQTT - a Proggy to act as a bridge between an iZone airconditioning */
/* system and MQTT                                                           */
/* uses:             */
/*      libcurl      */
/*      jansson      */
/*      mosquitto    */
/* Todo */
/* 1 - Put in config items */
/* 2 - make mosquitto reconnect if disconnected */
/* 3 - Disconnect if haven't heard from iZone and cant initialise */
/* 4 - Send state info to MQTT every so often */ 
/* 5 - Tidy code */
/* 6 - Make auto demonize and start/stop script */



#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <jansson.h>
#include <mosquitto.h>


struct ZoneStruct {
	char AirStreamDeviceUId[10];
	int Id;
	int Index;			// Zone number
	char Name[80];		// Zones name
	int Type;			// TYpe of zone. 0=auto, 1=Open/Close only, 2=Constant
	int Mode;			// current mode. 0=close, 1=open, 3=auto
	float SetPoint;	// Current setpoint for the zone
	float Temp;			// Current tempeture of the Zone
	int MaxAir;		// Maximium airflow
	int MinAir;		// Minmium airflow
	int Const;		// The constant number or 255 if not a constant zone
	int ConstA;		// Constatnt Zone Active? 0=no, 1=yes
};

struct SystemSettingsStruct {
	char cb_ip[20];		// The IP address of the CB
	int  cb_port;			// The port the CB is on
	char AirStreamDeviceUId[10];   // 9 digit CN ID
	char DeviceType[4];	       // Always ASH
	int SysOn;		       // System state. 0=off, 1=on
	int SysMode;	 // System Mode. 0=cool, 1=heat, 2=vent, 3=dry, 4=auto
	int SysFan;      // System Fan mode. 0=low, 1=med, 2=high, 3=auto
	int SleepTimer;  // Sleep timer. 0 is off
	char UnitType[30];  // Aircon type
	float Supply;	  // In duct air temp
	float Setpoint;	  // AC setpoint temperature
	float Temp;	  // Return tempeture
	int RAS;	// What Temp indicates 0=Master controler, 1=Return, 2-Zones
	int CtrlZone;	// Zone controlling aircon
	char Tag1[81];	// USer config text on screen
	char Tag2[81];  // user config text on screen
	char Warnings[81];   // System warnings
	char ACError[5];    // AC error code
	int Id;
	int EcoLock;	// EcoLock. 0-off, 1-on
	float EcoMax; 	// Maxium tempture that can be set in EcoLock
	float EcoMin;	// Minimium tempeture that can be set in EcoLock
	int NoOfConst;	// Number of constatnt zones
	int NoOfZones;	// Number of zones. Zero numbering
	char SysType[5];   // iZone system type
	int AirflowLock;  // AitflowLock state. 0-Off, 1-onMin, 2-on
	int UnitLocked;	 // Unit locked?. 0-No, 1-Yes
	int FreeAir;   // Free Air setting. 0-off, -1- disabled,  1=on
	int FanAuto;   // Fan Auto mode. 0-disbaled, 1-var-speed, 2- 2-speed, 3- 3-speed
	struct ZoneStruct Zones[12];		// The zone details Zero Numbering
};


int SystemSettingsData(char *buff, int Size, int Nmemb, struct SystemSettingsStruct *Settings)
{
	int BuffLen;
	json_t *root;
	json_error_t error;
	json_t *numzones;

	const char *temp;
	int t1;

	BuffLen=Size * Nmemb;

	printf("Size=%d, nmemb=%d\n", Size, Nmemb);
	printf("Data=%s\n", buff);

	root=json_loads( buff, 0, &error);
	if( ! root)
	{

		printf("json_loads error: on line %d: %s\n", error.line, error.text);
		return(0);
	}

	strcpy( Settings->AirStreamDeviceUId,json_string_value(json_object_get(root,"AirStreamDeviceUId")));

	strcpy( Settings->DeviceType,json_string_value(json_object_get(root,"DeviceType")));

	temp=json_string_value(json_object_get(root,"SysOn"));
	if( strcmp(temp, "on") == 0) t1=1;
	else if( strcmp(temp, "off") == 0) t1=0;
	else t1=0;
	Settings->SysOn=t1;

	temp=json_string_value(json_object_get(root,"SysMode"));
	if( strcmp(temp, "cool") == 0) t1=0;
	else if( strcmp(temp, "heat") == 0) t1=1;
	else if( strcmp(temp, "vent") == 0) t1=2;
	else if( strcmp(temp, "dry") == 0) t1=3;
	else if( strcmp(temp, "auto") == 0) t1=4;
	else t1=0;
	Settings->SysMode=t1;

	temp=json_string_value(json_object_get(root,"SysFan"));
	if( strcmp(temp, "low") == 0) t1=0;
	else if( strcmp(temp, "med") == 0) t1=1;
	else if( strcmp(temp, "high") == 0) t1=2;
	else if( strcmp(temp, "auto") == 0) t1=3;
	else t1=0;
	Settings->SysFan=t1;

	Settings->SleepTimer=json_integer_value(json_object_get(root, "SleepTimer"));	

	strcpy( Settings->UnitType,json_string_value(json_object_get(root,"UnitType")));

	Settings->Supply=atof(json_string_value(json_object_get(root,"Supply")));

	Settings->Setpoint=atof(json_string_value(json_object_get(root,"Setpoint")));

	Settings->Temp=atof(json_string_value(json_object_get(root,"Temp")));

	temp=json_string_value(json_object_get(root,"RAS"));
	if( strcmp(temp, "master") == 0) t1=0;
	else if( strcmp(temp, "RAS") == 0) t1=1;
	else if( strcmp(temp, "zones") == 0) t1=2;
	else t1=0;
	Settings->RAS=t1;

	Settings->CtrlZone=json_integer_value(json_object_get(root, "CtrlZone"));	

	strcpy( Settings->Tag1,json_string_value(json_object_get(root,"Tag1")));

	strcpy( Settings->Tag2,json_string_value(json_object_get(root,"Tag2")));

	strcpy( Settings->Warnings,json_string_value(json_object_get(root,"Warnings")));

	strcpy( Settings->ACError,json_string_value(json_object_get(root,"ACError")));

	Settings->Id=0;

	temp=json_string_value(json_object_get(root,"EcoLock"));
	if( strcmp(temp, "true") == 0) t1=1;
	else if( strcmp(temp, "false") == 0) t1=0;
	else t1=0;
	Settings->EcoLock=t1;

	Settings->EcoMax=atof(json_string_value(json_object_get(root,"EcoMax")));

	Settings->EcoMin=atof(json_string_value(json_object_get(root,"EcoMin")));

	Settings->NoOfConst=json_integer_value(json_object_get(root, "NoOfConst"));	

	Settings->NoOfZones=json_integer_value(json_object_get(root, "NoOfZones"));	

	strcpy( Settings->SysType,json_string_value(json_object_get(root,"SysType")));

	temp=json_string_value(json_object_get(root,"AirflowLock"));
	if( strcmp(temp, "on") == 0) t1=2;
	if( strcmp(temp, "onMin") == 0) t1=1;
	else if( strcmp(temp, "off") == 0) t1=0;
	else t1=0;
	Settings->AirflowLock=t1;

	temp=json_string_value(json_object_get(root,"UnitLocked"));
	if( strcmp(temp, "true") == 0) t1=1;
	else if( strcmp(temp, "false") == 0) t1=0;
	else t1=0;
	Settings->UnitLocked=t1;

	temp=json_string_value(json_object_get(root,"FreeAir"));
	if( strcmp(temp, "disabled") == 0) Settings->FreeAir=-1;
	else if( strcmp(temp, "off") == 0) Settings->FreeAir=0;
	else if( strcmp(temp, "on") == 0) Settings->FreeAir=1;
	else Settings->FreeAir=0;

	temp=json_string_value(json_object_get(root,"FanAuto"));
	if( strcmp(temp, "disabled") == 0) Settings->FanAuto=0;
	else if( strcmp(temp, "3-speed") == 0) Settings->FanAuto=3;
	else if( strcmp(temp, "2-speed") == 0) Settings->FanAuto=2;
	else if( strcmp(temp, "var-speed") == 0) Settings->FanAuto=1;
	else Settings->FanAuto=0;

	json_decref(root);
	return(BuffLen);

}


// This will analyse the JSON data pointed to by data
// and pack teh data into the array.
// returns 1 if successful or 0 if an error
int iZoneProcessZonesJSON(char *data, struct ZoneStruct Zones[], int NumZones)
{
	json_t *root;
	json_error_t error;
	json_t *element;

	int x;
	int Index;
	const char *temp;
	int t1;


	root=json_loads( data, 0, &error);
	if( ! root)
	{
		printf("json_loads error: on line %d: %s\n", error.line, error.text);
		return(0);
	}

	if( !json_is_array(root))
	{
		printf("Error: root is not an array of zones\n");
		json_decref(root);
		return(0);
	}

	for(x=0; x < json_array_size(root); x++)
	{
		element=json_array_get(root,x);
		if( !json_is_object(element ))
		{
			printf("Error: Array element is not an object\n");
			json_decref(root);
			return(0);
		}
		Index=json_integer_value(json_object_get(element, "Index"));
		if( Index < NumZones)
		{
			Zones[Index].Index=Index;

			strcpy( Zones[Index].AirStreamDeviceUId,json_string_value(json_object_get(element,"AirStreamDeviceUId")));
			Zones[Index].Id=0;
			strcpy( Zones[Index].Name,json_string_value(json_object_get(element,"Name")));
		
			temp=json_string_value(json_object_get(element,"Type"));
			if( strcmp(temp, "auto") == 0) t1=0;
			else if( strcmp(temp, "opcl") == 0) t1=1;
			else if( strcmp(temp, "const") == 0) t1=2;
			else t1=0;
			Zones[Index].Type=t1;

			temp=json_string_value(json_object_get(element,"Mode"));
			if( strcmp(temp, "auto") == 0) t1=3;
			else if( strcmp(temp, "open") == 0) t1=1;
			else if( strcmp(temp, "close") == 0) t1=0;
			else t1=0;
			Zones[Index].Mode=t1;

			Zones[Index].SetPoint=json_number_value(json_object_get(element, "SetPoint"));
			Zones[Index].Temp=json_number_value(json_object_get(element, "Temp"));
			Zones[Index].MaxAir=json_integer_value(json_object_get(element, "MaxAir"));
			Zones[Index].MinAir=json_integer_value(json_object_get(element, "MinAir"));
			Zones[Index].Const=json_integer_value(json_object_get(element, "Const"));
		
			temp=json_string_value(json_object_get(element,"ConstA"));
			if( strcmp(temp, "true") == 0) t1=1;
			else if( strcmp(temp, "false") == 0) t1=0;
			else t1=0;
			Zones[Index].ConstA=t1;
		}
	}

	json_decref(root);
	return(1);
}	



// Call back function for request_url function. just copies
// the data to the buffer
int request_url_resp(void *ptr, int size, int nmemb, char *data)
{
	int RealSize;

	printf("Size=%d, nmemb=%d\n", size, nmemb);
	printf("Data=%s\n", ptr);

	RealSize= size * nmemb;
	memcpy(data, ptr, RealSize+1);

	return( RealSize);
}

// Will read a jason packet from the URL, will palce the return in
// buffer. buffer must be big enough for it.
// return 1 if good, 0 is something wring
int request_url(char *url, char *buffer)
{
	CURL *curl;
	CURLcode status;
	long code;

	curl_global_init(CURL_GLOBAL_ALL);
	curl=curl_easy_init();
	if( ! curl)
	{
		curl_global_cleanup();
		return(0);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_url_resp);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
	status = curl_easy_perform(curl);
	if( status != 0)
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return(0);
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	if( code != 200)
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return(0);
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return(1);
}

// This will read the data for a given group of zones.
// the ZonesURL is the path part of the URL to query
// cb_ip is the IP of the CB
// Zones is the array of zones to put the data into
// returns 1 if ok or 0 is something went wrong
int iZoneReadZoneData(char *ZonesUrl, char *cb_ip, struct ZoneStruct *Zones, int NumZones)
{
	char url[60];
	char data[2000];	// Somewhere to put the returned data

	sprintf(url,"http://%s/%s", cb_ip, ZonesUrl);
	if( ! request_url(url, data))
	{
		return(0);
	}
	iZoneProcessZonesJSON(data,Zones,NumZones);
	return(1);
}

// This will try to get the current seetings from 
// the CB. Puts the data into the structure
// Return 1 of successful or 0 if an error
int iZoneGetSettings(struct SystemSettingsStruct *Settings )
{
	CURL *handle;
	char url[60];
	CURLcode res;

	
	handle=curl_easy_init();
	if( ! handle)
	{
		perror("curl_easy_init");
		return(0);
	}

	sprintf(url,"http://%s/SystemSettings",Settings->cb_ip);
	
	curl_easy_setopt(handle, CURLOPT_URL, url);		// Set the url
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, SystemSettingsData);  // Funtion to handle the data
   curl_easy_setopt(handle, CURLOPT_WRITEDATA, Settings);		// Where to store the data

	res= curl_easy_perform(handle);
	if( res != CURLE_OK)
	{
		printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_easy_cleanup(handle);
		return(0);
	}
	// printf("\nResult of opteratin:: %d\n",res);
	curl_easy_cleanup(handle);

	if( Settings->NoOfZones > 0 )
	{
		if( ! iZoneReadZoneData("Zones1_4",Settings->cb_ip,Settings->Zones,Settings->NoOfZones))
		{
			return(0);
		}
	}
	if( Settings->NoOfZones > 4 )
	{
		if( ! iZoneReadZoneData("Zones5_8",Settings->cb_ip,Settings->Zones,Settings->NoOfZones))
		{
			return(0);
		}
	}
	if( Settings->NoOfZones > 8 )
	{
		if( ! iZoneReadZoneData("Zones9_12",Settings->cb_ip,Settings->Zones,Settings->NoOfZones))
		{
			return(0);
		}
	}

	return(1);
}


/* This will try to get the izones CB's IP information */
/* This can be done by sending a UDP packet containing "IASD" */
/* to 255.255.255.255:12107, and waiting for the response */
/* return 0 on fail, or 1 on success */
int iZoneFindCB(char *IP, int *Port, int timeout)
{
	int result;		/* Somewhere to put the result */

	struct sockaddr_in servaddr, listener;
	int sock;
	char buf[100];
	int recv_len;
	int slen;
	int x;

	int res;
	fd_set rfds;
	struct timeval tv;

	char *ASPort;
	char *CB_IP;


	IP[0]=0;
	*Port=0;


	/* create the socket */
	sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
   if(sock<0)
	{
   	//perror("cannot open socket");
   	return(0); 
   }

	/* Setup a listener to catch the response */
	bzero(&listener, sizeof(servaddr));
	listener.sin_family=AF_INET;
	listener.sin_port = htons(12107);
	listener.sin_addr.s_addr=htonl(INADDR_ANY);

	if( bind( sock, (struct sockaddr*)&listener, sizeof(listener)) == -1)
	{
		//perror("Can't bind to listerner");
		close(sock);
		return(0);
	}	

	// Now send the trigger packet
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("255.255.255.255");
	servaddr.sin_port = htons(12107);

	// Enable sending to broadcast address
	int broadcastPermission;
	broadcastPermission = 1;
   if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
	{
		//perror("Setting broadcast permission");
		close(sock);
		return(0);
	}

	if (sendto(sock, "IASD", 5, 0, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
       //perror("cannot send message");
       close(sock);
       return(0);
   }

	slen=sizeof(servaddr);
	
   // Now wait for the response
   while(1)
   {
      //printf("Waiting for data...");
      //fflush(stdout);

		// Wait for a response, with a timeout
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec=timeout;
		tv.tv_usec=0;

		res=select(sock+1, &rfds, NULL, NULL, &tv);
		if( res == -1)
		{
			//perror("Select");
			close(sock);
			return(0);
		}
		else if( res==0)
		{
			//printf("Timeout\n");
			close(sock);
			return(0);
		}
      else
		{  
      	//try to receive some data, this is a blocking call
      	if ((recv_len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &servaddr, &slen)) == -1)
      	{
        	 	//perror("Problem recieving data\n");
				close(sock);
				return(0);
      	}
			buf[recv_len]=0;	// NULL terminate the string
        
      	//print details of the client/peer and the data received
      	//printf("Received packet from %s:%d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
      	//printf("Data: %s\n" , buf);

			if( recv_len > 6)  // Less than 6 then to small to be anything we are interested in
			{
				ASPort=strstr( buf, "ASPort");
				if( ASPort )	// If it has ASPORT then it is the line we are after
				{
					*Port=atoi( ASPort+7);

					CB_IP=strstr(buf, "IP_");
					if( ! CB_IP )		// This shouldn't happen, if it does use the sourec IP instead
					{
						strcpy(IP, inet_ntoa(servaddr.sin_addr));
					}
					else
					{
						CB_IP+=3;	// Point at IP address
						for(x=0; CB_IP[x]!=',' && CB_IP[x]!='\0' && x<16; x++)
						{
							IP[x]=CB_IP[x];
						}
						IP[x]=0;
					}
					break;
				}
			}
		}
   }

	close(sock);
	return(1);
}

// This will setip the socket for monitoring the broadcast
// signal from the CB on 255.255.255.255 port 7005.
// return the socket number is successful or -1 if not
int iZoneInitMonitor()
{
   int result;    /* Somewhere to put the result */

   struct sockaddr_in servaddr, listener;
   int sock;
   char buf[100];
   int recv_len;
   int slen;
   int x;

   int res;
   fd_set rfds;
   struct timeval tv;

   char *ASPort;
   char *CB_IP;

   /* create the socket */
   sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
   if(sock<0)
   {
      //perror("cannot open socket");
      return(-1);
   }

   /* Setup a listener to catch the response */
   bzero(&listener, sizeof(listener));
   listener.sin_family=AF_INET;
   listener.sin_port = htons(7005);
   listener.sin_addr.s_addr=htonl(INADDR_ANY);

   if( bind( sock, (struct sockaddr*)&listener, sizeof(listener)) == -1)
   {   
      //perror("Can't bind to listerner");
      close(sock);
      return(-1);
   }   

   return(sock);
}


int MQTT_SendFloat(struct mosquitto *mosq, char *Topic, float Value)
{
	int len;
   char buf[20];

	len=sprintf(buf,"%2.1f",Value);
	mosquitto_publish( mosq, NULL, Topic,len ,buf, 0, 1);

	return(1);
}

int MQTT_SendZoneFloat(struct mosquitto *mosq, char *Topic, int Zone, float Value)
{
   int len;
   char buf[20];
	char Top[40];

	sprintf(Top, Topic, Zone);
   len=sprintf(buf,"%2.1f",Value);
   mosquitto_publish( mosq, NULL, Top,len ,buf, 0, 1);

   return(1);
}


int MQTT_SendZoneInt(struct mosquitto *mosq, char *Topic, int Zone, int Value)
{

	char Top[40];
	int len;
	char buf[20];

	sprintf(Top, Topic, Zone);
	len=sprintf(buf,"%d", Value);
	mosquitto_publish( mosq, NULL, Top,len ,buf, 0, 1);

	return(1);
}

int MQTT_SendString(struct mosquitto *mosq, char *Topic, char *Text)
{
   int len;
   char buf[20];

   len=strlen(Text);
   mosquitto_publish( mosq, NULL, Topic,len ,Text, 0, 1);

   return(1);
}

int MQTT_SendZoneName(struct mosquitto *mosq,int Zone, char *Name)
{
	int len;
	char Topic[40];

	sprintf(Topic,"iZone/zone/%d/name", Zone);
	len=strlen(Name);
	
	mosquitto_publish(mosq, NULL, Topic, len, Name, 0, 1);
	return(1);
}

int MQTT_SendState(struct mosquitto *mosq, int state)
{
 	int len;
	char buf[20];

	len=sprintf(buf,"%d",state);
 	mosquitto_publish( mosq, NULL, "iZone/main/state",len ,buf, 0, 1);

	return(1);
}

int MQTT_SendMode(struct mosquitto *mosq, int Mode)
{
 	int len;
	char buf[20];

	switch( Mode)
	{
		case 0:	mosquitto_publish( mosq, NULL, "iZone/main/mode",4,"cool",0,1);	break;
		case 1:	mosquitto_publish( mosq, NULL, "iZone/main/mode",4,"heat",0,1);	break;
		case 2:	mosquitto_publish( mosq, NULL, "iZone/main/mode",4,"vent",0,1);	break;
		case 3:	mosquitto_publish( mosq, NULL, "iZone/main/mode",3,"dry",0,1);		break;
		case 4:	mosquitto_publish( mosq, NULL, "iZone/main/mode",4,"auto",0,1);	break;
		deafult:	break;
	}

	return(1);
}

int MQTT_SendFan(struct mosquitto *mosq, int Fan)
{
   int len;
   char buf[20];

   switch( Fan)
   {
      case 0:  mosquitto_publish( mosq, NULL, "iZone/main/fan",3,"low",0,1);   break;
      case 1:  mosquitto_publish( mosq, NULL, "iZone/main/fan",3,"med",0,1);   break;
      case 2:  mosquitto_publish( mosq, NULL, "iZone/main/fan",4,"high",0,1);   break;
      case 3:  mosquitto_publish( mosq, NULL, "iZone/main/fan",4,"auto",0,1);    break;
      deafult: break;
   }

   return(1);
}

int MQTT_SendZoneMode(struct mosquitto *mosq,int Zone, int Mode)
{
   int len;
   char buf[20];

	sprintf(buf,"iZone/zone/%d/mode",Zone); 
   switch( Mode)
   {
      case 0:  mosquitto_publish( mosq, NULL, buf, 5,"close",0,1);   break;
      case 1:  mosquitto_publish( mosq, NULL, buf, 4,"open",0,1);   break;
      case 3:  mosquitto_publish( mosq, NULL, buf, 4,"auto",0,1);   break;
      deafult: break;
   }

   return(1);
}

// Send the initial states for everything, to make sure everything is
// in sync
int MQTT_SendCurrentStates(struct mosquitto *mosq,struct SystemSettingsStruct *Settings)
{
	int x;

	mosquitto_publish( mosq, NULL, "iZone/connection",6 ,"online", 0, 1);  // Send we are online

   MQTT_SendState(mosq, Settings->SysOn);
   MQTT_SendMode(mosq, Settings->SysMode);
   MQTT_SendFan(mosq, Settings->SysFan);
   MQTT_SendFloat(mosq, "iZone/main/DuctTemperature", Settings->Supply);
   MQTT_SendFloat(mosq, "iZone/main/TemperatureSetting", Settings->Setpoint);
   MQTT_SendFloat(mosq, "iZone/main/Temperature", Settings->Temp);
   MQTT_SendString(mosq, "iZone/main/error", Settings->ACError);


   // Zones are the same so best check them
   for(x=0; x<Settings->NoOfZones; x++)
   {
      MQTT_SendZoneName(mosq, x, Settings->Zones[x].Name);
   	MQTT_SendZoneMode(mosq, x, Settings->Zones[x].Mode);
      MQTT_SendZoneFloat(mosq, "iZone/zone/%d/TemperatureSetting", x, Settings->Zones[x].SetPoint);
      MQTT_SendZoneFloat(mosq, "iZone/zone/%d/Temperature", x, Settings->Zones[x].Temp);
     	MQTT_SendZoneInt(mosq, "iZone/zone/%d/AirMin", x, Settings->Zones[x].MinAir);
     	MQTT_SendZoneInt(mosq, "iZone/zone/%d/AirMax", x, Settings->Zones[x].MaxAir);
   }
	return(1);
}


/* Compares the current settings against a new set of settings, if there is a difference it will */
/* update the current settings, and also send out MQTT signals to indicate the change */
/* returns 1 if ok or 0 if something wrong */
/* I could have updated the current settings from Newsettings with just a memcpy, but I */
/* didn't feel lik it */
int iZoneCompareUpdate(struct SystemSettingsStruct *CurrSettings, struct SystemSettingsStruct *NewSettings, struct mosquitto *mosq)
{
	int Zone;
	int x;

	if( strcmp(CurrSettings->cb_ip, NewSettings->cb_ip) != 0)
	{
		strcpy( CurrSettings->cb_ip, NewSettings->cb_ip);
		CurrSettings->cb_port=NewSettings->cb_port;
	}
	if( CurrSettings->SysOn != NewSettings->SysOn)
	{
		MQTT_SendState(mosq, NewSettings->SysOn);
		CurrSettings->SysOn = NewSettings->SysOn;
	}
	if( CurrSettings->SysMode != NewSettings->SysMode)
	{
		MQTT_SendMode(mosq, NewSettings->SysMode);
		CurrSettings->SysMode = NewSettings->SysMode;
	}

	if( CurrSettings->SysFan != NewSettings->SysFan)
	{
		MQTT_SendFan(mosq, NewSettings->SysFan);
		CurrSettings->SysFan = NewSettings->SysFan;
	}

	if( CurrSettings->SleepTimer != NewSettings->SleepTimer)
	{
		CurrSettings->SleepTimer = NewSettings->SleepTimer;
	}

	if( CurrSettings->Supply != NewSettings->Supply)
	{
		MQTT_SendFloat(mosq, "iZone/main/DuctTemperature", NewSettings->Supply);
		CurrSettings->Supply = NewSettings->Supply;
	}
	if( CurrSettings->Setpoint != NewSettings->Setpoint)
	{
		MQTT_SendFloat(mosq, "iZone/main/TemperatureSetting", NewSettings->Setpoint);
		CurrSettings->Setpoint = NewSettings->Setpoint;
	}
	if( CurrSettings->Temp != NewSettings->Temp)
	{
		MQTT_SendFloat(mosq, "iZone/main/Temperature", NewSettings->Temp);
		CurrSettings->Temp = NewSettings->Temp;
	}

	if( CurrSettings->RAS != NewSettings->RAS)
	{
		CurrSettings->RAS = NewSettings->RAS;
	}
	if( CurrSettings->CtrlZone != NewSettings->CtrlZone)
	{
		CurrSettings->CtrlZone = NewSettings->CtrlZone;
	}

	if( strcmp(CurrSettings->ACError,NewSettings->ACError) != 0)
	{
		MQTT_SendString(mosq, "iZone/main/error", NewSettings->ACError);
		strcpy(CurrSettings->ACError,NewSettings->ACError);
	}
	

	if( CurrSettings->EcoLock != NewSettings->EcoLock)
   {
      CurrSettings->EcoLock = NewSettings->EcoLock;
   }
	if( CurrSettings->EcoMax != NewSettings->EcoMax)
   {
      CurrSettings->EcoMax = NewSettings->EcoMax;
   }
	if( CurrSettings->EcoMin != NewSettings->EcoMin)
   {
      CurrSettings->EcoMin = NewSettings->EcoMin;
   }
	if( CurrSettings->NoOfConst != NewSettings->NoOfConst)
   {
      CurrSettings->NoOfConst = NewSettings->NoOfConst;
   }

	if( CurrSettings->NoOfZones != NewSettings->NoOfZones)
   {
      CurrSettings->NoOfZones = NewSettings->NoOfZones;  // How did this change..
		// Just copy the zone info across and hope for the best.
		for(x=0; x < NewSettings->NoOfZones; x++)
		{
			memcpy(&CurrSettings->Zones[x], &NewSettings->Zones[x], sizeof( struct ZoneStruct) );
		}
   }
	else
	{
		// Zones are the same so best check them
		for(x=0; x<NewSettings->NoOfZones; x++)
		{
			if( CurrSettings->Zones[x].Type != NewSettings->Zones[x].Type)
			{
				CurrSettings->Zones[x].Type = NewSettings->Zones[x].Type;
			}
			if( CurrSettings->Zones[x].Mode != NewSettings->Zones[x].Mode)
			{
				MQTT_SendZoneMode(mosq, x, NewSettings->Zones[x].Mode);
				CurrSettings->Zones[x].Mode = NewSettings->Zones[x].Mode;
			}
			if( CurrSettings->Zones[x].SetPoint != NewSettings->Zones[x].SetPoint)
			{
				MQTT_SendZoneFloat(mosq, "iZone/zone/%d/TemperatureSetting", x, NewSettings->Zones[x].SetPoint);
				CurrSettings->Zones[x].SetPoint = NewSettings->Zones[x].SetPoint;
			}
			if( CurrSettings->Zones[x].Temp != NewSettings->Zones[x].Temp)
			{
				MQTT_SendZoneFloat(mosq, "iZone/zone/%d/Temperature", x, NewSettings->Zones[x].Temp);
				CurrSettings->Zones[x].Temp = NewSettings->Zones[x].Temp;
			}
			


			if( CurrSettings->Zones[x].MaxAir != NewSettings->Zones[x].MaxAir)
			{
      		MQTT_SendZoneInt(mosq, "iZone/zone/%d/AirMax", x, NewSettings->Zones[x].MaxAir);
				CurrSettings->Zones[x].MaxAir = NewSettings->Zones[x].MaxAir;
			}
			if( CurrSettings->Zones[x].MinAir != NewSettings->Zones[x].MinAir)
			{
      		MQTT_SendZoneInt(mosq, "iZone/zone/%d/AirMin", x, NewSettings->Zones[x].MinAir);
				CurrSettings->Zones[x].MinAir = NewSettings->Zones[x].MinAir;
			}
			if( CurrSettings->Zones[x].Const != NewSettings->Zones[x].Const)
			{
				CurrSettings->Zones[x].Const = NewSettings->Zones[x].Const;
			}
			if( CurrSettings->Zones[x].ConstA != NewSettings->Zones[x].ConstA)
			{
				CurrSettings->Zones[x].ConstA = NewSettings->Zones[x].ConstA;
			}

		}
	}

	if( CurrSettings->AirflowLock != NewSettings->AirflowLock)
   {
      CurrSettings->AirflowLock = NewSettings->AirflowLock;
   }
	if( CurrSettings->UnitLocked != NewSettings->UnitLocked)
   {
      CurrSettings->UnitLocked = NewSettings->UnitLocked;
   }
	if( CurrSettings->FreeAir != NewSettings->FreeAir)
   {
      CurrSettings->FreeAir = NewSettings->FreeAir;
   }
	if( CurrSettings->FanAuto != NewSettings->FanAuto)
   {
      CurrSettings->FanAuto = NewSettings->FanAuto;
   }
	

	return(1);
}


/* The select has detected something on the broadcast listener */
/* So this will read it. Basically it will jusr reread the settings */
/* compare against the existing ones  and send an MQTT signall if any */
/* have changed. Return 1 if all is ok or 0 is something went wrong */
int iZoneBroadcastRead( struct mosquitto *mosq, int MonBroadcast, struct SystemSettingsStruct *Settings)
{
	int recv_len;
	char buf[100];
	int slen;
	struct sockaddr_in servaddr;
	struct SystemSettingsStruct NewSettings;
	int res;

   // Initialise the atructure to get the address info in
   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = inet_addr("255.255.255.255");
   servaddr.sin_port = htons(7005);
   slen=sizeof(servaddr);

	/* Read from the socket to stop select triggering again */
	recv_len=recvfrom( MonBroadcast, buf, sizeof(buf), 0, (struct sockaddr *) &servaddr, &slen);
	if( recv_len <=0 )
	{
		printf("Error reading broadcast message\n");
		return(0);
	}
	buf[recv_len]=0;  // NULL terminate the string
	if( strncmp(buf, "iZoneChanged", 12) == 0)	// Check it is from te CB
	{
		strcpy(NewSettings.cb_ip, Settings->cb_ip);
		NewSettings.cb_port=Settings->cb_port;
   	res=iZoneGetSettings(&NewSettings);    // Now read the current settings
   	if( !res )
   	{
      	printf("Could not get iZone information\n");
      	return(0);
   	}
		res=iZoneCompareUpdate(Settings, &NewSettings, mosq);
		if( !res )
		{
			printf("Error sending updates\n");
			return(0);
		}
	}

	return(1);
}

/* This is a little routine for listening for the broadcast messages */
/* from the iZone CB. These are sent to 255.255.255.255 port 7005,   */
/* the payload will indicate what has changed */
int iZoneMonitorBroadcasts()
{
   int result;    /* Somewhere to put the result */

   struct sockaddr_in servaddr, listener;
   int sock;
   char buf[100];
   int recv_len;
   int slen;
   int x;

   int res;
   fd_set rfds;
   struct timeval tv;

   char *ASPort;
   char *CB_IP;

   /* create the socket */
   sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
   if(sock<0)
   {    
      //perror("cannot open socket");
      return(0); 
   }

   /* Setup a listener to catch the response */
   bzero(&listener, sizeof(listener));
   listener.sin_family=AF_INET;
   listener.sin_port = htons(7005);
   listener.sin_addr.s_addr=htonl(INADDR_ANY);

   if( bind( sock, (struct sockaddr*)&listener, sizeof(listener)) == -1)
   {    
      //perror("Can't bind to listerner");
      close(sock);
      return(0);
   }    

   // Now send the trigger packet
   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = inet_addr("255.255.255.255");
   servaddr.sin_port = htons(12107);

   // Enable sending to broadcast address
   //int broadcastPermission;
   //broadcastPermission = 1; 
   //if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
   //{    
   //   //perror("Setting broadcast permission");
   //   close(sock);
   //   return(0);
   //}    


   slen=sizeof(servaddr);

   // Now wait for the response
   while(1)
   {
      //try to receive some data, this is a blocking call
      if ((recv_len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &servaddr, &slen)) == -1)
      {
         //perror("Problem recieving data\n");
         close(sock);
         return(0);
      }
      buf[recv_len]=0;  // NULL terminate the string

     // print details of the client/peer and the data received
     printf("Received packet from %s:%d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
     printf("Data: %s\n" , buf);
   }

   close(sock);
   return(1);
}



/**************************/
/* Commands to iZone      */
/**************************/

// Call back function for request_url function. just copies
// the data to the buffer
int iZoneSendCmd_Callback(void *ptr, int size, int nmemb, char *data)
{
   int RealSize;

   printf("Size=%d, nmemb=%d\n", size, nmemb);
   printf("Data=%s\n", ptr);

   RealSize= size * nmemb;
   memcpy(data, ptr, RealSize+1);

   return( RealSize);
}

/* Sends the command to the controller and checks the result */
/* return 1 if all ok or 0 if an error */
int iZoneSendCmd(char *IP, char *Path, char *Data)
{
   CURL *curl;
   CURLcode status;
   long code;
	struct curl_slist *headers = NULL;

	char buffer[2000];
	char url[200];


	sprintf(url,"http://%s/%s", IP, Path);	// Build URL

   curl_global_init(CURL_GLOBAL_ALL);
   curl=curl_easy_init();
   if( ! curl)
   {
      curl_global_cleanup();
      return(0);
   }

	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Data);

   curl_easy_setopt(curl, CURLOPT_URL, url);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, iZoneSendCmd_Callback);  // Don't care what is returned, but capture just in case
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
   status = curl_easy_perform(curl);
   if( status != 0)
   {
      curl_easy_cleanup(curl);
		curl_slist_free_all(headers);
      curl_global_cleanup();
      return(0);
   }

   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
   if( code != 200)
   {
      curl_easy_cleanup(curl);
		curl_slist_free_all(headers);
      curl_global_cleanup();
      return(0);
   }

   curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
   curl_global_cleanup();

   return(1);
}


/* This will turn the Airconditioner On or Off */
/* Return 1 is successful or 0 if an error */
/* Also will update the current settings */
int iZoneState(int State, struct SystemSettingsStruct *Settings, struct mosquitto *mosq )
{
	char *data;
	int res;

	if( State )
		data="{ \"SystemON\":\"on\" }";
	else
		data="{ \"SystemON\":\"off\" }";

	res=iZoneSendCmd(Settings->cb_ip, "SystemON", data);
	if( res )
	{
		Settings->SysOn=State;
   	MQTT_SendState(mosq, State);
	}

	return(res);
}

/* This will set the mode */
/* 0=cool, 1=heat, 2=vent, 3=dry, 4=auto */
/* Return 1 on success or 0 on fail */
int iZoneMode(int Mode, struct SystemSettingsStruct *Settings,struct mosquitto *mosq  )
{
	char *data;
	int res;

	switch( Mode )
	{
		case 0:	data="{ \"SystemMODE\":\"cool\" }";		break;
		case 1:	data="{ \"SystemMODE\":\"heat\" }";		break;
		case 2:	data="{ \"SystemMODE\":\"vent\" }";		break;
		case 3:	data="{ \"SystemMODE\":\"dry\" }";		break;
		case 4:	data="{ \"SystemMODE\":\"auto\" }";		break;
		default: return(0);
	}
	res=iZoneSendCmd(Settings->cb_ip, "SystemMODE", data);
	if( res)
	{
		Settings->SysMode=Mode;
   	MQTT_SendMode(mosq, Mode);
	}

	return(res);
}

/* This will set the fan speed */
/* 0=low, 1=med, 2=high, 3=auto */
/* Return 1 on success or 0 on fail */
int iZoneFan(int Speed, struct SystemSettingsStruct *Settings, struct mosquitto *mosq  )
{
	char *data;
	int res;

	switch( Speed )
	{
		case 0:	data="{ \"SystemFAN\":\"low\" }";		break;
		case 1:	data="{ \"SystemFAN\":\"medium\" }";		break;
		case 2:	data="{ \"SystemFAN\":\"high\" }";		break;
		case 3:	data="{ \"SystemFAN\":\"auto\" }";		break;
		default: return(0);
	}
	res=iZoneSendCmd(Settings->cb_ip, "SystemFAN", data);
	if( res)
	{
		Settings->SysFan=Speed;
   	MQTT_SendFan(mosq, Speed);
	}

	return(res);
}

/* This will set the  AC setpoint */
/* Return 1 on success or 0 on fail */
int iZoneSetpoint(float Setpoint, struct SystemSettingsStruct *Settings, struct mosquitto *mosq  )
{
	char data[80];
	int res;

	sprintf(data,"{ \"UnitSetpoint\":\"%2.1f\" }", Setpoint);

	res=iZoneSendCmd(Settings->cb_ip, "UnitSetpoint", data);
	if( res)
	{
		Settings->Setpoint=Setpoint;
   	MQTT_SendFloat(mosq, "iZone/main/TemperatureSetting", Setpoint);
	}

	return(res);
}


/* This will change the zones  current mode to open or closed */
int iZoneZoneMode( int Zone, int Mode, struct SystemSettingsStruct *Settings,struct mosquitto *mosq )
{
   char data[80];
   int res;

   sprintf(data,"{ \"ZoneCommand\": { \"ZoneNo\":\"%d\",\"Command\":\"%s\" } }", Zone+1, Mode ? "open" : "close" );

   res=iZoneSendCmd(Settings->cb_ip, "ZoneCommand", data);
   if( res)
	{
      Settings->Zones[Zone].Mode=Mode;
   	MQTT_SendZoneMode(mosq, Zone,Mode);		// Send the same thing back on MQTT to reflect the change
		// printf("Successfuly sent %s\n", data);
	}
	else
	{
		printf("Didn't set the zone with %s\n", data);
	}

   return(res);
}

/* This will change the zones Setpoint */
int iZoneZoneSetpoint( int Zone, float Setpoint, struct SystemSettingsStruct *Settings, struct mosquitto *mosq )
{
   char data[80];
   int res;

   sprintf(data,"{ \"ZoneCommand\": { \"ZoneNo\":\"%d\",\"Command\":\"%2.1f\" } }", Zone+1, Setpoint );

   res=iZoneSendCmd(Settings->cb_ip, "ZoneCommand", data);
   if( res)
	{
      Settings->Zones[Zone].SetPoint=Setpoint;
		MQTT_SendZoneFloat(mosq, "iZone/zone/%d/TemperatureSetting", Zone, Setpoint);
	}

   return(res);
}

/* This will change the Minimum airflow */
int iZoneZoneAirMin( int Zone, int MinAir, struct SystemSettingsStruct *Settings, struct mosquitto *mosq )
{
   char data[80];
   int res;

   sprintf(data,"{ \"AirMinCommand\": { \"ZoneNo\":\"%d\",\"Command\":\"%d\" } }", Zone+1, MinAir );

   res=iZoneSendCmd(Settings->cb_ip, "AirMinCommand", data);
   if( res)
   {
      Settings->Zones[Zone].MinAir=MinAir;
      MQTT_SendZoneInt(mosq, "iZone/zone/%d/AirMin", Zone, MinAir);
   }

   return(res);
}

/* This will change the Maximum airflow */
int iZoneZoneAirMax(int Zone, int MaxAir, struct SystemSettingsStruct *Settings, struct mosquitto *mosq )
{
   char data[80];
   int res;

   sprintf(data,"{ \"AirMaxCommand\": { \"ZoneNo\":\"%d\",\"Command\":\"%d\" } }", Zone+1, MaxAir );

   res=iZoneSendCmd(Settings->cb_ip, "AirMaxCommand", data);
   if( res)
	{
      Settings->Zones[Zone].MaxAir=MaxAir;
      MQTT_SendZoneInt(mosq, "iZone/zone/%d/AirMax", Zone, MaxAir);
	}

   return(res);
}


/*******************************/
/** MQTT stuff                **/
/*******************************/

/* A set for the main unit, so best process it */
int HandleMQTTSetMain( char *What, char *Value, struct SystemSettingsStruct *Settings, struct mosquitto *mosq)
{
	int val;
	float fval;
	
	if( strcmp(What, "mode") == 0)	// Set the mode
	{
		if( strcmp(Value, "cool")==0)	val=0;
		else if(strcmp(Value, "heat") ==0)	val=1;
		else if(strcmp(Value, "vent") ==0)	val=2;
		else if(strcmp(Value, "dry") ==0)	val=3;
		else if(strcmp(Value, "auto") ==0)	val=4;
		else
		{
			val=-1;
			printf("got a set/main/mode with an unknown value - %s\n", Value);
		}
		if( val != -1)
		{
			iZoneMode(val, Settings, mosq );
		}
	}
	else if( strcmp(What, "fan") == 0)	// set fan mode
	{
		if( strcmp(Value, "low")==0)	val=0;
		else if(strcmp(Value, "med") ==0)	val=1;
		else if(strcmp(Value, "high") ==0)	val=2;
		else if(strcmp(Value, "auto") ==0)	val=3;
		else
		{
			val=-1;
			printf("got a set/main/fan with an unknown value - %s\n", Value);
		}
		if( val != -1)
		{
			iZoneFan(val, Settings, mosq );
		}
	}
	else if( strcmp( What, "TemperatureSetting") == 0)		// Set Setpoint
	{
		fval=atof(Value);
		if( fval>0 && fval<50)
		{
			iZoneSetpoint( fval, Settings, mosq);
		}
		else
			printf("got a set/main/TemperatureSetting with an invaliud value of %s\n", Value);
	}
	else if( strcmp( What, "state") == 0 )		// Turn on or off
	{
		val=atoi(Value);
		if( val )		// Convert val to a 0 or 1 value	
			val=1;
		iZoneState( val, Settings, mosq);
	}
	else	// Don't know what it is
	{
		printf("Unknown set received - %s\n", What);
	}
	return(1);
}

int HandleMQTTSetZone(char *ZoneWhat, char *Value,struct SystemSettingsStruct *Settings, struct mosquitto *mosq)
{
	int Zone;
	char *What;
	int val;
	int fval;

	Zone=atoi(ZoneWhat);
	if( Zone >= Settings->NoOfZones )
	{
		printf("Got a zone command, but given zone number is to high\n");
		return(0);
	}

	What=strchr(ZoneWhat, '/');
	if( What==NULL)
	{
		printf("Got a zone command, but with no command\n");
		return(0);
	}
	What++;
	if( *What == 0)
	{
		printf("Got a zone command, but with no command\n");
		return(0);
	}
	if( strcmp( What, "mode")==0)
	{
		if( strcmp(Value, "open")==0)	val=1;
		else if(strcmp(Value, "close") ==0)	val=0;
		else
		{
			val=-1;
			printf("got a set/zone/%d/mode with an unknown value - %s\n",Zone, Value);
		}
		if( val != -1)
		{
			iZoneZoneMode(Zone, val, Settings, mosq );
		}
	}
	else if( strcmp( What, "TemperatureSetting") == 0)
	{
		fval=atof(Value);
		if( fval>0 && fval<50)
		{
			iZoneZoneSetpoint( Zone, fval, Settings, mosq);
		}
		else
			printf("got a set/zone/%d/TemperatureSetting with an invaliud value of %s\n",Zone, Value);
	}
	else if( strcmp( What, "MinimumAitflow") == 0)
	{
		val=atoi(Value);
		if( val>=0 && val<=100)
		{
			iZoneZoneAirMin( Zone, val, Settings, mosq);
		}
		else
			printf("got a set/zone/%d/MinimumAitflow with an invaliud value of %s\n",Zone, Value);
	}
	else if( strcmp( What, "MaximumAirflow") == 0)
	{
		val=atoi(Value);
		if( val>=0 && val<=100)
		{
			iZoneZoneAirMax( Zone, val, Settings, mosq);
		}
		else
			printf("got a set/zone/%d/MinimumAitflow with an invaliud value of %s\n",Zone, Value);

	}
	else
	{
		printf("Unknown Zone set received - %s\n", What);
	}
	return(1);
}



/* This is called when a MQTT message has been received */
void MqttMessageCallback( struct mosquitto *mosq, void *Settings, const struct mosquitto_message *message)
{
	if(message->payloadlen)
	{
		if( strncmp(message->topic, "iZone/set/main/", 15) == 0)
			HandleMQTTSetMain( message->topic+15, message->payload, Settings, mosq);
		else if( strncmp(message->topic, "iZone/set/zone/", 15)==0)
			HandleMQTTSetZone(message->topic+15, message->payload, Settings, mosq);
		else
			printf("MqttMessageCallback: %s %s\n", message->topic, message->payload);
	}
	else	
	{
		printf("MqttMessageCallback: %s (null)\n", message->topic);
	}
	fflush(stdout);
	return;
}


/* This is called when the broker responds to the subscribe request */
void MqttSubscribeCallback( struct mosquitto *mosq, void *Settings, int mid, int qos_count, const int *granted_qos )
{
	int i;

	printf("MqttSubscribeCallback: Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
	return;
}

/* This is called when the broker responds to a connection attemppt */
/* result. 0=success, 1=bad version, 2-bad identifier, 3-broker unavailable */
void MqttConnectCallback(struct mosquitto *mosq, void *Settings, int result)
{
	int res;

	if(!result)
	{
		// Sucessfully connected, so subscribe to topics.
		mosquitto_subscribe(mosq, NULL, "iZone/get/#", 2);
		mosquitto_subscribe(mosq, NULL, "iZone/set/#", 2);

		// Publish the connected message
		MQTT_SendCurrentStates(mosq, Settings);	// Send all the current states
	}
	else
	{
		printf("MqttLogCallback: error connecting- %d\n", result);
	}
}


/* This is called when the MQTT utils want to log something */
void MqttLogCallback(struct mosquitto *mosq, void *Settings, int level, const char *str)
{
	printf("MqttLogCallback: Level=%d, %s\n", level, str);
}


// This will dump the data to check it is right
int DumpData(struct SystemSettingsStruct *Settings)
{
	int x;

	printf("AirStreamDeviceUId=%s\n", Settings->AirStreamDeviceUId);
	printf("DeviceType=%s\n", Settings->DeviceType);
	printf("Syson=%d\nSysMode=%d\nSysFan=%d\nSleepTimer=%d\n",Settings->SysOn,Settings->SysMode,Settings->SysFan,Settings->SleepTimer);
	printf("UnitType=%s\n", Settings->UnitType);
	printf("Supply=%f\n", Settings->Supply);
	printf("Setpoint=%f\n", Settings->Setpoint);
	printf("Temp=%f\n", Settings->Temp);
	printf("RAS=%d\n",Settings->RAS);
	printf("CtrlZone=%d\n",Settings->CtrlZone);
	printf("Tag1=%s\n", Settings->Tag1);
	printf("Tag2=%s\n", Settings->Tag2);
	printf("Warnings=%s\n", Settings->Warnings);
	printf("ACError=%s\n", Settings->ACError);
	printf("Id=%d\n",Settings->Id);
	printf("EcoLock=%d\n",Settings->EcoLock);
	printf("EcoMax=%f\n", Settings->EcoMax);
	printf("EcoMin=%f\n", Settings->EcoMin);
	printf("NoOfConst=%d\n",Settings->NoOfConst);
	printf("NoOfZones=%d\n",Settings->NoOfZones);
	printf("SysType=%s\n", Settings->SysType);
	printf("AirflowLock=%d\n",Settings->AirflowLock);
	printf("UnitLocked=%d\n",Settings->UnitLocked);
	printf("FreeAir=%d\n",Settings->FreeAir);
	printf("FanAuto=%d\n",Settings->FanAuto);


	for(x=0; x < Settings->NoOfZones; x++)
	{
		printf("==Zone info for Zone %d\n", x);
		printf("  AirStreamDeviceUId=%s\n", Settings->Zones[x].AirStreamDeviceUId);
		printf("  Id=%d\n",Settings->Zones[x].Id);
		printf("  Index=%d\n",Settings->Zones[x].Index);
		printf("  Name=%s\n", Settings->Zones[x].Name);
		printf("  Type=%d\n",Settings->Zones[x].Type);
		printf("  Mode=%d\n",Settings->Zones[x].Mode);
		printf("  SetPoint=%f\n", Settings->Zones[x].SetPoint);
		printf("  Temp=%f\n", Settings->Zones[x].Temp);
		printf("  MaxAir=%d\n",Settings->Zones[x].MaxAir);
		printf("  MinAir=%d\n",Settings->Zones[x].MinAir);
		printf("  Const=%d\n",Settings->Zones[x].Const);
		printf("  ConstA=%d\n",Settings->Zones[x].ConstA);
	}
	return(0);
}

/* This is the main loop, it is called once everything is setup */
/* it will wait for something to turn up from the iZone controller */
/* or the MQTT broker, and take appropiate action                  */
void MainLooop( struct mosquitto *mosq, int MonBroadcast, struct SystemSettingsStruct *Settings)
{
	int mosq_fd;
	fd_set rfds;
	int nfd;
	int res, ret;
	struct timeval tv;	// Used to force a timeout


	mosq_fd=mosquitto_socket(mosq);

	while(1)
	{
		tv.tv_sec=5;	// Set timeout
		tv.tv_usec=0;

		FD_ZERO(&rfds);
		FD_SET( MonBroadcast, &rfds);
		FD_SET( mosq_fd, &rfds);
		nfd = mosq_fd < MonBroadcast ? MonBroadcast : mosq_fd;
		nfd++;
		res=select( nfd, &rfds, NULL, NULL, &tv);	// Read the file
		if( res == -1)
		{
			printf("I/O Error in select loop\n");
			break;
		}
		else if( res== 0)
		{
			// Do nothing on timeout
		}
		else
		{
			if( FD_ISSET(mosq_fd, &rfds))
			{
				ret=mosquitto_loop_read(mosq,1);
				if( ret ==  MOSQ_ERR_CONN_LOST)
				{
					printf("Disconnected from MQTT broker\n");
					break;
				}
			}
			if( FD_ISSET( MonBroadcast,  &rfds))
			{
				ret=iZoneBroadcastRead(mosq, MonBroadcast, Settings);
				if( ! ret)
				{
					printf("Something wrong reading the Broadcast signal from iZone\n");
					break;
				}
			}
		}
		/* Now do all the general maintenece stuff */
		if (mosquitto_want_write(mosq)) 
		{
			mosquitto_loop_write(mosq, 1);
		}
		mosquitto_loop_misc(mosq);
	}

	return;
}


int main()
{
	int res;
	struct SystemSettingsStruct SystemSettings;
	int MonitorBroadcast;

	struct mosquitto *mosq;


	// Initialise the iZone Stuff
	res=iZoneFindCB( SystemSettings.cb_ip, &SystemSettings.cb_port, 5); // Get the IP address of the CB
	if( ! res )
	{
		printf("Could find the CB address\n");
		exit(-1);
	}

	res=iZoneGetSettings(&SystemSettings);    // Now read the current settings
	if( !res )
	{
		printf("Could not get initial iZone information\n");
		exit(-1);
	}

	MonitorBroadcast=iZoneInitMonitor();	// Initialise the broadcase monitoring
	if( MonitorBroadcast < 0 )
	{
		printf("Could not setup broadcast monitor\n");
		exit(-1);
	}

	// Now initialise the MQTT stuff.
	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, 1, &SystemSettings);
	if( ! mosq )
	{
		printf("Could not initiase an MQTT client\n");
		exit(-1);
	}

	mosquitto_log_callback_set(mosq, MqttLogCallback);
	mosquitto_connect_callback_set(mosq, MqttConnectCallback);
	mosquitto_message_callback_set(mosq, MqttMessageCallback);
	mosquitto_subscribe_callback_set(mosq, MqttSubscribeCallback);
	

	res=mosquitto_will_set(mosq,"iZone/connection",7 ,"offline", 0, 1);
	if( res != MOSQ_ERR_SUCCESS)
	{
		printf("Couldn't set will\n");
		exit(-1);
	}

	res=mosquitto_connect( mosq, "10.0.0.22", 1883, 180);
	if( res != MOSQ_ERR_SUCCESS )	// Connect to the MQTT broker
	{
		printf("Could not connect to MQTT broker\n");
		exit(-1);

	}


	MainLooop( mosq, MonitorBroadcast, &SystemSettings);


	//DumpData(&SystemSettings);
	//printf("Setting mode to dry\n");
	//printf("Result=%d\n", iZoneMode(cb_ip, 3, &SystemSettings));

	//printf("Listening for broadcast messages\n");
	//iZoneMonitorBroadcasts();

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	

	exit(0);
}




