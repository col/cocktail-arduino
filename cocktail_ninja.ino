#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>

unsigned long maxx(unsigned long a, unsigned long b){
	return (a > b)? a : b;
}

class Pump {
	private:
		unsigned long shouldStopPouringAt, shouldStartsPouringAt, mlToMsConstant;
		byte pumpPin, isPouring;

		void stopPouring() {
			digitalWrite(pumpPin, HIGH);
			isPouring = false;
		}

		void startPouring() {
			digitalWrite(pumpPin, LOW);
			isPouring = true;
		}

	public:
		Pump(int _pumpPin, double flowRate) {
			mlToMsConstant = 1000 / flowRate;
			pumpPin = _pumpPin;
			pinMode(pumpPin, OUTPUT);
			stopPouring();
		}

		unsigned long pourMilliliters(unsigned long milliliters, unsigned long startsFrom) {
			return pour(milliliters * mlToMsConstant, startsFrom); 
		}
		
		unsigned long pour(unsigned long milliseconds, unsigned long startsFrom) {
			unsigned long currentTime = millis();
			shouldStartsPouringAt = currentTime + startsFrom;
			shouldStopPouringAt = shouldStartsPouringAt + milliseconds;
			return milliseconds;
		}

		void loop(){
			unsigned long currentTime = millis();
			if (!isPouring && currentTime > shouldStartsPouringAt && currentTime < shouldStopPouringAt) {
				startPouring();
			} else if (isPouring && (currentTime > shouldStopPouringAt)) {
				stopPouring();
			}
		}
		
		byte isBusy(){
			return isPouring;
		}
};

YunServer server;

#define NumOfAlcoholPumps 6
#define NumOfDrinkValves 4
#define NumOfIngredients 10
int PUMP_1 = 7;
int PUMP_2 = 12;
int PUMP_3 = 11;
int PUMP_4 = 10;
int PUMP_5 = 9;
int PUMP_6 = 8;
int VALVE_1 = A1;
int VALVE_2 = A2;
int VALVE_3 = A3;
int VALVE_4 = A4;

int PUMP_FLOWRATE = 2;
int VALVE_FLOWRATE = 52;
int PINGPIN = 3;

Pump pumps[NumOfIngredients] = {
	Pump(PUMP_1, PUMP_FLOWRATE), 
	Pump(PUMP_2, PUMP_FLOWRATE), 
	Pump(PUMP_3, PUMP_FLOWRATE), 
	Pump(PUMP_4, PUMP_FLOWRATE), 
	Pump(PUMP_5, PUMP_FLOWRATE), 
	Pump(PUMP_6, PUMP_FLOWRATE), 
	Pump(VALVE_1, VALVE_FLOWRATE), 
	Pump(VALVE_2, VALVE_FLOWRATE), 
	Pump(VALVE_3, VALVE_FLOWRATE), 
	Pump(VALVE_4, VALVE_FLOWRATE)
};

byte isBusy() {
	for(int i = 0; i < NumOfIngredients; i++){
		if (pumps[i].isBusy()) {
			return true;
		}
	}
	return false;
}

long getDistance(){
	long duration,cm;
	
	pinMode(PINGPIN, OUTPUT);
	digitalWrite(PINGPIN, LOW);
	delayMicroseconds(2);
	digitalWrite(PINGPIN, HIGH);
	delayMicroseconds(5);
	digitalWrite(PINGPIN, LOW);
	pinMode(PINGPIN, INPUT);
	 
	duration = pulseIn(PINGPIN, HIGH);
	cm = microsecondsToCentimeters(duration);

	return cm;
}

long microsecondsToCentimeters(long microseconds){
	return microseconds / 29 / 2;
}

bool isGlassNotFound() {
	return getDistance() > 5;
}

void printStatus(YunClient client, int statusCode, String key, String value) {
	printHeader(client, statusCode);
	client.print("{\"" + key + "\": \"" + value + "\"}");    
}


void printHeader(YunClient client, int statusCode){
	client.println("Status: " +  String(statusCode));
	client.println("Content-Type: application/json");
	client.println("Connection: close");
	client.println();
}

void processStatusResponse(YunClient client){
	if (isBusy()){
		printStatus(client, 200, "status", "busy");
	} else if (isGlassNotFound()) { 
		printStatus(client, 200, "status", "glass not found"); 
	} else {
		printStatus(client, 200, "status", "ready");
	}
}

void processMakeDrinkResponse(YunClient client){
	if (isBusy()){
		printStatus(client, 503, "status", "busy");
	} else if(isGlassNotFound()) {    
		printStatus(client, 404, "status", "glass not found");
	} else {
		int amounts[NumOfIngredients] = {0, 0, 0, 0, 0, 0, 0, 0 ,0, 0};
		
		for (int i = 0; client.available(); i++) {
			int pumpNumber = client.parseInt() - 1;
			client.readStringUntil('-');
			amounts[pumpNumber] = client.parseInt();
			client.readStringUntil('/');
		}
	
		unsigned long alcoholPouringTime = 0L;
		for (int i = 0; i < NumOfAlcoholPumps; i++) {
			if (amounts[i]) {
				unsigned long pouringTime = pumps[i].pourMilliliters(amounts[i], 0L);
				alcoholPouringTime = maxx(alcoholPouringTime, pouringTime);
			}
		}


		unsigned long nonAlcoholPouringTime = 0L;
		for (int i = NumOfAlcoholPumps; i < NumOfIngredients; i++) {
			if (amounts[i]) {
				unsigned long pouringTime = pumps[i].pourMilliliters(amounts[i], alcoholPouringTime);
				nonAlcoholPouringTime = maxx(nonAlcoholPouringTime, pouringTime);
			}
		}


		printStatus(client, 200, "ready_in", String(alcoholPouringTime + nonAlcoholPouringTime));
	}
}

void process(YunClient client) {
	if (!client) return;
	
	String command = client.readStringUntil('/');
	
	if (command.startsWith("status")){
		processStatusResponse(client);
	} else if (command.equals("make_drink")){
		processMakeDrinkResponse(client);
	} else {
		printStatus(client, 405, "status", "not recognized");
	}
	
	client.stop();
}

void setup() {
	Serial.begin(9600);
	Bridge.begin();
	server.listenOnLocalhost();
	server.begin();
}

void loop() {
	for (int i = 0; i < sizeof(pumps)/sizeof(Pump); i ++) {
		pumps[i].loop();
	}

	process(server.accept());
}
