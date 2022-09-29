class FanControl{
private:
int FanTrigPin;

public:

  void fanSetup(int fanTrigPin){
    FanTrigPin = fanTrigPin;
    pinMode(FanTrigPin, OUTPUT);
    
    }
  void startFan(){

   digitalWrite(FanTrigPin,HIGH);
    }

  void stopFan(){
    digitalWrite(FanTrigPin,LOW);
    }
  
  };
FanControl FN;
