
#ifndef SHARED_PACKET_H
#define SHARED_PACKET_H

struct SensorPacket {
  uint8_t node_id;
  float temperature;
  float humidity;
  float light;
  uint8_t prediction;   
};

struct ControlPacket {
  uint8_t node_id;
  uint8_t led_state;    
};

#endif
