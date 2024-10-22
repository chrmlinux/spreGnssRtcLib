//
// name           : spreGnssRtcLib.ino
// date/author    : 2024/10/22 chrmlinux03
// update/author  : 2024/10/22 chrmlinux03
//


#include "spreGnssRtcLib.hpp"
spreGnssRtcLib gnss;

//================================
// setup
//================================
void setup(void) {
  Serial.begin(115200);
  gnss.setDebugMode(true);
  gnss.setSftPos(0.0, 0.0);
  gnss.setSatelliteType(eSatGpsQz1cQz1S);
  gnss.begin();
}

//================================
// displayPos
//================================
void displayPos() {
  RtcTime nowTime = RTC.getTime();
  Serial.printf("RTC:%04d/%02d/%02d-%02d:%02d:%02d\n",
                nowTime.year(), nowTime.month(), nowTime.day(),
                nowTime.hour(), nowTime.minute(), nowTime.second());
  Serial.printf("Lat:%.6f Lon:%.6f\n", gnss.getLatitude(), gnss.getLongitude());
}

//================================
// loop
//================================
void loop(void) {
  gnss.update();
  Serial.printf("numSat:%2d\n", gnss.getSatelliteCount());
  gnss.skyPlot(40, 20);
  if (gnss.isPosDataExist()) displayPos();
}
