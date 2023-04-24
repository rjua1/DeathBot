// ------- Telegram setup --------
//Including the two libraries
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

/////visit this site for telegram bot setup (I think)/////
//https://randomnerdtutorials.com/telegram-control-esp32-esp8266-nodemcu-outputs/

unsigned long hoursToMillis(int hrs) {
  unsigned long millis = (3.6 * pow(10, 6)) * hrs;
  return millis;
}

int millisToHours(unsigned long millis) {
  int hrs = millis / (3.6 * pow(10, 6));
  return hrs;
}

int millisToMinutes(unsigned long millis) {
  int mins = millis / 60000;
  int hrs = millisToHours(millis);
  mins = mins - (hrs * 60);

  return mins;
}

//------- Timers/distance  (change these) -------
int range[2] = { 5, 200 };  //min to max distance in cm
int deathInt1 = 72;         ///hours to send 1st death message
int deathInt2 = 12;         ///hours to send 2nd death message to everytone

//------- WiFi Settings (change these) -------
char ssid[] = "girl's club";          // your network SSID (name)
char password[] = "foreversMy4ever";  // your network key


// ------- Telegram config (change these)--------
#define BOT_TOKEN "5920036574:AAHITrnD3EsVgj80yVwEgHsxNgos1MywEsI"  // your Bot Token (Get from Botfather)
#define CHAT_ID "5983360667"                                        // Chat ID of where you want the message to go (You can use MyIdBot to get the chat ID)
#define DEAD_CHAT_ID "-880319725"                                   // group chat for the 2nd death message
#include <ArduinoJson.h>

//end of changing things... There's more though in telegram functions.

// ------- Distance setup --------
//#define lifeLED 32
//#define deathLED 15
#define lifeLED 2
#define deathLED 4
#define trigPin 22
#define echoPin 23


//------- timing/flagging stuff --------
bool timerOn;
bool maybeDead = 0;  //toggle the maybe dead
bool definitelyDead = 0;
unsigned long currentTime;
unsigned long prevTime[3];  // [0] last time body was detected, [1] last time death check was sent, [2] last time message was checked
//72, 12, 10000
int timer[3] = { hoursToMillis(deathInt1), hoursToMillis(deathInt2), 10000 };  //[0] 1st death check to send text, [1] 2nd death check to text someone else, [2] check message frequency
int hours;
int minutes;
long distance, duration;
bool textFlag[3] = { 0, 0, 0 };  //[0] reset time, [1] reset low [2] reset high


// SSL client needed for both libraries
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
// Checks for new messages every 1 second.
String ipAddress = "";

void setup() {

  Serial.begin(9600);
  pinMode(lifeLED, OUTPUT);
  digitalWrite(lifeLED, HIGH);

  pinMode(deathLED, OUTPUT);
  digitalWrite(deathLED, HIGH);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // ------- Telegram config --------
  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  ipAddress = ip.toString();
  client.setInsecure();

  //flash  LEDS when connected to wifi
  for (int i = 0; i < 5; i++) {
    if (i % 2 == 0) {
      digitalWrite(lifeLED, LOW);
      digitalWrite(deathLED, LOW);
    } else {
      digitalWrite(lifeLED, HIGH);
      digitalWrite(deathLED, HIGH);
    }
    delay(120);
  }
}

void loop() {
  currentTime = millis();

  // ------- check distance --------
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = smooth(1, 7, int((duration / 2) / 29.1));
  //Serial.println(distance);

  //reset death timer if motion detected
  //shine green LED when motion is detected
  if (distance < range[1] && distance > range[0]) {
    notDead();
  } else {
    if (maybeDead == 0) {
      digitalWrite(lifeLED, LOW);
    }
  }

  //might be dead since no motion has been detected. Let's send a text.
  if (currentTime - prevTime[0] > timer[0]) {
    if (maybeDead == 0) {
      maybeDead = 1;
      digitalWrite(deathLED, HIGH);
      digitalWrite(lifeLED, LOW);
      sendTelegramMessage();
      Serial.println("maybe dead");
      prevTime[1] = currentTime;  //start timer for serious text
    }
  }

  //check for telegram messages. this is annoyingly done every ~10s since it haults distance processing
  if (currentTime - prevTime[2] > timer[2]) {
    hours = millisToHours(currentTime - prevTime[0]);
    minutes = millisToMinutes(currentTime - prevTime[0]);
    Serial.print("hours : ");
    Serial.print(hours);
    Serial.print(", minutes : ");
    Serial.println(minutes);
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    prevTime[2] = currentTime;
  }

  if (maybeDead == 1) {
    //2nd death timer to really verify this chick's death
    if (currentTime - prevTime[1] > timer[1] && definitelyDead == 0) {
      //Serial.println("oh shit. She may actually be dead.");
      definitelyDead = 1;
      sendTelegramMessage();
    }
  }
  delay(100);
}


void sendTelegramMessage() {

  //1st death message detect. Send to personal telegram chat
  if (definitelyDead == 0) {
    String message = "";
    message.concat("Michelle hasn't moved in " + String(millisToHours(timer[0])) + " hours. She might be dead.");

    if (bot.sendMessage(CHAT_ID, message, "Markdown")) {
      Serial.println("TELEGRAM Successfully sent");
    }
  }
  //2nd death message detect. Send to both telegram chats
  if (definitelyDead == 1) {
    String message = "";
    message.concat("Oh shit. It's been another " + String(millisToHours(timer[1])) + " hours. She may actually be dead.");
    if (bot.sendMessage(CHAT_ID, message, "Markdown")) {
      Serial.println("TELEGRAM Successfully sent");
    }
    message = "";
    message.concat("Someone may want to check on Michelle. It's been  " + String(millisToHours(timer[0] + timer[1])) + " hours since movement was last detected.");
    if (bot.sendMessage(DEAD_CHAT_ID, message, "Markdown")) {
      Serial.println("TELEGRAM Successfully sent");
    }
  }
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/reset to reset timer \n";
      welcome += "/time to get last detect \n";
      welcome += "/changeTimer to change timer frequency \n";
      welcome += "/changeRange to change distance \n";
      bot.sendMessage(chat_id, welcome, "");
    }
    if (text == "/reset") {
      bot.sendMessage(chat_id, "Seems she's not dead", "");
      notDead();
    }
    if (text == "/time") {
      hours = millisToHours(currentTime - prevTime[0]);  //seconds minutes hours
      minutes = millisToMinutes(currentTime - prevTime[0]);


      String message = "Something moved " + String(hours) + " hours and " + String(minutes) + " minutes ago";
      bot.sendMessage(chat_id, message, "");
      Serial.println("Seems she's not dead");
    }

    if (textFlag[2] == 1) {
      range[1] = text.toInt();
      String message = ("Distance range set from " + String(range[0]) + " to " + String(range[1]) + " cm.");
      bot.sendMessage(chat_id, message, "");
      textFlag[2] = 0;
    }
    if (textFlag[1] == 1) {
      range[0] = text.toInt();
      String message = ("What is the max distance?");
      bot.sendMessage(chat_id, message, "");
      textFlag[1] = 0;
      textFlag[2] = 1;
    }
    if (textFlag[0] == 1) {
      timer[0] = hoursToMillis(text.toInt());
      String message = (" Time changed to " + String(millisToHours(timer[0])) + " hours.");
      bot.sendMessage(chat_id, message, "");
      Serial.println(" Time changed to " + String(millisToHours(timer[0])) + " hours.");
      textFlag[0] = 0;
    }

    if (text == "/changeTimer") {
      String message = "How many hours for death checks??";
      bot.sendMessage(chat_id, message, "");
      textFlag[0] = 1;
    }
    if (text == "/changeRange") {
      String message = "What is the min distance (cm)?";
      bot.sendMessage(chat_id, message, "");
      textFlag[1] = 1;
    }
  }
}

void notDead() {
  prevTime[0] = currentTime;
  prevTime[1] = currentTime;
  maybeDead = 0;
  definitelyDead = 0;
  digitalWrite(deathLED, LOW);
  digitalWrite(lifeLED, HIGH);
  //Serial.println("Seems she's not dead");
}

// ------- smoothing distance --------

#define maxarrays 3    //max number of different variables to smooth
#define maxsamples 21  //max number of points to sample and
//reduce these numbers to save RAM
unsigned int smoothArray[maxarrays][maxsamples];

// sel should be a unique number for each occurrence
// samples should be an odd number greater that 7. It's the length of the array. The larger the more smooth but less responsive
// raw_in is the input. positive numbers in and out only.

unsigned int smooth(byte sel, unsigned int samples, unsigned int raw_in) {
  int j, k, temp, top, bottom;
  long total;
  static int i[maxarrays];
  static int sorted[maxarrays][maxsamples];
  boolean done;

  i[sel] = (i[sel] + 1) % samples;    // increment counter and roll over if necessary. -  % (modulo operator) rolls over variable
  smoothArray[sel][i[sel]] = raw_in;  // input new data into the oldest slot

  for (j = 0; j < samples; j++) {  // transfer data array into anther array for sorting and averaging
    sorted[sel][j] = smoothArray[sel][j];
  }

  done = 0;            // flag to know when we're done sorting
  while (done != 1) {  // simple swap sort, sorts numbers from lowest to highest
    done = 1;
    for (j = 0; j < (samples - 1); j++) {
      if (sorted[sel][j] > sorted[sel][j + 1]) {  // numbers are out of order - swap
        temp = sorted[sel][j + 1];
        sorted[sel][j + 1] = sorted[sel][j];
        sorted[sel][j] = temp;
        done = 0;
      }
    }
  }
  //I changed this to just 3 off the top and bottom
  bottom = 3;
  top = samples - 3;
  k = 0;
  total = 0;
  for (j = bottom; j < top; j++) {
    total += sorted[sel][j];  // total remaining indices
    k++;
  }
  return total / k;  // divide by number of samples
}
