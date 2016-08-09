#include <pebble.h>
#include <stdbool.h>
#include "toStringHelpers.h"

#define SAMPLING_RATE          ACCEL_SAMPLING_10HZ
#define SAMPLES_PER_CALL_BACK  1

#define ACCEL_DICT_LENGTH      4

#define SYNC_BUFFER_SIZE       48

static Window *my_window;
static TextLayer *text_layer;

static AppSync sync;

static bool isConnected = false;
static bool sleepModeState = false;
static int syncChangeCount = 0;
static uint8_t sync_buffer[SYNC_BUFFER_SIZE];

enum Axis_Index {
  AXIS_X = 0,
  AXIS_Y = 1,
  AXIS_Z = 2
};

//keys into accelerometer data dictionary
enum Accel_Dict_Keys {
  KEY_CMD = 128,
  KEY_X = 1,
  KEY_Y = 2,
  KEY_Z = 3
};

//dict.KEY_CMD values, used to indicated if data in dictionary is (in)valid
enum Accel_Dict_Cmd_Values {
  CMD_INVALID = 0,
  CMD_VECTOR = 1 //i.e. valid
};

typedef struct {
  uint64_t sync_set;
  uint64_t sync_vib;
  uint64_t sync_missed;
} sync_stats_t;

static sync_stats_t syncStats = {0};

//Callback for sync errors
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult error, void * context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s: DICT_%s, APP_MSG_%s", __FUNCTION__, DictionaryResult_to_String(dict_error), AppMessageResult_to_String(error));
}

//Callback for sync tuple changes
/*A new accelerometer data can't be sent until all of the previous data has been
 *received, so this function decrements syncChangeCount each time a piece of data
 *from the previous dictionary has been received. We know that each dictionary only
 *has four elements, so we set syncChangeCount to 4 when we send new data.*/
static void sync_tuple_changed_callback(const uint32_t key, const Tuple * new_tuple, const Tuple * old_tuple, void * context) {
  if (syncChangeCount > 0)
    syncChangeCount--;
  else
    APP_LOG(APP_LOG_LEVEL_INFO, "unblock");
}

static void main_window_load(Window * window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "%s", __FUNCTION__);
  
  //Setup text layer
  //Get information about window
  Layer * window_layer = window_get_root_layer(my_window);
  GRect bounds = layer_get_bounds(window_layer);
  
  //Create text layer and set text
  text_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
  text_layer_set_text(text_layer, "Alarm.Wakeup");
  
  //Add text layer as child layer to window's root
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  
  //init Tuplet dictionary
  Tuplet vector_dict[] = {
    TupletInteger(KEY_CMD, CMD_INVALID),
    TupletInteger(KEY_X, (int) 0),
    TupletInteger(KEY_Y, (int) 0),
    TupletInteger(KEY_Z, (int) 0),
  };
  
  syncChangeCount = ACCEL_DICT_LENGTH;
  
  app_sync_init(&sync
                , sync_buffer, sizeof(sync_buffer)
                , vector_dict, ACCEL_DICT_LENGTH
                , sync_tuple_changed_callback
                , sync_error_callback
                , NULL);
  
  syncStats.sync_set++;
}  

static void main_window_unload(Window * window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "%s", __FUNCTION__);

  app_sync_deinit(&sync);
  
  //destory text layer
  text_layer_destroy(text_layer);
}

//Bluetooth state chance callback
void bluetooth_connection_callback (bool connected) {
  isConnected = connected;
  APP_LOG(APP_LOG_LEVEL_INFO, "Bluetooth %sonnected", (isConnected) ? "c" : "disc");
}

//Handler for when we receive new accelerometer data
void accel_data_callback(void * data, uint32_t num_samples) {
  AppMessageResult result;
  AccelData * vector = (AccelData *) data;
  
  //ignore accelerometer data during Pebble vibration
  if (vector->did_vibrate) {
    syncStats.sync_vib++;
    return;
  }

  //can't send new data until all previously sent data is received
  if (syncChangeCount > 0) {
    syncStats.sync_missed++;
    return;
  }
  
  //Build dictionary to hold the vector
  Tuplet vector_dict[] = {
    TupletInteger(KEY_CMD, CMD_VECTOR),
    TupletInteger(KEY_X, (int) vector->x),
    TupletInteger(KEY_Y, (int) vector->y),
    TupletInteger(KEY_Z, (int) vector->z)
  };
  
  //Send the dictionary to the remote 
  result = app_sync_set(&sync, vector_dict, ACCEL_DICT_LENGTH);
  
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "app_sync_set: APP_MSG_%s", AppMessageResult_to_String(result));
  } else {
    //set syncChangeCount so we know when this dictionary has been fully received
    syncChangeCount = ACCEL_DICT_LENGTH;
    
    syncStats.sync_set++;
  }
}

//callback for tap events.
/*Tapping toggles sleep mode. When in sleep mode, we want to track the accelerometer*/
void accel_tap_callback(AccelAxisType axis, uint32_t direction) {
  //don't allow sleep mode to activate unless connected to a phone
  if (!isConnected)
    sleepModeState = false;
  else
    sleepModeState = !sleepModeState;
  
  if (sleepModeState) {
    APP_LOG(APP_LOG_LEVEL_INFO, "start sleep mode");
    
    //Register to receive accelerometer data
    accel_data_service_subscribe(SAMPLES_PER_CALL_BACK, (AccelDataHandler) accel_data_callback);
    
    text_layer_set_text(text_layer, "Sleep Mode: ON");
  } else {
    //unregister from receiving accelerometer data
    accel_data_service_unsubscribe();
    
    APP_LOG(APP_LOG_LEVEL_INFO, "stop sleep mode");
    APP_LOG(APP_LOG_LEVEL_INFO, "stats: sync_set:    %lu",
            (unsigned long)syncStats.sync_set);
    APP_LOG(APP_LOG_LEVEL_INFO, "stats: sync_vib:    %lu",
            (unsigned long)syncStats.sync_vib);
    APP_LOG(APP_LOG_LEVEL_INFO, "stats: sync_missed: %lu",
            (unsigned long)syncStats.sync_missed);
    syncStats.sync_set = 0;
    syncStats.sync_vib = 0;
    syncStats.sync_missed = 0;
    
    text_layer_set_text(text_layer, "Sleep Mode: OFF");
  }
}

void handle_init(void) {
  my_window = window_create();
  WindowHandlers handlers = {.load = main_window_load, .unload = main_window_unload };
  window_set_window_handlers(my_window, handlers);
  
  window_stack_push(my_window, true);
  
  //Init Accelerometer
  accel_service_set_sampling_rate(SAMPLING_RATE);
  accel_tap_service_subscribe((AccelTapHandler) accel_tap_callback);
  app_message_open(SYNC_BUFFER_SIZE, SYNC_BUFFER_SIZE);
  
  
  //subscribe to Bluetooth status changes
  bluetooth_connection_service_subscribe(bluetooth_connection_callback);
  isConnected = bluetooth_connection_service_peek(); //what's the bluetooth status right now?
  APP_LOG(APP_LOG_LEVEL_INFO, "initially %sonnected", (isConnected) ? "c" : "disc");
}

void handle_deinit(void) {
  //unsubscribe from accelerometer if deinit'ing during sleep mode
  if (sleepModeState)
    accel_data_service_unsubscribe();
  
  window_destroy(my_window);
  APP_LOG(APP_LOG_LEVEL_INFO, "main: exit");
}

int main(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "main: entry:  %s %s", __TIME__, __DATE__);
  
  handle_init();
  app_event_loop();
  handle_deinit();
}
