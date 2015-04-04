
#include <Servo.h> 


/// SerialPrinter 
struct SerialPrinter {
};

SerialPrinter sp;
const char endl = '\n';

template <typename T>
SerialPrinter& operator<<(SerialPrinter& sp, T value) {
  Serial.print(value);
}

//////////////////


class MelodyTrack {
public:
  MelodyTrack() : index(0), cur_time(0), next_time(0), base_duration(350) {}
  
  void reset() { index = 0; note=0xff; cur_time = 400;}
  
  bool has_melody() { return pMelody != 0; }
  bool is_done() { return (note == 0  || !has_melody()); }
  
  void set_melody(char* melody) {
    pMelody = melody;
    index = 0;
  }
    
  unsigned long get_note_time() {
    return cur_time;
  }
  
  unsigned long get_note() {
    return note;
  }
  
  
  void next() {
    
    if (pMelody == NULL) {
       note = 0;
       return;
    }
    
    int vtime = 0;
    while ((pMelody[index] & 0x80) != 0) {       
       vtime = (vtime << 7) + (pMelody[index] & 0x7f);
       index += 1;
    }
    if (vtime > 0) {
      cur_time = vtime * 10;
    }
    
    note = pMelody[index];
    index +=1;
  }
  
  void next2() {
    //unsigned long stime = millis();
    if (pMelody == NULL) {
      note = 0;
      return;
    }
    unsigned long base_time = 0; //max(next_time, millis());
    note = pMelody[index];
    cur_time = next_time;
    next_time = base_time + base_duration ;
    if (note == 0) {
      return;
    }
    while (true) {
      index += 1;
      char code = pMelody[index];
      if (code == 0 || is_note(code)) {
        break;
      }
      switch(code) {
        case '-': 
          next_time -= base_duration; break;
        case ' ':
          next_time += 200; break;
        case 'z':
          next_time += 100; break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': 
        case '8':
        case '9':
          next_time += (code - '0') * 10; break;        
      }  
    }
    
    //sp << base_time << ": Next time: " << next_time << "; Next note: " << note << endl;
    //sp << "next-time: " << millis() - stime << endl;
  }
  
  bool is_note(char ch) {
    return ((ch >= 'A' && ch <= 'G') || (ch >= 'a' && ch <= 'c'));    
  }
  
  bool is_numeric(char ch) {
    return (ch >= '0' && ch <= '9');
  }
    
  int index;
  char* pMelody;
  unsigned long cur_time; 
  unsigned long next_time;
  char note;
  int base_duration;
};

enum XYLState { READ_NOTE, HIT_DOWN, HIT_UP, POST_HIT };

class XYLPlayer {
public:  
  XYLPlayer() : base_hit(31), last_hit_time(0) {

  }
  
  void init(int note_pin, int hit_pin) {
     note_servo.attach(note_pin);
     hit_servo.attach(hit_pin);
     hit_servo.write(base_hit);
  }
  
  void reset() {
    set_state(READ_NOTE, 0);
    hit_count = 0;
    last_hit_time = millis() + 600;
    mtrack.reset();
    hit_servo.write(base_hit);
  }
  
  void wait(int delta) {
    last_hit_time += delta;
  }
  
  bool is_done() { return mtrack.is_done(); }
  
  void set_melody(char* melody) {
    mtrack.set_melody(melody);
    reset();
  }
  
  int get_hit_count() { return hit_count; }
  
  void center() {
    goto_angle(get_note_angle('F'));
  }
  
  void play_melody() {
    
    goto_angle(get_note_angle('F'));

    if (!mtrack.has_melody()) {
      sp << "No melody" << endl;
      return;      
    }
    
    while (true) {
      mtrack.next();
      char note = mtrack.get_note();
      unsigned long  play_time = last_hit_time + mtrack.get_note_time();
      
      if (note == 0) {
        break;
      }
      playNote(note, play_time);
      //sp << "index=" << mtrack.index << endl;
    }
    sp << "Done" << endl;
    
    goto_angle(90);          
  }
  
  
  void reeval_play_melody() {
    
    unsigned long entry = millis();
    if (state != READ_NOTE && entry < state_active_time) {
      return;
    }
    
    //sp << entry << ": reeval_play_mellody: state=" << state << endl; 
    if (state == READ_NOTE) {
      mtrack.next();
      char note = mtrack.get_note();      
      //sp << millis() << ": read note  " << note << " at "  <<  mtrack.get_note_time() << endl;
      if (note == 0) {return; }
      int angle = get_note_angle(note);
      if (angle < 0) { return; }
      //sp << millis() << ": GOTO" << endl;
      note_servo.write(angle); 
      //sp << millis() << ": SET STATE" << endl;
      set_state(HIT_DOWN, last_hit_time + mtrack.get_note_time());
      if (millis() < state_active_time) {
        return;
      }
    }
    
    if (state == HIT_DOWN) {
      last_hit_time = millis();
      //sp << last_hit_time << ": HIT !!!" << endl;
      set_state(HIT_UP, last_hit_time + 40);
      hit_servo.write(base_hit + 19);
      hit_count++;
      
    } else if (state == HIT_UP) {
      set_state(POST_HIT, millis() + 50);
      hit_servo.write(base_hit);
    } else if (state == POST_HIT) {
      set_state(READ_NOTE, 0);
    }
    
  }

//private:

  void set_state(XYLState new_state, unsigned long time) {
    //sp << "setting state " << new_state << " at " << time << endl;
    state = new_state;
    state_active_time = time;
  }
  
  void hit() {
    Serial.print(millis()); Serial.println(": Hit");
    last_hit_time = millis();
    hit_servo.write(base_hit + 18); //21);
    delay(100);
    delay(100);
    hit_servo.write(base_hit);
  
  
  };
  
  void playNote(char note, unsigned long time) {
    int angle = get_note_angle(note);
    if (angle < 0) { return; }
    Serial.print(time); Serial.print(": Play "); Serial.print(note); Serial.print(" at "); Serial.print(angle); Serial.print("\n");
    note_servo.write(angle);
    //delay(50);
    unsigned long now = millis();
    if (now < time) {
      delay(time - now);
    }
    hit();
    delay(50);  
  }

  void goto_angle(int target_angle) {
    int cur_angle = note_servo.read();
    int dir = (target_angle > cur_angle) ? 1 : -1;
    
    for (;cur_angle != target_angle; cur_angle += dir) {
      note_servo.write(cur_angle);
      delay(100);
    }  
  }

  int get_note_offset(char note) {
    switch (note) {
      case 'C': return 88;
      case 'D': return 79;
      case 'E': return 70;
      case 'F': return 63;
      case 'G': return 56;
      case 'A': return 48;
      case 'B': return 41;    
      case 'c': return 32;    
    
    }
    return -1;
  }

  int get_note_angle(char note) {
    int offset = get_note_offset(note);
    if (offset < 0) return -1;
    return offset -9;
  }
  
  Servo note_servo;
  Servo hit_servo;
  MelodyTrack mtrack;
  unsigned long last_hit_time;
  int base_hit;
  XYLState state;
  unsigned long state_active_time;
  int hit_count;

};
char MELODY_LONDON_BRIDGE[] = {0x47, 0xa6, 0x41, 0x8c, 0x47, 0x99, 0x46, 0x45, 0x46, 0x47, 0xb3, 0x44, 0x99, 0x45, 0x46, 0xb3, 0x45, 0x99, 0x46, 0x47, 0xb3, 0x47, 0xa6, 0x41, 0x8c, 0x47, 0x99, 0x46, 0x45, 0x46, 0x47, 0xb3, 0x44, 0x47, 0x45, 0x99, 0x43, 0x0};
char MELODY_OH_SUSANNA[] = {0x43, 0x91, 0x44, 0x45, 0xa2, 0x47, 0xa2, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0xa2, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0xa2, 0x43, 0x44, 0xe7, 0x43, 0xa2, 0x45, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x44, 0x43, 0xe7, 0x43, 0x91, 0x44, 0x45, 0xa2, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x43, 0x44, 0xe7, 0x43, 0xa2, 0x45, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x44, 0x43, 0x43, 0x44, 0x45, 0x46, 0xc5, 0x46, 0x41, 0xa2, 0x41, 0xc5, 0x41, 0xa2, 0x47, 0x47, 0x45, 0x43, 0x44, 0xe7, 0x43, 0x91, 0x44, 0x45, 0xa2, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x44, 0x43, 0x0};
char MELODY_WHEN_THE_SAINTS[] = {0x43, 0x9c, 0x45, 0x46, 0x47, 0x81, 0x90, 0x43, 0x9c, 0x45, 0x46, 0x47, 0x81, 0x90, 0x43, 0x9c, 0x45, 0x46, 0x47, 0xb9, 0x45, 0x43, 0x45, 0x44, 0x81, 0x90, 0x45, 0x9c, 0x45, 0x44, 0x43, 0xd6, 0x43, 0x9c, 0x45, 0xb9, 0x47, 0x47, 0x9c, 0x46, 0x81, 0x90, 0x45, 0x9c, 0x46, 0x47, 0xb9, 0x45, 0x43, 0x44, 0x43, 0x0};
char MELODY_ROW_ROW_YOUR_BOAT[] = {0x43, 0xb3, 0x43, 0x43, 0xa2, 0x44, 0x91, 0x45, 0xb3, 0x45, 0xa2, 0x44, 0x91, 0x45, 0xa2, 0x46, 0x91, 0x47, 0xe7, 0x63, 0x91, 0x63, 0x63, 0x47, 0x47, 0x47, 0x45, 0x45, 0x45, 0x43, 0x43, 0x43, 0x47, 0xa2, 0x46, 0x91, 0x45, 0xa2, 0x44, 0x91, 0x43, 0x0};
char MELODY_JINGLE_BELLS[] = {0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x47, 0x43, 0x44, 0x45, 0xbd, 0x43, 0x46, 0x9e, 0x46, 0x46, 0x46, 0x46, 0x45, 0x9e, 0x45, 0x9e, 0x45, 0x45, 0x44, 0x9e, 0x44, 0x9e, 0x45, 0x44, 0xbd, 0x47, 0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x47, 0x9e, 0x43, 0x44, 0x45, 0xbd, 0x43, 0x46, 0x9e, 0x46, 0x46, 0x46, 0x46, 0x45, 0x45, 0x45, 0x47, 0x47, 0x46, 0x44, 0x43, 0x0};
char MELODY_TWINKLE_TWINKLE[] = {0x43, 0x9e, 0x43, 0x47, 0x9e, 0x47, 0x9e, 0x41, 0x41, 0x47, 0xbd, 0x46, 0x9e, 0x46, 0x45, 0x45, 0x44, 0x44, 0x9e, 0x43, 0x9e, 0x43, 0x47, 0x47, 0x46, 0x46, 0x45, 0x45, 0x44, 0xbd, 0x47, 0x9e, 0x47, 0x46, 0x9e, 0x46, 0x45, 0x45, 0x44, 0xbd, 0x43, 0x9e, 0x43, 0x47, 0x47, 0x41, 0x41, 0x47, 0xbd, 0x46, 0x9e, 0x46, 0x45, 0x45, 0x44, 0x44, 0x43, 0x0};
char MELODY_JOY_TO_THE_WORLD[] = {0x63, 0xb0, 0x42, 0xa4, 0x41, 0x8c, 0x47, 0xc8, 0x46, 0x98, 0x45, 0xb0, 0x44, 0x43, 0xc8, 0x47, 0x98, 0x41, 0xc8, 0x41, 0x98, 0x42, 0xc8, 0x42, 0x98, 0x63, 0xc8, 0x63, 0x98, 0x63, 0x42, 0x41, 0x47, 0x47, 0xa4, 0x46, 0x8c, 0x45, 0x98, 0x63, 0x63, 0x42, 0x41, 0x47, 0x47, 0xa4, 0x46, 0x8c, 0x45, 0x98, 0x45, 0x45, 0x45, 0x45, 0x45, 0x8c, 0x46, 0x47, 0xc8, 0x46, 0x8c, 0x45, 0x44, 0x98, 0x44, 0x44, 0x44, 0x8c, 0x45, 0x46, 0xc8, 0x45, 0x8c, 0x44, 0x43, 0x98, 0x63, 0xb0, 0x41, 0x98, 0x47, 0xa4, 0x46, 0x8c, 0x45, 0x98, 0x46, 0x45, 0xb0, 0x44, 0x43, 0x0};
char MELODY_CAMPTOWN_RACES[] = {0x47, 0x9d, 0x47, 0x45, 0x47, 0x41, 0x9d, 0x47, 0x45, 0xba, 0x45, 0x9d, 0x44, 0xba, 0x45, 0x9d, 0x44, 0xba, 0x47, 0x9d, 0x47, 0x45, 0x47, 0x41, 0x47, 0x45, 0xba, 0x44, 0x9d, 0x46, 0x45, 0x44, 0x9d, 0x43, 0xf5, 0x43, 0x9d, 0x43, 0x45, 0x47, 0x63, 0xf5, 0x41, 0x9d, 0x41, 0x63, 0x41, 0x47, 0xf5, 0x47, 0x9d, 0x47, 0x45, 0x47, 0x41, 0x47, 0x45, 0xba, 0x44, 0x9d, 0x46, 0x45, 0x44, 0x43, 0x0};
char MELODY_OLD_MCDONALD_HAD_A_FARM[] = {0x47, 0xa1, 0x47, 0x47, 0x44, 0x45, 0x45, 0x44, 0xc3, 0x42, 0xa1, 0x42, 0x41, 0x41, 0x47, 0xe4, 0x44, 0xa1, 0x47, 0x47, 0x47, 0x44, 0x45, 0x45, 0x44, 0xc3, 0x42, 0xa1, 0x42, 0x41, 0x41, 0x47, 0xe4, 0x44, 0x90, 0x44, 0x47, 0xa1, 0x47, 0x47, 0x44, 0x90, 0x44, 0x47, 0xa1, 0x47, 0x47, 0xc3, 0x47, 0x90, 0x47, 0x47, 0xa1, 0x47, 0x90, 0x47, 0x47, 0xa1, 0x47, 0x90, 0x47, 0x47, 0x47, 0x47, 0xa1, 0x47, 0x47, 0x47, 0x47, 0x44, 0x45, 0x45, 0x44, 0xc3, 0x42, 0xa1, 0x42, 0x41, 0x41, 0x47, 0x0};
char* MELODY_ABC = "CDEFGABc";
#define NUM_MELODIES 10
char* MELODIES[NUM_MELODIES] = {MELODY_ABC, MELODY_LONDON_BRIDGE, MELODY_OH_SUSANNA, MELODY_WHEN_THE_SAINTS, MELODY_ROW_ROW_YOUR_BOAT, MELODY_JINGLE_BELLS, MELODY_TWINKLE_TWINKLE, MELODY_JOY_TO_THE_WORLD, MELODY_CAMPTOWN_RACES, MELODY_OLD_MCDONALD_HAD_A_FARM};

char* melody = NULL;

const int JX_PIN = A0;
const int JY_PIN = A1;


class JControl {
public:
  JControl() : xpin(A5), ypin(A4), prev_xvalue(0), prev_yvalue(0), xdir(0), ydir(0), last_sample_time(0) {}
  
  void reeval() {
    unsigned long now = millis();
    if (now < last_sample_time + 100) {
      xdir = 0;
      ydir = 0;
      return;
    }
    last_sample_time = now;
    
    update(analogRead(xpin), prev_xvalue, xdir);
    update(analogRead(ypin), prev_yvalue, ydir);  
    
    if (xdir != 0 || ydir != 0) {
      sp << "JControl : " << xdir << " : " << ydir << endl;
    }
    
  }
  
  int get_dir_x() { return xdir;}
  int get_dir_y() { return ydir;}
  
  
private:  
  int normalize(int value) {
    if (value < 100) {
      return - 1;
    } else if (value > 900) {
      return 1;
    } else if (value > 350 && value < 650) {
      return 0;
    } else {
      return 2;
    }
  }
  
  void update(int raw_value, int& prev, int& dir) {
    int value = normalize(raw_value);
    if (value == 2) return;
    if (value == 1 && value > prev) {
      dir = 1;
    } else if (value == -1 && value < prev) {
      dir = -1;
    } else {
      dir = 0;
    }
      
    prev = value;
    
  }
  
  int xpin;
  int ypin;
  int prev_xvalue;
  int prev_yvalue;
  int xdir;
  int ydir;
  unsigned long last_sample_time;
    
};

XYLPlayer xyl;
JControl jcontrol;


int done = 0;
int index = 0;
bool pause = true;
unsigned long last_skip_prev_time = 0;

void next_melody(int dir) {
    index = (index + NUM_MELODIES + dir) % NUM_MELODIES; 
    melody = MELODIES[index];
    xyl.set_melody(melody);
    pause = false;
}

void setup() 
{ 
  
  Serial.begin(9600);
  xyl.init(9,10);
  next_melody(0);
  pause = true;
  //xyl.set_melody(melody);
  //xyl.center();
  
} 
 

void loop() 
{ 
  //xyl.goto_angle(90);
  //xyl.goto_angle(60);
  
  //return;
  if (xyl.is_done()) {
    next_melody(1);
    xyl.wait(2000);
    return;
    
  }
  //xyl.set_melody(melody);
  if (!pause) {  
    xyl.reeval_play_melody();
  }
  
  jcontrol.reeval();
  bool pause_press = (jcontrol.get_dir_y() < 0);
  bool goto_head_press = (jcontrol.get_dir_y() > 0);
  int skip_dir = -jcontrol.get_dir_x();
  if (pause_press) {
    pause = !pause;
    sp << "Mode: " << (pause ? "Pause" : "Play") << endl;
  } else if (goto_head_press) {
    index = 0;
    next_melody(0);
  } else if (skip_dir != 0) {
    if (skip_dir < 0) {
      unsigned long now = millis();
      if (last_skip_prev_time + 1000 < now && xyl.get_hit_count() > 0) {
        skip_dir = 0;
      }
      last_skip_prev_time = now;
    }
    next_melody(skip_dir);
    sp << (skip_dir < 0 ? "Prev" : (skip_dir == 0 ? "Restart" : "Next")) << " mellody " << endl; 
    xyl.wait(1000);
  }
  
  //delay(3000);
}


