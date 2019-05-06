
#include "LEDString.hpp"


LEDString::LEDString(uint8_t LEDPin){
        Pin = LEDPin;
}

void LEDString::init(){
        pinMode(Pin,OUTPUT);
        analogWrite(Pin, 0);
}

void LEDString::set(int duty){
        duty_save = duty;
        if(duty >= 100) duty_save = 100;
        if(duty <= 0) duty_save = 0;
        analogWrite(Pin, duty_save*10);
        Serial.print("duty: ");
        Serial.println(duty_save);
}

void LEDString::on(){
        if(duty_old <= LOWERLIMIT) {
                LEDString::move(100);
        }else{
                LEDString::move(duty_old);
        }
        status = 1;
}

void LEDString::off(){
        duty_old = duty_save;
        status = 0;
        LEDString::move(0);
}

void LEDString::move(int duty, int time_ms){
        if(duty <= LOWERLIMIT) {
                duty = 0;
        }
        if (duty_save > duty) {
                direction = 0;
                steps = time_ms / (duty_save - duty);
        }
        if (duty_save < duty) {
                direction = 1;
                steps = time_ms / (duty - duty_save);
        }
        duty_goal = duty;
        Serial.print("direction"); Serial.println(direction);
        Serial.print("steps"); Serial.println(steps);
        Serial.print("duty_goal"); Serial.println(duty_goal);
        Serial.print("duty_save"); Serial.println(duty_save);
        tick.attach_ms(steps, LEDSTRING::tickHandlerLEDStr,this);
}

bool LEDString::ismoving(){
        Serial.print("duty_goal"); Serial.println(duty_goal);
        Serial.print("duty_save"); Serial.println(duty_save);
        if(duty_goal == duty_save) {
                return false;
        } else {
                return true;
        }
}

bool LEDString::getStatus(){
        if (duty_save <= LOWERLIMIT) {
                status = false;
        }
        if (duty_save > LOWERLIMIT) {
                status = true;
        }
        return status;
}

void LEDString::up(){
        set(duty_save+1);
}

void LEDString::down(){
        set(duty_save-1);
}

int LEDString::getDuty(){
        return duty_save;
}

int LEDString::getDirection(){
        return direction;
}

void LEDSTRING::tickHandlerLEDStr(LEDString *obj){
        if(obj->getDirection() == 0) {
                obj->down();
        }
        if(obj->getDirection() == 1) {
                obj->up();
        }
        if(!obj->ismoving()) {
                obj->tick.detach();
        }
}
