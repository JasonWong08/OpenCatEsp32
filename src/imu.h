bool calibrateQ = false;
float ypr[3];
float previous_ypr[3];
#define IMU_SKIP 1
#define IMU_SKIP_MORE 23  // use prime number to avoid repeatly skipping the same joint
#define ARX xyzReal[0]
#define ARY xyzReal[1]
#define ARZ xyzReal[2]
#define AWX aaWorld.x
#define AWY aaWorld.y
#define AWZ aaWorld.z
#define GRAVITY 10.0
float gFactor = GRAVITY / 8192;
byte imuSkip = IMU_SKIP;
float previousXYZ[3];
int8_t yprTilt[3];
float xyzReal[3];
int thresX, thresY, thresZ;
#ifdef IMU_MPU6050

// I2C device class (I2Cdev) demonstration Arduino sketch for MPU6050 class using DMP (MotionApps v6.12)
// 6/21/2012 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib
//
// Changelog:
//      2019-07-10 - Uses the new version of the DMP Firmware V6.12
//                 - Note: I believe the Teapot demo is broken with this versin as
//                 - the fifo buffer structure has changed
//      2016-04-18 - Eliminated a potential infinite loop
//      2013-05-08 - added seamless Fastwire support
//                 - added note about gyro calibration
//      2012-06-21 - added note about Arduino 1.0.1 + Leonardo compatibility error
//      2012-06-20 - improved FIFO overflow handling and simplified read process
//      2012-06-19 - completely rearranged DMP initialization code and simplification
//      2012-06-13 - pull gyro and accel data from FIFO packet instead of reading directly
//      2012-06-09 - fix broken FIFO read sequence and change interrupt detection to RISING
//      2012-06-05 - add gravity-compensated initial reference frame acceleration output
//                 - add 3D math helper file to DMP6 example sketch
//                 - add Euler output and Yaw/Pitch/Roll output formats
//      2012-06-04 - remove accel offset clearing for better results (thanks Sungon Lee)
//      2012-06-01 - fixed gyro sensitivity to be 2000 deg/sec instead of 250
//      2012-05-30 - basic DMP initialization working

/* ============================================
  I2Cdev device library code is placed under the MIT license
  Copyright (c) 2012 Jeff Rowberg

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
  ===============================================
*/

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "mpu6050/src/I2Cdev.h"

#include "mpu6050/src/MPU6050_6Axis_MotionApps_V6_12.h"
// #include "MPU6050.h" // not necessary if using MotionApps include file

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

/* =========================================================================
   NOTE: In addition to connection 3.3v, GND, SDA, and SCL, this sketch
   depends on the MPU-6050's INT pin being connected to the Arduino's
   external interrupt #0 pin. On the Arduino Uno and Mega 2560, this is
   digital I/O pin 2.
   ========================================================================= */

/* =========================================================================
   NOTE: Arduino v1.0.1 with the Leonardo board generates a compile error
   when using Serial.write(buf, len). The Teapot output uses this method.
   The solution requires a modification to the Arduino USBAPI.h file, which
   is fortunately simple, but annoying. This will be fixed in the next IDE
   release. For more info, see these links:

   http://arduino.cc/forum/index.php/topic,109987.0.html
   http://code.google.com/p/arduino/issues/detail?id=958
   ========================================================================= */

// uncomment "OUTPUT_READABLE_QUATERNION" if you want to see the actual
// quaternion components in a [w, x, y, z] format (not best for parsing
// on a remote host such as Processing or something though)
// #define OUTPUT_READABLE_QUATERNION

// uncomment "OUTPUT_READABLE_EULER" if you want to see Euler angles
// (in degrees) calculated from the quaternions coming from the FIFO.
// Note that Euler angles suffer from gimbal lock (for more info, see
// http://en.wikipedia.org/wiki/Gimbal_lock)
// #define OUTPUT_READABLE_EULER

// uncomment "OUTPUT_READABLE_YAWPITCHROLL" if you want to see the yaw/
// pitch/roll angles (in degrees) calculated from the quaternions coming
// from the FIFO. Note this also requires gravity vector calculations.
// Also note that yaw/pitch/roll angles suffer from gimbal lock (for
// more info, see: http://en.wikipedia.org/wiki/Gimbal_lock)
#define OUTPUT_READABLE_YAWPITCHROLL

// uncomment "OUTPUT_READABLE_REALACCEL" if you want to see acceleration
// components with gravity removed. This acceleration reference frame is
// not compensated for orientation, so +X is always +X according to the
// sensor, just without the effects of gravity. If you want acceleration
// compensated for orientation, us OUTPUT_READABLE_WORLDACCEL instead.
// #define OUTPUT_READABLE_REALACCEL
/* REALACCEL represents the acceleration related to the sensor.
   Even if the robot is not moving, it will be large if the sensor is tilted.
   Because gravity has a component in that tilted direction.
   It's useful to use aaReal.z to determine if the sensor is flipped up-side-down.
*/

// uncomment "OUTPUT_READABLE_WORLDACCEL" if you want to see acceleration
// components with gravity removed and adjusted for the world frame of
// reference (yaw is relative to initial orientation, since no magnetometer
// is present in this case). Could be quite handy in some cases.
#define OUTPUT_READABLE_WORLDACCEL
/* WORLDACCEL represents the acceleration related to the world reference.
   It will be close to zero as long as the robot is not moved.
   It's useful to detect the wobbling about the sensor's original position.
*/

// uncomment "OUTPUT_TEAPOT" if you want output that matches the
// format used for the InvenSense teapot demo
// #define OUTPUT_TEAPOT
class mpu6050p : public MPU6050 {

  // MPU control/status vars
  bool dmpReady = false;   // set true if DMP init was successful
  uint8_t mpuIntStatus;    // holds actual interrupt status byte from MPU
  uint8_t devStatus;       // return status after each device operation (0 = success, !0 = error)
  uint16_t packetSize;     // expected DMP packet size (default is 42 bytes)
  uint16_t fifoCount;      // count of all bytes currently in FIFO
  uint8_t fifoBuffer[64];  // FIFO storage buffer

  // orientation/motion vars
  Quaternion q;         // [w, x, y, z]         quaternion container
  VectorInt16 aa;       // [x, y, z]            accel sensor measurements
  VectorInt16 gy;       // [x, y, z]            gyro sensor measurements
  VectorFloat gravity;  // [x, y, z]            gravity vector
  float euler[3];       // [psi, theta, phi]    Euler angle container
public:
  VectorInt16 aaReal;   // [x, y, z]            gravity-free accel sensor measurements
  VectorInt16 aaWorld;  // [x, y, z]            world-frame accel sensor measurements
  int16_t *xyzReal[3] = { &aaReal.x, &aaReal.y, &aaReal.z };
  float ypr[3];     // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector. unit is radian
  float a_real[3];  // [x, y, z]            gravity vector in the real world

  // packet structure for InvenSense teapot demo
  uint8_t teapotPacket[14] = { '$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, '\r', '\n' };

  // ================================================================
  // ===               INTERRUPT DETECTION ROUTINE                ===
  // ================================================================

  // volatile bool mpuInterrupt = false;  // indicates whether MPU interrupt pin has gone high
  // void dmpDataReady() {
  //   mpuInterrupt = true;
  // }

  // The REALACCEL numbers are calculated with respect to the orientation of the sensor itself, so that if it is flat and you move it straight up, the "Z" accel will change, but if you flip it up on one side and move it in the new relative "up" direction (along the sensor's Z axis), it will still register acceleration on the Z axis. Essentially, it is sensor-oriented acceleration which removes the effects of gravity and any non-flat/level orientation.

  // The WORLDACCEL numbers are calculated to ignore orientation. Moving it straight up while flat will look the same as the REALACCEL numbers, but if you then flip it upside-down and do the exact same movement ("up" with respect to you), you'll get exactly the same numbers as before, even though the sensor itself is upside-down.

  bool read_mpu6050() {
    if (!dmpReady)
      return false;
    if (dmpGetCurrentFIFOPacket(fifoBuffer)) {  // Get the Latest packet
      // display Euler angles in degrees
      dmpGetQuaternion(&q, fifoBuffer);
      dmpGetAccel(&aa, fifoBuffer);
      dmpGetEuler(euler, &q);
      dmpGetGravity(&gravity, &q);
      dmpGetYawPitchRoll(ypr, &q, &gravity);
      dmpGetLinearAccel(&aaReal, &aa, &gravity);
      dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

      for (byte i = 0; i < 3; i++) {  // no need to flip yaw
        ypr[i] *= degPerRad;
        a_real[i] = *xyzReal[i] / 8192.0 * GRAVITY;
#ifdef BiBoard_V0_1
        ypr[i] = -ypr[i];
        if (i != 2)
          a_real[i] = -a_real[i];
#endif
      }
      // imuException = aaReal.z < 0 && fabs(ypr[2]) > 85;  //the second condition is used to filter out some noise

      // Acceleration Real
      //      ^ head
      //        ^ x+
      //        |
      //  y+ <------ y-
      //        |
      //        | x-
      // if (AWZ < -8500 && AWZ > -8600)
      //   imuException = -1;  //dropping
      // else
      if (a_real[2] < 0 && fabs(ypr[2]) > 85)  //  imuException = aaReal.z < 0;
        imuException = -2;                     // flipped
#ifndef ROBOT_ARM
      else if (!moduleDemoQ && abs(a_real[0] - previousXYZ[0]) > 6000 * gFactor && abs(a_real[1] - previousXYZ[1]) > 6000 * gFactor && abs(a_real[2] - previousXYZ[2]) > 6000 * gFactor)
        imuException = -3;
      else if (!moduleDemoQ && (abs(a_real[0] - previousXYZ[0]) > 6000 * gFactor && abs(a_real[0]) > thresX * gFactor || abs(a_real[1] - previousXYZ[1]) > 5000 * gFactor && abs(a_real[1]) > thresY * gFactor))
        imuException = -4;
#endif
      // else if (  //keepDirectionQ &&
      //   abs(previous_ypr[0] - ypr[0]) > 15 && abs(abs(ypr[0] - previous_ypr[0]) - 360) > 15)
      //   imuException = -5;
      else
        imuException = 0;
      // however, its change is very slow.
      for (byte m = 0; m < 3; m++) {
        previousXYZ[m] = a_real[m];
        if (abs(ypr[0] - previous_ypr[0]) < 2 || abs(abs(ypr[0] - previous_ypr[0]) - 360) < 2) {
          previous_ypr[m] = ypr[m];
        }
      }
      return true;
    }
    return false;
  }

  void mpu6050Setup(bool calibrateQ = true) {
    // join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.setClock(400000);  // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
#endif

    // initialize serial communication
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)
    // Serial.begin(115200);
    // while (!Serial)
    // ;  // wait for Leonardo enumeration, others continue immediately

    // NOTE: 8MHz or slower host processors, like the Teensy @ 3.3V or Arduino
    // Pro Mini running at 3.3V, cannot handle this baud rate reliably due to
    // the baud timing being too misaligned with processor ticks. You must use
    // 38400 or slower in these cases, or use some kind of external separate
    // crystal solution for the UART timer.
    int connectAttempt = 0;
    do {
      // initialize device
      PTLF("\nInitializing MPU6050...");
#if defined CONFIG_DISABLE_HAL_LOCKS && CONFIG_DISABLE_HAL_LOCKS == 1
      PTL("OK");
      PTL("If the program stucks, reinstall Arduino ESP32 boards version 2.0.12. Newer version may cause bugs!");
#else
      PTL("If the program stucks, modify the header file:\n  https://docs.petoi.com/arduino-ide/upload-sketch-for-biboard#sdkconfig.h");
#endif
      initialize();
      // pinMode(INTERRUPT_PIN, INPUT);
      // verify connection
      PTF("- Testing MPU connections...attempt ");
      PTL(connectAttempt++);
      delay(500);
    } while (!testConnection());
    PTLF("- MPU6050 connection successful");

    // load and configure the DMP
    PTLF("- Initializing DMP...");

    devStatus = dmpInitialize();
    PT("MPU offsets: ");
    for (byte m = 0; m < 6; m++) {
#ifdef I2C_EEPROM_ADDRESS
      mpuOffset[m] = i2c_eeprom_read_int16(EEPROM_MPU + m * 2);
#else
      mpuOffset[m] = config.getShort(("mpu" + String(m)).c_str());
#endif
      PTT(mpuOffset[m], '\t');
    }
    PTL();
    // supply the gyro offsets here, scaled for min sensitivity
    setXAccelOffset(mpuOffset[0]);
    setYAccelOffset(mpuOffset[1]);
    setZAccelOffset(mpuOffset[2]);  // gravity
    setXGyroOffset(mpuOffset[3]);   // yaw
    setYGyroOffset(mpuOffset[4]);   // pitch
    setZGyroOffset(mpuOffset[5]);   // roll

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
      // Calibration Time: generate offsets and calibrate our MPU6050
      if (calibrateQ) {
        PTLF("Calibrate MPU6050...");
        CalibrateAccel(20);
        CalibrateGyro(20);
#ifdef I2C_EEPROM_ADDRESS
        i2c_eeprom_write_int16(EEPROM_MPU, getXAccelOffset());
        i2c_eeprom_write_int16(EEPROM_MPU + 2, getYAccelOffset());
        i2c_eeprom_write_int16(EEPROM_MPU + 4, getZAccelOffset());
        i2c_eeprom_write_int16(EEPROM_MPU + 6, getXGyroOffset());
        i2c_eeprom_write_int16(EEPROM_MPU + 8, getYGyroOffset());
        i2c_eeprom_write_int16(EEPROM_MPU + 10, getZGyroOffset());
#else
        config.putShort("mpu0", getXAccelOffset());
        config.putShort("mpu1", getYAccelOffset());
        config.putShort("mpu2", getZAccelOffset());
        config.putShort("mpu3", getXGyroOffset());
        config.putShort("mpu4", getYGyroOffset());
        config.putShort("mpu5", getZGyroOffset());

#endif
        PrintActiveOffsets();
      }
      // turn on the DMP, now that it's ready
      PTLF("- Enabling DMP...");
      setDMPEnabled(true);

      // enable Arduino interrupt detection
      // PTF("- Enabling interrupt detection (Arduino external interrupt ");
      // PT(digitalPinToInterrupt(INTERRUPT_PIN));
      // PTLF(")...");
      // attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
      mpuIntStatus = getIntStatus();

      // set our DMP Ready flag so the main loop() function knows it's okay to use it
      PTLF("- DMP ready! Waiting for the first interrupt...");
      dmpReady = true;

      // get expected DMP packet size for later comparison
      packetSize = dmpGetFIFOPacketSize();
    } else {
      // ERROR!
      // 1 = initial memory load failed
      // 2 = DMP configuration updates failed
      // (if it's going to break, usually the code will be 1)
      PTF("- DMP Initialization failed (code ");
      PT(devStatus);
      PTLF(")");
    }

    delay(10);
    read_mpu6050();
  }
  void print6AxisMacro() {
#ifdef OUTPUT_READABLE_QUATERNION
    // display quaternion values in easy matrix form: w x y z
    PT("quat\t");
    PT(q.w);
    PT("\t");
    PT(q.x);
    PT("\t");
    PT(q.y);
    PT("\t");
    PT(q.z);
    PT("\t");
#endif

#ifdef OUTPUT_READABLE_EULER
    // display Euler angles in degrees
    PT("euler\t");
    PT(euler[0] * 180 / M_PI);
    PT("\t");
    PT(euler[1] * 180 / M_PI);
    PT("\t");
    PT(euler[2] * 180 / M_PI);
    PT("\t");
#endif

#ifdef OUTPUT_READABLE_YAWPITCHROLL
    // display angles in degrees
    PT("ypr\t");
    PT(ypr[0]);
    PT("\t");
    PT(ypr[1]);
    PT("\t");
    PT(ypr[2]);
    PT("\t");
    /*
      dmpGetAccel(&aa, fifoBuffer);
      PT("\tRaw Accl XYZ\t");
      PT(aa.x);
      PT("\t");
      PT(aa.y);
      PT("\t");
      PT(aa.z);
      dmpGetGyro(&gy, fifoBuffer);
      PT("\tRaw Gyro XYZ\t");
      PT(gy.x);
      PT("\t");
      PT(gy.y);
      PT("\t");
      PT(gy.z);
      PT("\t");
    */
#endif

#ifdef OUTPUT_READABLE_WORLDACCEL
    // display initial world-frame acceleration, adjusted to remove gravity
    // and rotated based on known orientation from quaternion
    PT("aworld\t");
    PT(aaWorld.x);
    PT("\t");
    PT(aaWorld.y);
    PT("\t");
    PT(aaWorld.z);
    PT("\t");
#endif

#ifdef OUTPUT_READABLE_REALACCEL
    // display real acceleration, adjusted to remove gravity
    PT("areal\t");
    PT(aaReal.x);
    PT("\t");
    PT(aaReal.y);
    PT("\t");
#endif
    PT("areal.z\t");
    PT(aaReal.z);  // becomes negative when flipped
    PT("\t");

    PTL();
  }
};

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation board)
// AD0 high = 0x69
mpu6050p mpu;
// MPU6050 mpu(0x69); // <-- use for AD0 high
#endif

#ifdef IMU_ICM42670
#include "icm42670/petoi_icm42670p.h"

imu42670p icm(Wire, 1);

void icm42670Setup(bool calibrateQ = true) {
  PTLF("\nInitializing ICM42670...");
  icm.begin();
  icm.init(200, 2, 250);
  // Wait icm to start
  delay(100);

  // Calibration Time: generate offsets and calibrate our MPU6050
  if (calibrateQ) {
    PTLF("Calibrate ICM42670...");
    icm.getOffset(2000);
    config.putFloat("icm_accel0", icm.offset_accel[0]);
    config.putFloat("icm_accel1", icm.offset_accel[1]);
    config.putFloat("icm_accel2", icm.offset_accel[2]);
    config.putFloat("icm_gyro0", icm.offset_gyro[0]);
    config.putFloat("icm_gyro1", icm.offset_gyro[1]);
    config.putFloat("icm_gyro2", icm.offset_gyro[2]);
  } else {
    Serial.println("calibration already done");
    icm.offset_accel[0] = config.getFloat("icm_accel0");
    icm.offset_accel[1] = config.getFloat("icm_accel1");
    icm.offset_accel[2] = config.getFloat("icm_accel2");
    icm.offset_gyro[0] = config.getFloat("icm_gyro0");
    icm.offset_gyro[1] = config.getFloat("icm_gyro1");
    icm.offset_gyro[2] = config.getFloat("icm_gyro2");
  }
  PT("ICM offsets: ");
  for (byte i = 0; i < 3; i++)
    PTT(icm.offset_accel[i], '\t');
  for (byte i = 0; i < 3; i++)
    PTT(icm.offset_gyro[i], '\t');
  PTL();
}
#endif

// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

#define PRINT_ACCELERATION
void print6Axis() {
  char buffer[45];  // Adjust buffer size as needed
#ifdef IMU_MPU6050
#ifdef PRINT_ACCELERATION
  sprintf(buffer, "%7.1f %7.1f %7.1f %5.1f %5.1f %5.1f",                                     // 7x6 = 42
          mpu.ypr[0], mpu.ypr[1], mpu.ypr[2], mpu.a_real[0], mpu.a_real[1], mpu.a_real[2]);  //, aaWorld.z);
#else
  sprintf(buffer, "%7.1f %7.1f %7.1f", mpu.ypr[0], mpu.ypr[1], mpu.ypr[2]);
#endif
  printToAllPorts(buffer, 0);
#endif

#ifdef IMU_ICM42670
#ifdef PRINT_ACCELERATION
  sprintf(buffer, "%7.1f %7.1f %7.1f %5.1f %5.1f %5.1f",  //
          icm.ypr[0], icm.ypr[1], icm.ypr[2], icm.a_real[0], icm.a_real[1], icm.a_real[2]);
#else
  sprintf(buffer, "%7.1f %7.1f %7.1f", icm.ypr[0], icm.ypr[1], icm.ypr[2]);
#endif
  printToAllPorts(buffer, 0);
#endif
  PTL();

  //   PT_FMT(ypr[0], 2);
  //   PT('\t');
  //   PT_FMT(ypr[1], 2);
  //   PT('\t');
  //   PT_FMT(ypr[2], 2);
  // #ifdef PRINT_ACCELERATION
  //   PT("\t");
  //   // PT(aaWorld.x);
  //   // PT("\t");
  //   // PT(aaWorld.y);
  //   PT(*xyzReal[0]);  // x is along the longer direction of the robot
  //   PT("\t");
  //   PT(*xyzReal[1]);
  //   PT('\t');
  //   PT(*xyzReal[2]);
  //   PT("\t");
  //   PT(aaWorld.z);
  // #endif
  //   PTL();
}

bool readIMU() {
  bool updated = false;
  if (gyroUpdateQ && !(frame % imuSkip)) {
#ifdef IMU_MPU6050
    // if programming failed, don't try to do anything

    // read a packet from FIFO
    updated |= mpu.read_mpu6050();
    for (byte i = 0; i < 3; i++) {
      ypr[i] = mpu.ypr[i];
      xyzReal[i] = mpu.a_real[i];
    }
#endif
#ifdef IMU_ICM42670
    icm.getImuGyro();
    for (byte i = 0; i < 3; i++)
      icm.a_real[i] *= GRAVITY;
#endif
    return updated;
  } else {
    delay(1);  // to avoid the task to be blocked the wdt
    return false;
  }
}

long imuTime = 0;
void taskIMU(void *parameter) {
  while (true) {
    if (millis() - imuTime > 10) {
      imuUpdated = readIMU();
      imuTime = millis();
    } else
      delay(1);  // to avoid the task to be blocked the wdt
  }
}
TaskHandle_t TASK_imu = NULL;

void imuSetup() {
  if (newBoard) {
#ifndef AUTO_INIT
    PTL("- Calibrate the Inertial Measurement Unit (IMU)? (Y/n): ");
    char choice = getUserInputChar();
    PTL(choice);
    if (choice == 'Y' || choice == 'y')
      calibrateQ = true;
    if (calibrateQ) {
      PTLF("\nPut the robot FLAT on the table and don't touch it during calibration.");
      beep(8, 500, 500, 5);
    }
#endif
    beep(15, 500, 500, 1);
  }
#ifdef IMU_MPU6050
  // for (byte t = 0; t < 100; t++) {
  //   read_mpu6050();
  //   print6Axis();
  //   delay(2);
  // }
  mpu.mpu6050Setup(calibrateQ);
  imuException = mpu.aaReal.z < 0;
#endif
#ifdef IMU_ICM42670
  icm42670Setup(calibrateQ);
#endif
  if (calibrateQ)
    beep(18, 50, 50, 6);
  previous_ypr[0] = ypr[0];
#ifdef GYRO_PIN
  xTaskCreatePinnedToCore(
    taskIMU,    // task function
    "TaskIMU",  // name
    10000,      // task stack size​​
    NULL,       // parameters
    1,          // priority
    &TASK_imu,  // handle
    0);         // core
#endif
}
