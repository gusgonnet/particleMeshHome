/*
Copyright (c) 2013-2018 Ubidots.
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Developed and maintained by Jose Garcia for IoT Services Inc
@jotathebest at github: https://github.com/jotathebest
*/

#ifndef _Ubidots_H_
#define _Ubidots_H_

#include "Particle.h"
#include "UbiConstants.h"
#include "UbiProtocol.h"
#include "UbiProtocolHandler.h"
#include "UbiTypes.h"
#if PLATFORM_ID != PLATFORM_PHOTON_DEV &&        \
    PLATFORM_ID != PLATFORM_PHOTON_PRODUCTION && \
    PLATFORM_ID != PLATFORM_ELECTRON_PRODUCTION
#include "UbiMesh.h"
#endif

class Ubidots {
 public:
  explicit Ubidots(char* token, IotProtocol iotProtocol = UBI_TCP);
  explicit Ubidots(char* token, UbiServer server = UBI_INDUSTRIAL,
                   IotProtocol iotProtocol = UBI_TCP);
  void add(char* variable_label, float value);
  void add(char* variable_label, float value, char* context);
  void add(char* variable_label, float value, char* context,
           unsigned long dot_timestamp_seconds);
  void add(char* variable_label, float value, char* context,
           unsigned long dot_timestamp_seconds,
           unsigned int dot_timestamp_millis);
  void addContext(char* key_label, char* key_value);
  void getContext(char* context_result);
  void getContext(char* context_result, IotProtocol iotProtocol);
  bool meshPublishToUbidots();
  bool meshPublishToUbidots(const char* device_label);
  bool meshPublishToUbidots(const char* device_label, const char* device_name);
  void meshLoop();
  void setCloudProtocol(IotProtocol iotProtocol);
  bool send();
  bool send(const char* device_label);
  bool send(const char* device_label, const char* device_name);
  bool send(const char* device_label, PublishFlags flags);
  bool send(const char* device_label, const char* device_name, UbiFlags* flags);
  float get(const char* device_label, const char* variable_label);
  void setDebug(bool debug);
  ~Ubidots();

 private:
#if PLATFORM_ID != PLATFORM_PHOTON_DEV &&        \
    PLATFORM_ID != PLATFORM_PHOTON_PRODUCTION && \
    PLATFORM_ID != PLATFORM_ELECTRON_PRODUCTION
  // Mesh devices protocol wrapper
  UbiMesh* _protocolMesh;
#endif

  // Only non-Xenon devices support cloud communication
  UbiProtocolHandler* _cloudProtocol;

  ContextUbi* _context;
  IotProtocol _iotProtocol;
  int8_t _current_context = 0;
  bool _debug = false;
  void _builder(char* token, UbiServer server, IotProtocol iot_protocol);
};

#endif
