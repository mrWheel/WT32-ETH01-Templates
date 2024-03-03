#include <Arduino.h>
#include <ETH.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>        // part of ESP32 Core https://github.com/ESP32/Arduino

#include "mySetup.h"

#define TZ_Europe_Amsterdam PSTR("CET-1CEST,M3.5.0,M10.5.0/3")

//-- debug(true) or not (false)
bool debugPrint = true;
#define DEBUG if (debugPrint)

/**
 * Define global variables.
*/
WiFiClientSecure client;

char      reqResponse[2048];
char      path[100];
char      body[1024];
char      name[20];

int       requestCounter = 0;
uint32_t  lastID_1, lastID_2, lastID_3;
bool      timeSet = false;


//-----------------------------------------------------------------------------------
/*
 * Make a request to the given host and path.
 * 
 * @param method The HTTP method to use (e.g. GET, POST, PUT, DELETE)
 * @param host The host to make the request to
 * @param path The path to make the request to
 * @param body The body to send with the request (optional)
 * @return The response from the server
*/
const char * execRequest(const char * method, const char * host,  const char * path, const  char * body =  NULL) 
{
  //Serial.println();

  if (client.connect(host, 443)) 
  {
    // Connected! Let's send the request headers.
    //if (p) Serial.printf("-> %s %s HTTP/1.1\r\n", method, path);
    DEBUG Serial.printf("-> %s %s HTTP/1.1\r\n", method, path);
    client.printf("%s %s HTTP/1.1\r\n", method, path);
    DEBUG Serial.printf("-> Host: %s\r\n", host);
    client.printf("Host: %s\r\n", host);
    DEBUG Serial.printf("-> Content-Type: application/json\r\n");
    client.printf("Content-Type: application/json\r\n");
    
    if (body != NULL) 
    {
      // If there's a body, we need to send the content length.
      DEBUG Serial.printf("-> Content-Length: %d\r\n", strlen(body));
      client.printf("Content-Length: %d\r\n", strlen(body));
    }

    DEBUG Serial.printf("-> Connection: close\r\n\r\n");
    client.printf("Connection: close\r\n\r\n");
  
    // Send the request body if there is one.
    if (body != NULL) 
    {
      DEBUG Serial.printf("-> %s\r\n", body);
      client.printf("%s\r\n", body);
    }

    memset(reqResponse, 0, sizeof(reqResponse));
    uint16_t pos = 0;
    while (client.connected()) 
    {
      // We're connected, since the repsonse is short we can read it all at once and return it. 
      // In a real-world scenario, you'd want to read the response in chunks and add it as some sort of buffer.
      // The good news is that the ArduinoJSON library can parse the response directly from the client.
      while(!client.available()) { delay(1); }
      while (client.available())
      {
        char in = (char)client.read();
        if (pos < sizeof(reqResponse) -5 ) 
        {
          if ((in >= ' ' && in <= '~') || in == '\r' || in == '\n')
          {
            reqResponse[pos++] = in;
          }
        }
        
      }
      client.stop();
      return reqResponse;
    }
  }
  
  // If we're here, the connection failed.
  client.stop();
  Serial.println("Connection failed");
  return "";

} //  execRequest()


//-----------------------------------------------------------------------------------
/**
 * Extract the ID from a JSON response.
 * 
 * @param jsonData The JSON data to extract the ID from
 * @return The ID from the JSON data
*/
String extractID(String jsonData) 
{
  int idPos = jsonData.indexOf("\"id\":");
  if (idPos == -1) return ""; // ID key not found

  int start = jsonData.indexOf(':', idPos) + 1;
  int end = jsonData.indexOf(',', start);
  
  // If there's no comma, the ID might be the last element
  if (end == -1) end = jsonData.indexOf('}', start);
  
  // If we still can't find the end, we can't extract the ID.
  if (start == -1 || end == -1) return "";

  // Removes any leading/trailing spaces
  String id = jsonData.substring(start, end);
  id.trim(); 

  // Check and remove double quotes at the start and end
  if (id.startsWith("\"")) id = id.substring(1);
  if (id.endsWith("\"")) id = id.substring(0, id.length() - 1);

  return id;

} //  extractID()

//-----------------------------------------------------------------------------------
/**
 * Connect to the Ethernet network.
*/
void connectEthernet() 
{
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
  while(!ETH.linkUp() || ETH.localIP() == INADDR_NONE) 
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\r\nConnected to the network! @");
  Serial.println(ETH.localIP());

} //  connectEthernet() 


//-----------------------------------------------------------------------------------
void setTimezone(const char *timezone)
{
  Serial.printf("setTimezone():  Setting Timezone to %s\n\n",timezone);
  //-- Now adjust the TZ.  Clock settings are adjusted to show the new local time
  setenv("TZ", timezone, 1);  
  tzset();

} //  setTimezone()

//-----------------------------------------------------------------------------------
bool setupTime(const char *timezone)
{
  struct tm timeinfo;

  Serial.printf("setupTime(): Setting up time for [%s]\r\n", timezone);
  //-- First connect to NTP server, with 0 TZ offset
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com"); 
  if(!getLocalTime(&timeinfo))
  {
    Serial.println("setupTime(): Failed to obtain time");
    return false;
  }
  Serial.println("setupTime(): Got the time from NTP");
  setTimezone(timezone);
  return true;

} //  setupTime()

//-----------------------------------------------------------------------------------
void printTime(bool all=false)
{
  time_t now;
  struct tm timeinfo;

    if (!timeSet)
    { 
      if (setupTime(TZ_Europe_Amsterdam))
      {   
        timeSet = true;
        printTime(true);
      }
      all = true;
    }

  time(&now);
  if (all)
  {
    Serial.printf("printTime(): Now: %ld\n", now);

    gmtime_r(&now, &timeinfo);
    Serial.printf("printTime():        GMT [%04d-%02d-%02d][%02d:%02d:%02d]\r\n"
                        , timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday
                        , timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  localtime_r(&now, &timeinfo);
  Serial.printf("\r\nprintTime(): Local Time [%04d-%02d-%02d][%02d:%02d:%02d]\r\n\n"
                      , timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday
                      , timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

} //  printTime()


//-----------------------------------------------------------------------------------
void startMDNS(const char *Hostname)
{
  MDNS.end(); //-- end service
  Serial.printf("[1] mDNS setup as [%s.local]\r\n", Hostname);
  //-- Start the mDNS responder for Hostname.local
  if (MDNS.begin(Hostname))               
  {
    Serial.printf("[2] mDNS responder started as [%s.local]\r\n", Hostname);
    MDNS.addService("https", "tcp", 443);
  }
  else
  {
    Serial.printf("[3] Error setting up MDNS responder!\r\n");
  }

} // startMDNS()


//-----------------------------------------------------------------------------------
/**
 * Setup the ESP32.
*/
void setup() 
{
  //-- switch off WiFi tranceiver
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.begin(115200);
  Serial.println("Let's begin...");

  //-- Connect to the Ethernet network.
  connectEthernet();
  
  startMDNS("myesp32");

  //-- Set for Amsterdam/NL
  if (setupTime(TZ_Europe_Amsterdam))
  {   
    timeSet = true;
    printTime(true);
  }

  delay(5000);

  //-- Set the client to be insecure so we can make requests to our https server.
  client.setInsecure();

} //  setup() 


//-----------------------------------------------------------------------------------
/**
 * Loop through creating and deleting stuff.
*/
void loop() 
{
  //-- Increase the request counter and create a new product payload.
  requestCounter++;

  if ((requestCounter % 5) == 0) 
  {  
    printTime();
  }

  snprintf(name, sizeof(name),"Artikel #%d", requestCounter);
  int size = random(100);
  int available = random(2);
  snprintf(body, sizeof(body), "{\"name\": \"%s\", \"size\": %d, \"available\": %d}", name, size, available);
  snprintf(path, sizeof(path), API_PATH);

  Serial.printf("Let's post [%s]\r\n", body);
  //-- Make a POST request to create a new product.
  const char * responseBuffer = execRequest("POST", API_SERVER, API_PATH, body);
  DEBUG Serial.printf("\r\nResponse: \n%s\n", responseBuffer);
  
  if (strncasecmp("HTTP/1.1 201 ", responseBuffer, 13) == 0)
  {
    Serial.println("POST OK!\r\n");
  }

  // Let's wait a bit before deleting the product.
  delay(1000);

  // Extract the ID from the response and make a DELETE request to delete the product.
  // We extract the ID using a quick and dirty method, but in a real-world scenario, you'd want to use a JSON library.
  String id = extractID(responseBuffer);

  lastID_3 = lastID_2;
  lastID_2 = lastID_1;
  lastID_1 = id.toInt();
  if (lastID_3 < 1) return;

  Serial.printf("Let's delete product with ID: [%d]\r\n", lastID_3);
  snprintf(path, sizeof(path), "%s/%d", API_PATH, lastID_3);

  const char * deleteResponse1 = execRequest("DELETE", API_SERVER, path);
  DEBUG Serial.printf("\nResponse: \n%s\n", deleteResponse1);

  if (strncasecmp("HTTP/1.1 200 ", deleteResponse1, 13) == 0)
  {
    Serial.println("DELETED OK!");
  }
  Serial.println();

} //  loop() 
