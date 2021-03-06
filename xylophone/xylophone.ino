#include <Servo.h> 

/**
 modified some values in IRremote.h to reduce interference
USECPERTICK 15 
RAWBUF 100 
MARK_EXCESS 50 
 **/
#include <IRremote.h>

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

// TODO: these should be moved down (always as params)
const int JX_PIN = A5;
const int JY_PIN = A4;
const int IR_PIN = 3;
const int NOTE_SERVO_PIN = 11;
const int HIT_SERVO_PIN = 9;
const int BASE_HIT = 90;
const int HIT_ANGLE = 10;
const int LED_PIN =6;
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
  XYLPlayer() : base_hit(BASE_HIT), last_hit_time(0) {

  }
  
  void init(int note_pin, int hit_pin) {
     note_servo.attach(note_pin);
     hit_servo.attach(hit_pin);     
     
     hit_servo.write(base_hit);
     
     angle_offset = 0;
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
  
  void wait_abs(int delta) {
    last_hit_time = millis() + delta;
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
      long note_delay = mtrack.get_note_time();
      long min_delay = get_min_delay(angle, last_hit_angle);
      if (note_delay < min_delay) { 
        note_delay = min_delay; 
       }
      last_hit_angle = angle;
      set_state(HIT_DOWN, last_hit_time + note_delay);
      if (millis() < state_active_time) {
        return;
      }
    }
    
    if (state == HIT_DOWN) {
      last_hit_time = millis();
      //sp << last_hit_time << ": HIT !!!" << endl;
      set_state(HIT_UP, last_hit_time + 40);  //40
      hit_servo.write(base_hit + HIT_ANGLE);
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
    
  void goto_angle(int target_angle) {
    int cur_angle = note_servo.read();
    int dir = (target_angle > cur_angle) ? 1 : -1;
    
    for (;cur_angle != target_angle; cur_angle += dir) {
      note_servo.write(cur_angle);
      delay(100);
    }  
  }

  long get_min_delay(int angle, int last_angle) {
    double dist = angle - last_angle;
    if (dist < 0) { dist = -dist; }
    return dist * 4.8; //4.6
  }
      
  int get_note_offset(char note) {
    switch (note) {
      case 'C': return 88;
      case 'D': return 80;
      case 'E': return 72;
      case 'F': return 63;
      case 'G': return 55;
      case 'A': return 46;
      case 'B': return 39;    
      case 'c': return 33 ;    
    
    }
    return -1;
  }

  int get_note_angle(char note) {
    int offset = get_note_offset(note);
    if (offset < 0) return -1;
    return offset + angle_offset;
  }
  
  void set_offset(int offset) {
    note_servo.write(note_servo.read() + offset - angle_offset);
    angle_offset = offset;
  }
  
  Servo note_servo;
  Servo hit_servo;
  MelodyTrack mtrack;
  unsigned long last_hit_time;
  int last_hit_angle;
  int base_hit;
  XYLState state;
  unsigned long state_active_time;
  int hit_count;
  int angle_offset;
};

char MELODY_MUPPETS[] = {0x80, 0x63, 0xa4, 0x63, 0x41, 0x42, 0x41, 0x92, 0x42, 0xa4, 0x47, 0xc8, 0x63, 0xa4, 0x63, 0x41, 0x42, 0x92, 0x41, 0xa4, 0x47, 0xc8, 0x45, 0xa4, 0x45, 0x47, 0x46, 0x45, 0x92, 0x46, 0xa4, 0x63, 0x92, 0x43, 0x44, 0x45, 0xa4, 0x45, 0x92, 0x45, 0xa4, 0x45, 0x92, 0x47, 0xec, 0x63, 0xa4, 0x63, 0x41, 0x42, 0x41, 0x92, 0x42, 0xa4, 0x47, 0xc8, 0x63, 0xa4, 0x63, 0x41, 0x42, 0x92, 0x41, 0xa4, 0x47, 0xc8, 0x45, 0xa4, 0x45, 0x47, 0x46, 0x45, 0x92, 0x46, 0xa4, 0x63, 0x92, 0x43, 0x44, 0x45, 0xa4, 0x45, 0x92, 0x44, 0xa4, 0x44, 0x92, 0x43, 0x0};
// char MELODY_LONDON_BRIDGE[] = {0x47, 0xa6, 0x41, 0x8c, 0x47, 0x99, 0x46, 0x45, 0x46, 0x47, 0xb3, 0x44, 0x99, 0x45, 0x46, 0xb3, 0x45, 0x99, 0x46, 0x47, 0xb3, 0x47, 0xa6, 0x41, 0x8c, 0x47, 0x99, 0x46, 0x45, 0x46, 0x47, 0xb3, 0x44, 0x47, 0x45, 0x99, 0x43, 0x0}; // 1.0
char MELODY_LONDON_BRIDGE[] = {0x47, 0xae, 0x41, 0x8f, 0x47, 0x9e, 0x46, 0x9e, 0x45, 0x46, 0x47, 0xbd, 0x44, 0x9e, 0x45, 0x46, 0xbd, 0x45, 0x9e, 0x46, 0x47, 0xbd, 0x47, 0xae, 0x41, 0x8f, 0x47, 0x9e, 0x46, 0x45, 0x46, 0x47, 0xbd, 0x44, 0x47, 0xbd, 0x45, 0x9e, 0x43, 0x0}; // 1.2
char MELODY_OH_SUSANNA[] = {0x43, 0x91, 0x44, 0x45, 0xa2, 0x47, 0xa2, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0xa2, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0xa2, 0x43, 0x44, 0xe7, 0x43, 0xa2, 0x45, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x44, 0x43, 0xe7, 0x43, 0x91, 0x44, 0x45, 0xa2, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x43, 0x44, 0xe7, 0x43, 0xa2, 0x45, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x44, 0x43, 0x43, 0x44, 0x45, 0x46, 0xc5, 0x46, 0x41, 0xa2, 0x41, 0xc5, 0x41, 0xa2, 0x47, 0x47, 0x45, 0x43, 0x44, 0xe7, 0x43, 0x91, 0x44, 0x45, 0xa2, 0x47, 0x47, 0xb3, 0x41, 0x91, 0x47, 0xa2, 0x45, 0x43, 0xb3, 0x44, 0x91, 0x45, 0xa2, 0x45, 0x44, 0x44, 0x43, 0x0};
char MELODY_TWINKLE_TWINKLE[] = {0x43, 0x9e, 0x43, 0x47, 0x9e, 0x47, 0x9e, 0x41, 0x41, 0x47, 0xbd, 0x46, 0x9e, 0x46, 0x45, 0x45, 0x44, 0x44, 0x9e, 0x43, 0x9e, 0x43, 0x47, 0x47, 0x46, 0x46, 0x45, 0x45, 0x44, 0xbd, 0x47, 0x9e, 0x47, 0x46, 0x9e, 0x46, 0x45, 0x45, 0x44, 0xbd, 0x43, 0x9e, 0x43, 0x47, 0x47, 0x41, 0x41, 0x47, 0xbd, 0x46, 0x9e, 0x46, 0x45, 0x45, 0x44, 0x44, 0x43, 0x0};
char MELODY_WHEN_THE_SAINTS[] = {0x43, 0x9c, 0x45, 0x46, 0x47, 0x81, 0x90, 0x43, 0x9c, 0x45, 0x46, 0x47, 0x81, 0x90, 0x43, 0x9c, 0x45, 0x46, 0x47, 0xb9, 0x45, 0x43, 0x45, 0x44, 0x81, 0x90, 0x45, 0x9c, 0x45, 0x44, 0x43, 0xd6, 0x43, 0x9c, 0x45, 0xb9, 0x47, 0x47, 0x9c, 0x46, 0x81, 0x90, 0x45, 0x9c, 0x46, 0x47, 0xb9, 0x45, 0x43, 0x44, 0x43, 0x0};
char MELODY_ROW_ROW_YOUR_BOAT[] = {0x43, 0xb3, 0x43, 0x43, 0xa2, 0x44, 0x91, 0x45, 0xb3, 0x45, 0xa2, 0x44, 0x91, 0x45, 0xa2, 0x46, 0x91, 0x47, 0xe7, 0x63, 0x91, 0x63, 0x63, 0x47, 0x47, 0x47, 0x45, 0x45, 0x45, 0x43, 0x43, 0x43, 0x47, 0xa2, 0x46, 0x91, 0x45, 0xa2, 0x44, 0x91, 0x43, 0x0};
char MELODY_JINGLE_BELLS[] = {0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x47, 0x43, 0x44, 0x45, 0xbd, 0x43, 0x46, 0x9e, 0x46, 0x46, 0x46, 0x46, 0x45, 0x9e, 0x45, 0x9e, 0x45, 0x45, 0x44, 0x9e, 0x44, 0x9e, 0x45, 0x44, 0xbd, 0x47, 0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x45, 0x45, 0xbd, 0x45, 0x9e, 0x47, 0x9e, 0x43, 0x44, 0x45, 0xbd, 0x43, 0x46, 0x9e, 0x46, 0x46, 0x46, 0x46, 0x45, 0x45, 0x45, 0x47, 0x47, 0x46, 0x44, 0x43, 0x0};
char MELODY_JOY_TO_THE_WORLD[] = {0x63, 0xb1, 0x42, 0xa5, 0x41, 0x8c, 0x47, 0xca, 0x46, 0x98, 0x45, 0xb1, 0x44, 0x43, 0xca, 0x47, 0x98, 0x41, 0xca, 0x41, 0x98, 0x42, 0xca, 0x42, 0x98, 0x63, 0xca, 0x63, 0x98, 0x63, 0x42, 0x41, 0x47, 0x47, 0xa5, 0x46, 0x8c, 0x45, 0x98, 0x63, 0x63, 0x42, 0x41, 0x47, 0x47, 0xa5, 0x46, 0x8c, 0x45, 0x98, 0x45, 0x45, 0x45, 0x45, 0x45, 0x8c, 0x46, 0x47, 0xca, 0x46, 0x8c, 0x45, 0x44, 0x98, 0x44, 0x44, 0x44, 0x8c, 0x45, 0x46, 0xca, 0x45, 0x8c, 0x44, 0x43, 0x98, 0x63, 0xb1, 0x41, 0x98, 0x47, 0xa5, 0x46, 0x8c, 0x45, 0x98, 0x46, 0x45, 0xb1, 0x44, 0x43, 0x0};
//char MELODY_OLD_MCDONALD_HAD_A_FARM[] = {0x47, 0xa1, 0x47, 0x47, 0x44, 0x45, 0x45, 0x44, 0xc3, 0x42, 0xa1, 0x42, 0x41, 0x41, 0x47, 0xe4, 0x44, 0xa1, 0x47, 0x47, 0x47, 0x44, 0x45, 0x45, 0x44, 0xc3, 0x42, 0xa1, 0x42, 0x41, 0x41, 0x47, 0xe4, 0x44, 0x90, 0x44, 0x47, 0xa1, 0x47, 0x47, 0x44, 0x90, 0x44, 0x47, 0xa1, 0x47, 0x47, 0xc3, 0x47, 0x90, 0x47, 0x47, 0xa1, 0x47, 0x90, 0x47, 0x47, 0xa1, 0x47, 0x90, 0x47, 0x47, 0x47, 0x47, 0xa1, 0x47, 0x47, 0x47, 0x47, 0x44, 0x45, 0x45, 0x44, 0xc3, 0x42, 0xa1, 0x42, 0x41, 0x41, 0x47, 0x0};

char* ALL_NOTES = "CDEFGABc";
char* MELODY_ABC = ALL_NOTES;
#define NUM_MELODIES 9
char* MELODIES[NUM_MELODIES] = {MELODY_ABC, MELODY_MUPPETS, MELODY_LONDON_BRIDGE, MELODY_OH_SUSANNA, MELODY_TWINKLE_TWINKLE, MELODY_WHEN_THE_SAINTS, MELODY_ROW_ROW_YOUR_BOAT, MELODY_JINGLE_BELLS, MELODY_JOY_TO_THE_WORLD};

char* melody = NULL;
char error_melody[5];

enum RecordingCode { REC_NONE, REC_OK, REC_INTERRUPTED, REC_TOO_LONG, REC_MISSING };
class RecordingDownloadManager { 

  public:
  RecordingDownloadManager() : recording(false), num_notes(0) {
    clear_recorded_melody();
  }
  
  void clear_recorded_melody() {
    for (int idx=0; idx < sizeof(melody) ; ++ idx) {
      melody[idx] = 0;
    }
    index = 0;
    num_notes = 0;
    last_seqnum = 0;
    last_msg_time = 0;
    last_seqnum = 0;
  }
  
  void start_recording(int notes) {
    clear_recorded_melody();
    recording = true;
    start_recording_time = millis();
    last_msg_time = start_recording_time;
    expected_num_notes = notes;
    success_code = REC_OK;
    online = false;
    sp << "start recording" << endl;    
  }
  
  void toggle_online_recording() {
    if (! recording) {
      start_recording(0);
      online = true;
    } else {
      end_recording();
    }
  }
  
  RecordingCode check_recording() {
    if (success_code == REC_NONE) { return REC_NONE; }
    check_errors();
    RecordingCode rc = success_code;
    if (!recording) {
      success_code = REC_NONE;
      return rc;
    }
    return REC_NONE;
  }
  
  void check_errors() {    
    if (recording) {
      unsigned long now = millis();
      int threshold_factor = (online ? 2 : 1);
      if (now - start_recording_time > 30000 * threshold_factor || now - last_msg_time > 5000 * threshold_factor) {
        //sp << "long gap" << endl;        
        recording = false;
        success_code = REC_INTERRUPTED;
        return;
      } 
    }
    
    if (online) { return; }
    
    if (!recording && success_code == REC_OK) {
      if (num_notes !=   expected_num_notes) {
        success_code = REC_MISSING;
      }
    }
  }
  
  void append_note(char note, int time, int seqnum) {

    if (!recording) {
      return;
    }
    long now = millis();
    if (online && time == 0) {
      time = min(max(now - last_msg_time, 200), 2000) / 25;
    }
    
    last_msg_time = now;
    if ((time >> 7) > 0) {
      melody[index++] = ((time >> 7) & 0x7f) | 0x80;
    }
    melody[index++] = (time & 0x7f) | 0x80;    
    melody[index++] = note;
    num_notes++;
    if (index + 10 > sizeof(melody)) {
      //sp << "recording too long" << endl;
      recording = false;
      success_code = REC_TOO_LONG;
    }
  }
  
  void end_recording() {
    recording = false;
    sp << "end recording" << endl;
    
  }
  
  bool is_recording() { return recording; }
  bool is_online_recording() { return is_recording() && online; }
  char* get_melody() { return melody; }
  
  private:
  char melody[100];
  bool recording;
  int index;
  int num_notes;
  int expected_num_notes;
  unsigned long start_recording_time;
  unsigned long last_msg_time;
  int last_seqnum;
  RecordingCode success_code;
  bool online;
};


enum CommandType { CC_NONE, CC_CHANNEL, CC_NEXT, CC_TOGGLE_PAUSE, CC_TOGGLE_CONTINUOUS, CC_RECORDING_START, CC_RECORDING_END, CC_RECORDED_NOTE, CC_PLAY_RECORDING, CC_TOGGLE_ONLINE_RECORDING };

#pragma pack(push, 1)


struct BasicCommand {
  CommandType ctype;
};

struct ChannelCommand {
  CommandType ctype;
  int channel;  
};

struct NextCommand {
  CommandType ctype;
  int dir;  
};

struct RecordingStart {
  CommandType ctype;
  int num_notes;
};

struct RecordedNoteCommand {
  CommandType ctype;
  char note;
  int duration;
  int seqnum;
};

union ControlCommand {
  BasicCommand basic;
  ChannelCommand channel;
  NextCommand next;
  RecordingStart recording_start;
  RecordedNoteCommand recording;
};

//typedef union ControlCommand ControlCommand;

#pragma pack(pop)

class JControl {
public:
  JControl() : xpin(JX_PIN), ypin(JY_PIN), prev_xvalue(0), prev_yvalue(0), 
	       xdir(0), ydir(0), last_sample_time(0), last_press_time(0) {}
  
  void reeval() {
    command.basic.ctype = CC_NONE;

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

    if (ydir > 0) {
      if (now - last_press_time < 2000) {
        command.basic.ctype = CC_TOGGLE_PAUSE;
      } else {
        command.basic.ctype = CC_TOGGLE_CONTINUOUS;
      }
    } else if (ydir < 0) {
      command.basic.ctype = CC_CHANNEL;
      command.channel.channel  = 0;

    } else if (xdir != 0) {
      command.basic.ctype = CC_NEXT;
      command.next.dir =  xdir;
    } 
  }
  
  int get_dir_x() { return xdir;}
  int get_dir_y() { return ydir;}

  const ControlCommand& get_command() {
    return command;
  }

  
  
private:  
  int normalize(int value) {
    if (value < 200) {
      return - 1;
    } else if (value > 800) {
      return 1;
    } else if (value > 350 && value < 650) {
      return 0;
    } else {
      return 2;
    }
  }
  
  void update(int raw_value, int& prev, int& dir) {
    int value = normalize(raw_value);
    dir = 0;
    if (value == 2) return;
    if (value == prev) return;
    if (value != 0) {
      last_press_time = millis();
    } else {
      dir = prev - value;
    }      
    prev = value;
    
  }

  void update2(int raw_value, int& prev, int& dir) {
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
  long last_press_time;
 
  ControlCommand command;

    
};



class IRControl {
public:
  IRControl(int ir_pin) : irrecv(ir_pin), last_poll_time(0), 
			  poll_interval(100), ir_value(0) {
  }

  void init() {
    irrecv.enableIRIn(); // Start the receiver
  }

  void reeval(bool require_rpt) {
    command.basic.ctype = CC_NONE;

    unsigned long now = millis();
    if (last_poll_time + poll_interval < now ) {
        return;
    }

    last_poll_time = now;
    if (!irrecv.decode(&results)) {
      return;
    }

    irrecv.resume(); 
    //sp << "## "; Serial.println(results.value, HEX);
    if ((results.value & 0xff0000l) == 0x880000l) {
      char note = 0;
      int note_idx = results.value & 0xf;
      if (note_idx < 1 || note_idx > 8) {
        sp << "Bad note idx " << note_idx << endl;
        return;
      }
      command.basic.ctype = CC_RECORDED_NOTE;
      command.recording.note = "CDEFGABc"[note_idx - 1];
      command.recording.duration = ((results.value & 0x0ff0) >> 4);
      command.recording.seqnum = ((results.value & 0xf000) >> 12);
      //Serial.println(results.value, HEX);
      sp << "Read note " << command.recording.note << " " << command.recording.duration << " seqnum=" << command.recording.seqnum << endl;
      return;      
    } else if ((results.value & 0xffff00) == 0x991100) {
        command.basic.ctype = CC_RECORDING_START; 
        command.recording_start.num_notes = results.value & 0xff;
        return;
    } else if (results.value == 0x992222) {
      command.basic.ctype = CC_RECORDING_END; return;
    } else if (results.value == 0x993333) {
      command.basic.ctype = CC_PLAY_RECORDING; return;
    }
       

    if (results.value != 0xFFFFFFFF) {
      ir_value = results.value;
      if (require_rpt) return;
    }

    if (ir_value == 0) {
      return;
    }
    

    switch (ir_value) {
    case 0xFF6897: command.basic.ctype = CC_CHANNEL; command.channel.channel = 0; break;
    case 0xFF30CF: command.basic.ctype = CC_CHANNEL; command.channel.channel = 1; break;
    case 0xFF18E7: command.basic.ctype = CC_CHANNEL; command.channel.channel = 2; break;
    case 0xFF7A85: command.basic.ctype = CC_CHANNEL; command.channel.channel = 3; break;
    case 0xFF10EF: command.basic.ctype = CC_CHANNEL; command.channel.channel = 4; break;
    case 0xFF38C7: command.basic.ctype = CC_CHANNEL; command.channel.channel = 5; break;
    case 0xFF5AA5: command.basic.ctype = CC_CHANNEL; command.channel.channel = 6; break;
    case 0xFF42BD: command.basic.ctype = CC_CHANNEL; command.channel.channel = 7; break;
    case 0xFF4AB5: command.basic.ctype = CC_CHANNEL; command.channel.channel = 8; break;
    // channel 9 button is used for playing recording
    //case 0xFF52AD: command.basic.ctype = CC_CHANNEL; command.channel.channel = 9; break; 
    case 0xFF02FD: command.basic.ctype = CC_NEXT; command.next.dir = -1; break;
    case 0xFFC23D: command.basic.ctype = CC_NEXT; command.next.dir = 1; break;
    case 0xFF22DD: command.basic.ctype = CC_TOGGLE_PAUSE; break;
    case 0xFF9867: command.basic.ctype = CC_TOGGLE_CONTINUOUS; break;
    case 0xFF629D: command.basic.ctype = CC_TOGGLE_ONLINE_RECORDING; break;
    case 0xFF52AD: command.basic.ctype = CC_PLAY_RECORDING; break;
    }

    sp << "## "; Serial.println(ir_value, HEX);
    ir_value = 0;

  }

  const ControlCommand& get_command() {
    return command;
  }

  
private:
  IRrecv irrecv;
  decode_results results;
  unsigned long last_poll_time;
  int poll_interval;
  unsigned long ir_value;
  ControlCommand command;
};


class PotentiometerReader {
  public:
  static const long INVALID = 0xFFF;
  
  PotentiometerReader(int a_pin, int a_ratio, int a_offset, int a_time_threshold, int a_sample_period) :
  pin(a_pin), ratio(a_ratio), offset(a_offset), time_threshold(a_time_threshold), sample_period(a_sample_period), last_value(INVALID), last_sample_time(0), value_time(0) {}
  
  int read() {
    long now = millis();
    if (last_sample_time + sample_period > now) {
      return INVALID;
    }
    last_sample_time = now;
    int value = analogRead(pin) / ratio + offset;
    
    if (value == last_value) {
      if (now - value_time > time_threshold && !reported) {
        reported = true;
        return value;
        
      }
      return INVALID;      
    }
    last_value = value;
    reported = false;
    value_time = now;
    return INVALID;
  }

  private:
  
  int pin;
  int ratio;
  int offset;
  int time_threshold;
  int sample_period;
  int last_value;  
  long last_sample_time;
  long value_time;
  bool reported;
};



XYLPlayer xyl;
JControl jcontrol;
IRControl ircontrol(IR_PIN);
PotentiometerReader ptreader(A0, -32, 16, 200, 20);



int done = 0;
int index = 0;
bool pause = false;
bool continuous = false;
int cached_continuous = 2;
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
  pinMode(LED_PIN, OUTPUT);
  xyl.init(NOTE_SERVO_PIN, HIT_SERVO_PIN);
  //xyl.center();
  ircontrol.init();
  next_melody(0);
  pause = true;
  continuous = true;  
} 

RecordingDownloadManager recording_manager;

void apply_control(const union ControlCommand& command) {
  sp << "CONTROL: command=" << command.basic.ctype  << endl;

  switch (command.basic.ctype) {
  case CC_CHANNEL:
    if (recording_manager.is_online_recording()) {
      if (command.channel.channel >= 1 && command.channel.channel <= 8) {
        char note = ALL_NOTES[command.channel.channel - 1];
        recording_manager.append_note(note, 0, 0);
        error_melody[0] = 0x80;;
        error_melody[1] = note;
        error_melody[2] = 0; 
        xyl.set_melody(error_melody);
        xyl.wait_abs(-100);
        pause = false; continuous = false;
      }
    } else {
      if (command.channel.channel >= 0 and command.channel.channel < NUM_MELODIES) {
        index = command.channel.channel ;
        next_melody(0);    
      }

    }
    break;
  case CC_TOGGLE_CONTINUOUS:
    continuous = !continuous;
    sp << "Continuous Mode: " << continuous << endl;
    // when set continous, but in pause - fallthrough to toggle pause
    if (true || !(continuous && pause)) {
      break;
    }
  case CC_TOGGLE_PAUSE:
    pause = !pause;
    xyl.wait_abs(500);
    sp << "Mode: " << (pause ? "Pause" : "Play") << endl;
    break;
    
  case CC_NEXT:
    {
      int skip_dir = command.next.dir;
      if (skip_dir < 0) {
	unsigned long now = millis();
	if (last_skip_prev_time + 1000 < now && xyl.get_hit_count() > 0) {
	  skip_dir = 0;
	}
	last_skip_prev_time = now;
      }
      next_melody(skip_dir);
      sp << (skip_dir < 0 ? "Prev" : (skip_dir == 0 ? "Restart" : "Next")) 
	 << " mellody " << endl; 
      xyl.wait_abs(1000);
    }
    break;    
    case CC_RECORDING_START: 
        recording_manager.start_recording(command.recording_start.num_notes);
        break;
    case CC_RECORDED_NOTE: 
        recording_manager.append_note(command.recording.note, command.recording.duration, command.recording.seqnum);
        break;
    case CC_RECORDING_END: 
        recording_manager.end_recording();
        break;    
    case CC_PLAY_RECORDING: 
        xyl.set_melody(recording_manager.get_melody());
        pause = false;        
        break;
    case CC_TOGGLE_ONLINE_RECORDING:
        recording_manager.toggle_online_recording();
        pause = true;
        break;
    
  };
  if (recording_manager.is_recording()) {
    continuous = false;
    if (!recording_manager.is_online_recording()) { pause = true; }
  }
  
};

void check_recording() {
   RecordingCode rc = recording_manager.check_recording();
   if (rc == REC_NONE) return;
    sp << "RecordingCode= " << rc << endl;

   if (rc == REC_OK) {
     xyl.set_melody(recording_manager.get_melody());
     pause = false;
     return;
   }
   
   char errnote = 'G';
   switch (rc) {
     case REC_INTERRUPTED: errnote = 'G'; break;
     case REC_MISSING : errnote = 'F'; break;
     case REC_TOO_LONG: errnote = 'E'; break;     
   }
   error_melody[0] = errnote;
   error_melody[1] = errnote;
   error_melody[2] = errnote;
   error_melody[3] = 0; 
   xyl.set_melody(error_melody);
   pause = false;
}

long last_pt_sample = 0;

void loop() 
{ 
  if (xyl.is_done()) {
    next_melody(1);
    xyl.wait(2000);
    pause = !continuous;
    return;
    
  }

  if (!pause) {  
    xyl.reeval_play_melody();
  }
  
  check_recording();

  jcontrol.reeval();
  if (jcontrol.get_command().basic.ctype != CC_NONE) {
    apply_control(jcontrol.get_command());
    return;
  }

  ircontrol.reeval(!recording_manager.is_online_recording());
  if (ircontrol.get_command().basic.ctype != CC_NONE) {
    apply_control(ircontrol.get_command());
  }
  
  if (recording_manager.is_recording()) {
    digitalWrite(LED_PIN, (millis() % 512 < 128) ? HIGH : LOW);
    cached_continuous = 2;
  } else if (cached_continuous != continuous) {
    cached_continuous = continuous;
    digitalWrite(LED_PIN, continuous ? HIGH : LOW);
  }
 
 int pt_value = ptreader.read();
 if (pt_value != PotentiometerReader::INVALID) {
    sp << "Adjusting offset " << pt_value << endl; 
    xyl.set_offset(pt_value);
 }

}
