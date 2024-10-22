#ifndef __SPREGNSSRTC_LIB_H__
#define __SPREGNSSRTC_LIB_H__

//
// name           : spreGnssRtcLib.hpp
// date/author    : 2024/10/22 chrmlinux03
// update/author  : 2024/10/22 chrmlinux03
//

#include <Arduino.h>
#include <RTC.h>
#include <GNSS.h>
#include <stdarg.h>
#include <vector>
#include <algorithm>
#include <tuple>

#define MY_TIMEZONE_IN_SECONDS (9 * 60 * 60)
#define STRING_BUFFER_SIZE  128
#define RESTART_CYCLE       (60 * 5)

enum ParamSat {
  eSatGps,
  eSatGlonass,
  eSatGpsSbas,
  eSatGpsGlonass,
  eSatGpsBeidou,
  eSatGpsGalileo,
  eSatGpsQz1c,
  eSatGpsGlonassQz1c,
  eSatGpsBeidouQz1c,
  eSatGpsGalileoQz1c,
  eSatGpsQz1cQz1S,
};

//====================================
//
// spreGnssRtcLib
//
//====================================
class spreGnssRtcLib {
  public:
    spreGnssRtcLib();
    ~spreGnssRtcLib();

    //================================
    // begin
    //================================
    void begin() {
      Gnss.setDebugMode(PrintNone);
      int result = Gnss.begin();
      if (result != 0) {
        debugPrint("Gnss begin error!!");
        error_flag = 1;
      } else {
        switch (satType) {
          case eSatGps: Gnss.select(GPS); break;
          case eSatGpsSbas: Gnss.select(GPS); Gnss.select(SBAS); break;
          case eSatGlonass: Gnss.select(GLONASS); Gnss.deselect(GPS); break;
          case eSatGpsGlonass: Gnss.select(GPS); Gnss.select(GLONASS); break;
          case eSatGpsBeidou: Gnss.select(GPS); Gnss.select(BEIDOU); break;
          case eSatGpsGalileo: Gnss.select(GPS); Gnss.select(GALILEO); break;
          case eSatGpsQz1c: Gnss.select(GPS); Gnss.select(QZ_L1CA); break;
          case eSatGpsQz1cQz1S: Gnss.select(GPS); Gnss.select(QZ_L1CA); Gnss.select(QZ_L1S); break;
          case eSatGpsBeidouQz1c: Gnss.select(GPS); Gnss.select(BEIDOU); Gnss.select(QZ_L1CA); break;
          case eSatGpsGalileoQz1c: Gnss.select(GPS); Gnss.select(GALILEO); Gnss.select(QZ_L1CA); break;
          case eSatGpsGlonassQz1c:
          default: Gnss.select(GPS); Gnss.select(GLONASS); Gnss.select(QZ_L1CA); break;
        }

        result = Gnss.start(COLD_START);
        if (result != 0) {
          debugPrint("Gnss start error!!");
          error_flag = 1;
        } else {
          debugPrint("Gnss setup OK");
        }
      }

      if (error_flag == 1) {
        exit(0);
      }
    }

    //================================
    // update
    //================================
    void update() {
      if (Gnss.waitUpdate(-1)) {
        SpNavData NavData;
        Gnss.getNavData(&NavData);
        updateRTC(NavData);
        printPosition();
        printConditions();
      } else {
        debugPrint("data not update");
      }

      LoopCount++;
      if (LoopCount >= RESTART_CYCLE) {
        gnssRestart();
      }
    }

    //================================
    // updateRTC
    //================================
    void updateRTC(SpNavData &NavData) {
      SpGnssTime *time = &NavData.time;
      if (time->year >= 2000) {
        RtcTime gps(time->year, time->month, time->day,
                    time->hour, time->minute, time->sec, time->usec * 1000);
#ifdef MY_TIMEZONE_IN_SECONDS
        gps += MY_TIMEZONE_IN_SECONDS;
#endif
        RtcTime now = RTC.getTime();
        int diff = now - gps;
        if (abs(diff) >= 1) {
          RTC.setTime(gps);
          debugPrint("RTC updated to: %04d/%02d/%02d %02d:%02d:%02d",
                     gps.year(), gps.month(), gps.day(),
                     gps.hour(), gps.minute(), gps.second());
        }
      }
    }

    //================================
    // isPositionFixed
    //================================
    bool isPositionFixed() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);
      return (NavData.posFixMode != FixInvalid);
    }

    //================================
    // printPosition
    //================================
    void printPosition() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);

      debugPrint("%04d/%02d/%02d %02d:%02d:%02d.%06ld, numSat:%2d, %s",
                 NavData.time.year, NavData.time.month, NavData.time.day,
                 NavData.time.hour, NavData.time.minute, NavData.time.sec, NavData.time.usec,
                 NavData.numSatellites,
                 (NavData.posFixMode == FixInvalid) ? "No-Fix" : "Fix"
                );

      if (NavData.posDataExist == 0) {
        debugPrint("No Position");
      } else {
        debugPrint("Lat=%.6f, Lon=%.6f", NavData.latitude, NavData.longitude);
      }
    }

    //================================
    // printConditions
    //================================
    void printConditions() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);

      debugPrint("Number of Satellites: %2d", NavData.numSatellites);

      for (unsigned long cnt = 0; cnt < NavData.numSatellites; cnt++) {
        const char *pType = "---";
        SpSatelliteType sattype = NavData.getSatelliteType(cnt);

        switch (sattype) {
          case GPS: pType = "GPS"; break;
          case GLONASS: pType = "GLONASS"; break;
          case QZ_L1CA: pType = "QZ_L1CA"; break;
          case SBAS: pType = "SBAS"; break;
          case QZ_L1S: pType = "QZ_L1S"; break;
          case BEIDOU: pType = "BEIDOU"; break;
          case GALILEO: pType = "GALILEO"; break;
          default: pType = "UNKNOWN"; break;
        }

        unsigned long Id = NavData.getSatelliteId(cnt);
        unsigned long Elv = NavData.getSatelliteElevation(cnt);
        unsigned long Azm = NavData.getSatelliteAzimuth(cnt);
        float sigLevel = NavData.getSatelliteSignalLevel(cnt);

        debugPrint("[%2ld] Type: %s, ID: %2ld, Elevation: %2ld°, Azimuth: %3ld°, Signal Level: %.6f dB",
                   cnt, pType, Id, Elv, Azm, sigLevel);
      }
    }

    //================================
    // getSatelliteCount
    //================================
    int getSatelliteCount() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);
      return NavData.numSatellites;
    }

    //================================
    // setSftPos
    //================================
    void setSftPos(double lat, double lon) {
      sftLatitude = lat;
      sftLongitude = lon;
    }

    //================================
    // setSatelliteType
    //================================
    void setSatelliteType(int type) {
      if (type < eSatGps || type > eSatGpsQz1cQz1S) {
        debugPrint("Invalid satellite type.");
        return;
      }
      satType = static_cast<ParamSat>(type);
    }

    //================================
    // getLatitude
    //================================
    double getLatitude() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);
      return NavData.posDataExist ? NavData.latitude - sftLatitude : 0.0;
    }

    //================================
    // getLongitude
    //================================
    double getLongitude() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);
      return NavData.posDataExist ? NavData.longitude - sftLongitude : 0.0;
    }

    //================================
    // setDebugMode
    //================================
    void setDebugMode(bool enable) {
      debugMode = enable;
    }

    //================================
    // isPosDataExist
    //================================
    bool isPosDataExist() {
      SpNavData NavData;
      Gnss.getNavData(&NavData);
      return NavData.posDataExist != 0;
    }

    //================================
    // skyPlot
    //================================
    void skyPlot(int w, int h) {
      SpNavData NavData;
      Gnss.getNavData(&NavData);
      std::vector<std::tuple<float, unsigned long, float, float>> satelliteData;

      for (unsigned long cnt = 0; cnt < NavData.numSatellites; cnt++) {
        if (NavData.posDataExist) {
          float signalLevel = NavData.getSatelliteSignalLevel(cnt);
          unsigned long satId = NavData.getSatelliteId(cnt);
          if (signalLevel < 0) {
            continue;
          }
          float elevation = NavData.getSatelliteElevation(cnt);
          float azimuth = NavData.getSatelliteAzimuth(cnt);
          satelliteData.push_back(std::make_tuple(signalLevel, satId, azimuth, elevation));
        }
      }

      std::sort(satelliteData.begin(), satelliteData.end(),
      [](const std::tuple<float, unsigned long, float, float>& a, const std::tuple<float, unsigned long, float, float>& b) {
        return std::get<0>(a) > std::get<0>(b);
      });

      std::vector<std::vector<char>> skyPlotArray(h, std::vector<char>(w, '.'));

      int verticalLineX = w / 2;
      for (int i = 0; i < h; i++) {
        skyPlotArray[i][verticalLineX] = '|';
      }

      for (const auto& sat : satelliteData) {
        float signalLevel = std::get<0>(sat);
        unsigned long satId = std::get<1>(sat);
        float azimuth = std::get<2>(sat);
        float elevation = std::get<3>(sat);

        int x = static_cast<int>(azimuth / 360.0 * w);
        int y = static_cast<int>((90.0 - elevation) / 90.0 * h);

        if (x >= 0 && x < w && y >= 0 && y < h) {
          char ids[4];
          snprintf(ids, sizeof(ids), "%02lu", satId);

          if (x + 1 < w) {
            skyPlotArray[y][x] = ids[0];
            skyPlotArray[y][x + 1] = ids[1];
          } else {
            skyPlotArray[y][x] = ids[0];
          }
        }
      }

      for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
          Serial.print(skyPlotArray[i][j]);
        }
        Serial.println();
      }
    }

  private:
    //================================
    // gnssRestart
    //================================
    void gnssRestart() {
      int error_flag = 0;
      if (Gnss.stop() != 0) {
        debugPrint("Gnss stop error!!");
        error_flag = 1;
      } else if (Gnss.end() != 0) {
        debugPrint("Gnss end error!!");
        error_flag = 1;
      } else {
        debugPrint("Gnss stop OK.");
      }

      if (Gnss.begin() != 0) {
        debugPrint("Gnss begin error!!");
        error_flag = 1;
      } else if (Gnss.start(HOT_START) != 0) {
        debugPrint("Gnss start error!!");
        error_flag = 1;
      } else {
        debugPrint("Gnss restart OK.");
      }
      LoopCount = 0;
      if (error_flag == 1) {
        exit(0);
      }
    }

    //================================
    // debugPrint
    //================================
    void debugPrint(const char *format, ...) {
      if (debugMode) {
        char buffer[STRING_BUFFER_SIZE];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.println(buffer);
      }
    }

    SpGnss Gnss;
    int error_flag = 0;
    int LoopCount = 0;
    bool debugMode = false;
    double sftLatitude = 0.0f;
    double sftLongitude = 0.0f;
    ParamSat satType = eSatGpsQz1cQz1S;
};

//================================
// spreGnssRtcLib
//================================
spreGnssRtcLib::spreGnssRtcLib()
  : error_flag(0), LoopCount(0), debugMode(false),
    sftLatitude(0.0), sftLongitude(0.0), satType(eSatGpsQz1cQz1S) {
}

//================================
// ~spreGnssRtcLib
//================================
spreGnssRtcLib::~spreGnssRtcLib() {
}

#endif // __SPREGNSSRTC_LIB_H__
