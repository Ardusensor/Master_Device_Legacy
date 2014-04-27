#include <GSM.h>
#include <XBee.h>

// PIN Number
#define PINNUMBER ""

// APN data
#define GPRS_APN       "internet.tele2.ee" //GPRS APN
#define GPRS_LOGIN     "wap"    //GPRS login
#define GPRS_PASSWORD  "wap" //GPRS password

// Initialize the library instances
GSMClient client;
GPRS gprs;
GSM gsmAccess;//(true); 
GSMScanner scannerNetworks;

XBee xbee = XBee();
XBeeResponse response = XBeeResponse();
// create reusable response objects for responses we expect to handle
ZBRxResponse rx = ZBRxResponse();
ZBRxIoSampleResponse ioSample = ZBRxIoSampleResponse();

//Server details
char server[] = "94.246.204.102";
int port = 18151; //18151 for logs, 18150 for data

unsigned long prevUpdate = 10000; //Time since previous update in milliseconds
unsigned long delayTime = 180000; //Time between data uploads in milliseconds
int nrOfTries = 0;
int nrOfSuccess = 0;
int ID = 13;
int nrOfUpdates = 0;
int maxUpdates = 20;

unsigned long timeSinceLastUpload = millis();

String signalStrength;
char buffer[20][35]; //Meta-array of updates, NB maxUpdates!buffer[max collected data][Data max length]
unsigned long bufferTimes[sizeof(buffer)]; //Array of xbee packet recieve times. Has to be as big as buffer[][]
String xbeeAddress[30];

void setup()
{
   Serial1.begin(9600);
   xbee.setSerial(Serial1);
   Serial.begin(9600); //Start serial connection
   Serial.println("Starting up...");
   client.setTimeout(3000); //generic method for Stream class
   scannerNetworks.begin();
}

void loop()
{
  xbee.readPacket();
  if (xbee.getResponse().isAvailable()) {
    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      xbee.getResponse().getZBRxResponse(rx);
       showFrameData();
       uint16_t senderShortAddress = rx.getRemoteAddress16();
       xbeeAddress[nrOfUpdates] = String(senderShortAddress, HEX);
       Serial.print("\n (");
       print16Bits(senderShortAddress);
       Serial.println(")");
       handleXbeeRxMessage(rx.getData(), rx.getDataLength());
       }
  }       
   unsigned long time = millis(); //Time since Arduino boot in milliseconds
     if(time - prevUpdate > delayTime || nrOfUpdates >= maxUpdates){ //Sends data if enough time has elapsed and new data is available
       if(!client.connected()){ //If not connected, connect.
            Serial.println("Starting connections!\n");
            nrOfTries += 1;
            connectGSM();
          }
      
          if (client.connected()){ //If connected, send data.
            Serial.print("\n\nBuffer: "); //Print buffer contents
              for(int i = 0; i < maxUpdates; i++){
                Serial.print(buffer[i]);
		  if(bufferTimes[i] != 0){
                    Serial.print("(");
                    Serial.print(xbeeAddress[i]);
                    Serial.print(";");
                    Serial.print(bufferTimes[i]);
                    Serial.print(")");}
                }
              Serial.println();
            for(int i = 0; i < maxUpdates; i++){ //Send buffer contents
              client.print(buffer[i]);
              if(bufferTimes[i] != 0){
                client.print("(");
                client.print(xbeeAddress[i]);
                client.print(";");
                client.print(bufferTimes[i]);
                client.print(")");}
            }
            
            int voltage = analogRead(A0);
            
            //Send diagnostic information. format: <ID;SIGNAL;VOLTAGE;WHATEVER YOU WANTED>
            client.print("<");
            client.print(ID);
            client.print(";");
            client.print(signalStrength);
            client.print(";");
            client.print(voltage);
            client.print(">");
            Serial.println("Sent!");
            timeSinceLastUpload = millis();
            emptyBuffer();
              
            nrOfUpdates = 0;
            nrOfSuccess += 1; 
      
            Serial.print("Nr of tries: ");
            Serial.println(nrOfTries);
            Serial.print("Nr of successseseses: ");
            Serial.println(nrOfSuccess);
            prevUpdate = time; //Set time of previous update
            disconnectServer();
            disconnectGSM();
          }
  }
  
 
}

void handleXbeeRxMessage(uint8_t *data, uint8_t length){
  for (int i = 0; i < length; i++){
    buffer[nrOfUpdates][i] = data[i];
    Serial.print(buffer[nrOfUpdates][i]);
    }
  bufferTimes[nrOfUpdates] = millis() - timeSinceLastUpload;
  
  Serial.print("Number Of Updates: ");
  Serial.println(++nrOfUpdates);
}

void showFrameData(){
  Serial.println("Incoming frame data:");
  for (int i = 0; i < xbee.getResponse().getFrameDataLength(); i++) {
    print8Bits(xbee.getResponse().getFrameData()[i]);
    Serial.print(' ');
  }
  Serial.println();
  for (int i= 0; i < xbee.getResponse().getFrameDataLength(); i++){
    Serial.write(' ');
    if (iscntrl(xbee.getResponse().getFrameData()[i]))
      Serial.write(' ');
    else
      Serial.write(xbee.getResponse().getFrameData()[i]);
    Serial.write(' ');
  }
  Serial.println(); 
}

void print32Bits(uint32_t dw){
  print16Bits(dw >> 16);
  print16Bits(dw & 0xFFFF);
}

void print16Bits(uint16_t w){
  print8Bits(w >> 8);
  print8Bits(w & 0x00FF);
}

void print8Bits(byte c){
  uint8_t nibble = (c >> 4);
  if (nibble <= 9)
    Serial.write(nibble + 0x30);
  else
    Serial.write(nibble + 0x37);
      
  nibble = (uint8_t) (c & 0x0F);
  if (nibble <= 9)
    Serial.write(nibble + 0x30);
  else
    Serial.write(nibble + 0x37);
}

void connectGSM(){ //Start GSM connection
  Serial1.end();
  Serial.println("Connecting to GSM network...");
  theGSM3ShieldV1ModemCore.println("AT");
  theGSM3ShieldV1ModemCore.println("AT");
  
  unsigned long myTimeout = 30000; // YOUR LIMIT IN MILLISECONDS 
  boolean notConnected = true;
  unsigned long timeConnect = millis();
  
  gsmAccess.begin(PINNUMBER, true, true);
  
  
  while(notConnected && (millis()-timeConnect < myTimeout)) //Keep trying to connect
    {
    int ok = 0;
    gsmAccess.ready();
    delay(1000);
    ok = gsmAccess.getStatus();
    if (ok != GSM_READY){
      Serial.print(F("GSM status: "));
      Serial.println(ok);
    } 
    else if (ok == 0){
        disconnectGSM();
        gsmAccess.begin(PINNUMBER, true, true);
    }
    else {
      signalStrength = scannerNetworks.getSignalStrength();
      delay(500);
      while(notConnected && (millis()-timeConnect < myTimeout))
        {  
          delay(300);
        if(gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD)==GPRS_READY){
          notConnected = false;
          Serial.println("Connected to GPRS.");
          connectServer();
    }
  }
    }
  }
}

void connectServer(){ //Connect to server
  delay(300);
  Serial.println("Connecting to server...");
     // if you get a connection, report back via serial:
    if (client.connect(server, port))
    {
      Serial.println("Connected");
    } 
    else
    {
      // if you didn't get a connection to the server:
      Serial.println("Connection failed.");
      disconnectGSM();
      connectGSM();
  }
}

void disconnectServer(){
  client.flush();
  client.stop();
  delay(2000);
  Serial.println("Disconnected from server.");
}

void disconnectGSM(){
  gsmAccess.shutdown();
  delay(2000);
  Serial1.begin(9600);
  Serial.println("GSM shutdown.");
}

void emptyBuffer(){
  memset(buffer,0,sizeof(buffer));
  memset(bufferTimes,0,sizeof(bufferTimes));
}

