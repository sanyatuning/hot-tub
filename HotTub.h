#include "base64.h"
#include "esphome.h"
#include "SPISlave.h"

static const char *TAG = "HotTub";

String tohex(const uint8_t *data, uint8_t len) {
  char buf[20];
  String res;
  for (size_t i = 0; i < len; i++) {
    sprintf(buf, "0x%02X,", data[i]);
    res += buf;
  }
  return res;
}

class HotTub : public PollingComponent {

  const uint8_t mode = INPUT;
  const unsigned long timeout = 400;
  const unsigned int sleep = 1;
  int state = 0;
  String last = "";

  uint8_t temp = 0;
  bool bubi = false;
  bool pump = false;
  bool heat = false;
  bool lock = false;
  bool on1 = false;
  bool on2 = false;

  HighFrequencyLoopRequester high_freq_;

  public:

    Sensor *temperature = new Sensor();
    BinarySensor *bubbles = new BinarySensor();
    BinarySensor *pumping = new BinarySensor();
    BinarySensor *heating = new BinarySensor();
    BinarySensor *locked = new BinarySensor();
    BinarySensor *on = new BinarySensor();

    HotTub() : PollingComponent(10000) {}

    void setup() override {
      pinMode(MISO, mode);
      pinMode(MOSI, mode);
      pinMode(D6, mode);
      pinMode(D5, mode);
    }

    void update() override {
      //publish_state(state);
      ESP_LOGI(TAG, "temp: %d, pump: %d, heat: %d, bubi: %d", temp, pump, heat, bubi);
      ESP_LOGI(TAG, "   on: %d, lock: %d,        screen: %d", on1, on2, lock);
      temperature->publish_state(temp);
      pumping->publish_state(pump);
      heating->publish_state(heat);
      bubbles->publish_state(bubi);
      locked->publish_state(lock);
      on->publish_state(on1);
    }

    void dump_config() override {
    }

    uint8_t readBytes(uint8_t *data, uint8_t len) {
      uint8_t i = 0;
      for (; i < len; i++) {
        uint8_t incoming = 0;
        for (uint8_t j = 0; j < 8; j++) {
          const unsigned long temp = pulseIn(D6, HIGH, timeout);
          if (temp == 0) {
            //ESP_LOGD(TAG, "sck timout");
            return i;
          }
          //state = temp;
          incoming += digitalRead(MOSI) << j;
          delayMicroseconds(sleep);
        }
        data[i] = incoming;
//           if (digitalRead(D5) == HIGH) {
  //           break;
    //       }
      }
      ESP_LOGD(TAG, "ok");

      return i;
    }

    uint8_t decode(uint8_t seg) {
      switch (seg) {
        case 63: return 0;
        case 6: return 1;
        case 91: return 2;
        case 79: return 3;
        case 102: return 4;
        case 109: return 5;
        case 125: return 6;
        case 7: return 7;
        case 127: return 8;
        case 111:
        default: return 9;
      }
    }

// SS -> D5
// SCK -> D6
    void loop() override {
      this->high_freq_.start();
      if (pulseIn(D5, LOW, 15000) > 0) {
        //state++;
        uint8_t data[20];
        noInterrupts();
        uint8_t len = readBytes(data, 20);
        interrupts();
        if (len == 12) {
          if (decode(data[4])) {
            temp = decode(data[6]) + 10 * decode(data[4]);
          }

          lock = data[8] & 0x04;
          heat = data[8] & 0x10;
          bubi = data[8] & 0x40;

          on1 = data[10] & 0x08;
          pump = data[10] & 0x01;

          on2 = data[11] & 0x80;



          String msg = tohex(data, len);
          if (msg != last) {
            last = msg;
            ESP_LOGD(TAG, "change detected");
            ESP_LOGD(TAG, "%s", last.c_str());
            update();
          }
        }
      }
    }
};
