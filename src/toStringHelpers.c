#include <pebble.h>
#include "toStringHelpers.h"

char * AppMessageResult_to_String(AppMessageResult error)
{
  switch (error) {
    case APP_MSG_OK:                          return "OK";
    case APP_MSG_SEND_TIMEOUT:                return "SEND_TIMEOUT";
    case APP_MSG_NOT_CONNECTED:               return "NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING:             return "APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS:                return "INVALID_ARGS";
    case APP_MSG_BUSY:                        return "BUSY";
    case APP_MSG_BUFFER_OVERFLOW:             return "BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED:            return "ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED:     return "CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY:               return "OUT_OF_MEMORY";
    case APP_MSG_CLOSED:                      return "CLOSED";
    case APP_MSG_INTERNAL_ERROR:              return "INTERNAL_ERROR";
    default:                                  return "unknown";
  }
}

char * DictionaryResult_to_String(DictionaryResult error)
{
  switch (error) {
    case DICT_OK:                      return "OK";
    case DICT_NOT_ENOUGH_STORAGE:      return "NOT_ENOUGH_STORAGE";
    case DICT_INVALID_ARGS:            return "INVALID_ARGS";
    case DICT_INTERNAL_INCONSISTENCY:  return "INTERNAL_INCONSISTENCY";
    case DICT_MALLOC_FAILED:           return "MALLOC_FAILED";
    default:                           return "unknown";
  }
}