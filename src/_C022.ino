#ifdef USES_C022
//#######################################################################################################
//########################### Controller Plugin 022: Pimatic RestApi ####################################
//#######################################################################################################

/*******************************************************************************
 * Release notes:
 * V 1.0
 - First version by deejaybeam, 7 July 2016
 * V 1.01
 - Update and rename _C009.ino to _C022.ino by Wutu due to new standard protocols, 21 September 2016
 * V1.02
 - Remove ">210" core statement to comply (and function) with new standard 230 core as of R114, 24 September 2016
 * V1.03
 - Try to make compatible with V2.x.x / Mega
 *
 /******************************************************************************/

#define CPLUGIN_022
#define CPLUGIN_ID_022         22
#define CPLUGIN_NAME_022       "Pimatic RestApi"

bool CPlugin_022(byte function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case CPLUGIN_PROTOCOL_ADD:
      {
        Protocol[++protocolCount].Number = CPLUGIN_ID_022;
        Protocol[protocolCount].usesMQTT = false;
        Protocol[protocolCount].usesAccount = true;
        Protocol[protocolCount].usesPassword = true;
        Protocol[protocolCount].defaultPort = 80;
        Protocol[protocolCount].usesID = true;
        break;
      }

    case CPLUGIN_GET_DEVICENAME:
      {
        string = F(CPLUGIN_NAME_022);
        break;
      }

      case CPLUGIN_INIT:
      {
        MakeControllerSettings(ControllerSettings);
        LoadControllerSettings(event->ControllerIndex, ControllerSettings);
        C022_DelayHandler.configureControllerSettings(ControllerSettings);
        break;
     }

    case CPLUGIN_FLUSH:
      {
        process_c022_delay_queue();
        delay(0);
        break;
      }

    case CPLUGIN_PROTOCOL_SEND:
      {
        if (event->idx != 0)
        {
          // We now create a URI for the request
          String url;

          switch (event->sensorType)
            {
              case SENSOR_TYPE_SINGLE:                      // single value sensor, used for Dallas, BH1750, etc
              case SENSOR_TYPE_SWITCH:
              case SENSOR_TYPE_DIMMER:
                pimaticUpdateVariable(event, 0, UserVar[event->BaseVarIndex], 0);
                break;
              case SENSOR_TYPE_LONG:                      // single LONG value, stored in two floats (rfid tags)
                pimaticUpdateVariable(event, 0, 0, (unsigned long)UserVar[event->BaseVarIndex] + ((unsigned long)UserVar[event->BaseVarIndex + 1] << 16));
                break;
              case SENSOR_TYPE_TEMP_HUM:
              case SENSOR_TYPE_TEMP_BARO:
                {
                  pimaticUpdateVariable(event, 0, UserVar[event->BaseVarIndex], 0);
                  unsigned long timer = millis() + Settings.MessageDelay;
                  while (millis() < timer)
                    backgroundtasks();
                  pimaticUpdateVariable(event, 1, UserVar[event->BaseVarIndex + 1], 0);
                  break;
                }
              case SENSOR_TYPE_TEMP_HUM_BARO:
                {
                  pimaticUpdateVariable(event, 0, UserVar[event->BaseVarIndex], 0);
                  unsigned long timer = millis() + Settings.MessageDelay;
                  while (millis() < timer)
                    backgroundtasks();
                  pimaticUpdateVariable(event, 1, UserVar[event->BaseVarIndex + 1], 0);
                  timer = millis() + Settings.MessageDelay;
                  while (millis() < timer)
                    backgroundtasks();
                  pimaticUpdateVariable(event, 2, UserVar[event->BaseVarIndex + 2], 0);
                  break;
                }
            } // end of switch (event->SensorType)
            success = C022_DelayHandler.addToQueue(C022_queue_element(event->ControllerIndex, url));
            scheduleNextDelayQueue(TIMER_C022_DELAY_QUEUE, C022_DelayHandler.getNextScheduleTime());
        } // if ixd !=0
        else
        {
          addLog(LOG_LEVEL_ERROR, F("Pimatic RestApi : IDX cannot be zero!"));
        }
        break;
      } // end of C_PLUGIN_PROTOCOL_SEND

  } // end of switch (function)
  return success;
}

bool do_process_c022_delay_queue(int controller_number, const C022_queue_element& element, ControllerSettingsStruct& ControllerSettings);

bool do_process_c022_delay_queue(int controller_number, const C022_queue_element& element, ControllerSettingsStruct& ControllerSettings) {
  WiFiClient client;
  if (!try_connect_host(controller_number, client, ControllerSettings))
    return false;

  // This will send the request to the server
  String request = create_http_request_auth(controller_number, element.controller_idx, ControllerSettings, F("GET"), element.txt);

#ifndef BUILD_NO_DEBUG
  addLog(LOG_LEVEL_DEBUG, element.txt);
#endif
  return send_via_http(controller_number, client, request, ControllerSettings.MustCheckReply);
}


//********************************************************************************
// Pimatic updateVariable
//********************************************************************************
bool pimaticUpdateVariable(struct EventStruct *event, byte varIndex, float value, unsigned long longValue)
{

  String authHeader = "";
  String url;
  
  MakeControllerSettings(ControllerSettings);
  LoadControllerSettings(event->ControllerIndex, ControllerSettings);
  C022_DelayHandler.configureControllerSettings(ControllerSettings);


  if ((SecuritySettings.ControllerUser[0] != 0) && (SecuritySettings.ControllerPassword[0] != 0))
  {
    base64 encoder;
    String auth_user = SecuritySettings.ControllerUser[event->ControllerIndex];
    //auth += ":";
    String auth_pass = SecuritySettings.ControllerPassword[event->ControllerIndex];
    authHeader = "Authorization: Basic " + encoder.encode(auth_user) + ":" + encoder.encode(auth_pass) + " \r\n";
  }


  char log[80];
  bool success = false;
  char host[20];
  //sprintf_P(host, PSTR("%u.%u.%u.%u"), ControllerSettings.IP[0], ControllerSettings.IP[1], ControllerSettings.IP[2], ControllerSettings.IP[3]);

  //sprintf_P(log, PSTR("%s%s using port %u"), "HTTP : connecting to ", host,ControllerSettings.Port);
  //addLog(LOG_LEVEL_DEBUG, log);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(host, ControllerSettings.Port))
  {
    connectionFailures++;
    strcpy_P(log, PSTR("HTTP : connection failed"));
    addLog(LOG_LEVEL_ERROR, log);
    return false;
  }
  statusLED(true);
  if (connectionFailures)
    connectionFailures--;

  if (ExtraTaskSettings.TaskDeviceValueNames[0][0] == 0)
    PluginCall(PLUGIN_GET_DEVICEVALUENAMES, event, dummyString);

  url = "/api/variables/";
  url += URLEncode(ExtraTaskSettings.TaskDeviceValueNames[varIndex]);
  url.toCharArray(log, 80);
  addLog(LOG_LEVEL_DEBUG_MORE, log);

  String data;
  if (longValue)
    data = String(longValue);
  else
    data = toString(value, ExtraTaskSettings.TaskDeviceValueDecimals[varIndex]);

  String yourdata = "{\"type\": \"value\", \"valueOrExpression\": \"" + data + "\"}";

  String hostName = host;
  if (ControllerSettings.UseDNS)
    hostName = ControllerSettings.HostName;

  // This will send the request to the server
  client.print(String("PATCH ") + url + " HTTP/1.1\r\n" +
               authHeader +
               "Host: " + hostName + "\r\n" +
               "Content-Type:application/json\r\n" +
               "Content-Length: " + yourdata.length() + "\r\n\r\n" +
               yourdata);

  unsigned long timer = millis() + 200;
  while (!client.available() && millis() < timer)
    delay(1);

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.toCharArray(log, 80);
    addLog(LOG_LEVEL_DEBUG_MORE, log);
    if (line.substring(0, 15) == "HTTP/1.1 200 OK")
    {
      strcpy_P(log, PSTR("HTTP : Succes!"));
      addLog(LOG_LEVEL_DEBUG, log);
      success = true;
    }
    delay(1);
  }
  strcpy_P(log, PSTR("HTTP : closing connection"));
  addLog(LOG_LEVEL_DEBUG, log);

  client.flush();
  client.stop();
}
#endif
