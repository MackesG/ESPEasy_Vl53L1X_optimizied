#include "src/Helpers/_CPlugin_Helper.h"
#ifdef USES_C001

# include "src/Helpers/_CPlugin_DomoticzHelper.h"

// #######################################################################################################
// ########################### Controller Plugin 001: Domoticz HTTP ######################################
// #######################################################################################################

# define CPLUGIN_001
# define CPLUGIN_ID_001         1
# define CPLUGIN_NAME_001       "Domoticz HTTP"


bool CPlugin_001(CPlugin::Function function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case CPlugin::Function::CPLUGIN_PROTOCOL_ADD:
    {
      ProtocolStruct& proto = getProtocolStruct(event->idx); //      = CPLUGIN_ID_001;
      proto.usesMQTT     = false;
      proto.usesAccount  = true;
      proto.usesPassword = true;
      proto.usesExtCreds = true;
      proto.defaultPort  = 8080;
      proto.usesID       = true;
      break;
    }

    case CPlugin::Function::CPLUGIN_GET_DEVICENAME:
    {
      string = F(CPLUGIN_NAME_001);
      break;
    }

    case CPlugin::Function::CPLUGIN_INIT:
    {
      success = init_c001_delay_queue(event->ControllerIndex);
      break;
    }

    case CPlugin::Function::CPLUGIN_EXIT:
    {
      exit_c001_delay_queue();
      break;
    }

    case CPlugin::Function::CPLUGIN_PROTOCOL_SEND:
    {
      if (C001_DelayHandler == nullptr || !validTaskIndex(event->TaskIndex)) {
        break;
      }
      if (C001_DelayHandler->queueFull(event->ControllerIndex)) {
        break;
      }

      if (event->idx != 0)
      {
        // We now create a URI for the request
        const Sensor_VType sensorType = event->getSensorType();
        String url;
        const size_t expectedSize = sensorType == Sensor_VType::SENSOR_TYPE_STRING ? 64 + event->String2.length() : 128;

        if (url.reserve(expectedSize)) {
          url = F("/json.htm?type=command&param=");

          if (sensorType == Sensor_VType::SENSOR_TYPE_SWITCH ||
              sensorType == Sensor_VType::SENSOR_TYPE_DIMMER) 
          {
            url += F("switchlight&idx=");
            url += event->idx;
            url += F("&switchcmd=");

            if (essentiallyZero(UserVar[event->BaseVarIndex])) {
              url += F("Off");
            } else {
              if (sensorType == Sensor_VType::SENSOR_TYPE_SWITCH) {
                url += F("On");
              } else {
                url += F("Set%20Level&level=");
                url += UserVar[event->BaseVarIndex];
              }
            }
          } else {
            url += F("udevice&idx=");
            url += event->idx;
            url += F("&nvalue=0");
            url += F("&svalue=");
            url += formatDomoticzSensorType(event);
          }

          // Add WiFi reception quality
          url += F("&rssi=");
          url += mapRSSItoDomoticz();
            # if FEATURE_ADC_VCC
          url += F("&battery=");
          url += mapVccToDomoticz();
            # endif // if FEATURE_ADC_VCC

          std::unique_ptr<C001_queue_element> element(new C001_queue_element(event->ControllerIndex, event->TaskIndex, std::move(url)));

          success = C001_DelayHandler->addToQueue(std::move(element));
          Scheduler.scheduleNextDelayQueue(SchedulerIntervalTimer_e::TIMER_C001_DELAY_QUEUE,
                                           C001_DelayHandler->getNextScheduleTime());
        }
      } // if ixd !=0
      else
      {
        addLog(LOG_LEVEL_ERROR, F("HTTP : IDX cannot be zero!"));
      }
      break;
    }

    case CPlugin::Function::CPLUGIN_FLUSH:
    {
      process_c001_delay_queue();
      delay(0);
      break;
    }

    default:
      break;
  }
  return success;
}

// Uncrustify may change this into multi line, which will result in failed builds
// *INDENT-OFF*
bool do_process_c001_delay_queue(int controller_number, const Queue_element_base& element_base, ControllerSettingsStruct& ControllerSettings) {
  const C001_queue_element& element = static_cast<const C001_queue_element&>(element_base);

// *INDENT-ON*
  # ifndef BUILD_NO_DEBUG

  if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
    addLog(LOG_LEVEL_DEBUG, element.txt);
  }
  # endif // ifndef BUILD_NO_DEBUG

  int httpCode = -1;
  send_via_http(
    controller_number,
    ControllerSettings,
    element._controller_idx,
    element.txt,
    F("GET"),
    EMPTY_STRING,
    EMPTY_STRING,
    httpCode);
  return (httpCode >= 100) && (httpCode < 300);
}

#endif // ifdef USES_C001
