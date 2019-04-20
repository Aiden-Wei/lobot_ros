#ifndef XARM_DRIVER_H
#define XARM_DRIVER_H

#include <ros/console.h>
#include <ros/ros.h>
#include <array>
#include <initializer_list>
#include <vector>

#include "hid/myhid.hpp"

namespace lobot_hardware_interface {

#define SERVO_NUM 6   // Number of servos
#define JOINT_NUM 5   // Number of the arm joints
#define GRIPPER_ID 5  // Index of the gripper joint

#define CMD_MULT_SERVO_SPIN 3
#define CMD_MULT_SERVO_POS_READ 21

class XArmDriver {
 public:
  XArmDriver();
  ~XArmDriver();

  void Execute(const std::array<double, SERVO_NUM>& cmd,
               const ros::Duration& period);
  void GetJointState(std::array<double, SERVO_NUM>& currStateArray);

 private:
  MyHid myHid_;
  std::array<int, SERVO_NUM> currPosArray_;

  void GetCurrentPosition();
  void Init();
  void SpinServos(const std::initializer_list<unsigned>& idList,
                  const std::initializer_list<int>& posList,
                  const unsigned period = 2000);
};

inline void XArmDriver::Execute(const std::array<double, SERVO_NUM>& cmd,
                                const ros::Duration& period) {
  std::array<int, SERVO_NUM> posCmdArray;

  // Commands for arm joints, convert radians to positions
  for (std::size_t i = 0; i != JOINT_NUM; ++i) {
    posCmdArray[i] = (i == 2 || i == 3) ? (500 - 750 * cmd[i] / M_PI)
                                        : (500 + 750 * cmd[i] / M_PI);
  }

  // Command for gripper joint
  auto gripperCmd = (0.03 - cmd[GRIPPER_ID]) * 2000;
  posCmdArray[GRIPPER_ID] = -0.0031 * gripperCmd * gripperCmd * gripperCmd +
                            0.2122 * gripperCmd * gripperCmd -
                            10.3352 * gripperCmd + 700.9078;

  // Send commands
  SpinServos({2, 3, 4, 5, 6},
             {posCmdArray[4], posCmdArray[3], posCmdArray[2], posCmdArray[1],
              posCmdArray[0]},
             period.toSec() * 1000);
  SpinServos({1}, {posCmdArray[5]}, 1500);
}

inline void XArmDriver::GetJointState(
    std::array<double, SERVO_NUM>& currStateArray) {
  GetCurrentPosition();

  // State of arm joints, convert positions to radians
  for (std::size_t i = 1; i != SERVO_NUM; ++i) {
    currStateArray[JOINT_NUM - i] =
        (i == 2 || i == 3) ? ((500 - currPosArray_[i]) * M_PI / 750)
                           : ((currPosArray_[i] - 500) * M_PI / 750);
  }

  // State of the gripper joint, mapped from angle to distance
  currStateArray[GRIPPER_ID] =
      0.03 - (-0.0006 * currPosArray_[0] * currPosArray_[0] +
              0.1945 * currPosArray_[0] + 41.9166) /
                 2000;
}

inline void XArmDriver::GetCurrentPosition() {
  myHid_.MakeAndSendCmd(CMD_MULT_SERVO_POS_READ, {SERVO_NUM, 1, 2, 3, 4, 5, 6});
  auto recvDataVecPtr = myHid_.Read(21);

  if (recvDataVecPtr->size() != 0 &&
      recvDataVecPtr->at(0) == CMD_MULT_SERVO_POS_READ &&
      recvDataVecPtr->at(1) == SERVO_NUM) {
    auto currPos = currPosArray_.begin();
    decltype(recvDataVecPtr->size()) i = 0;
    while (currPos != currPosArray_.end()) {
      *currPos = static_cast<int>(recvDataVecPtr->at(3 * i + 3)) +
                 static_cast<int>(recvDataVecPtr->at(3 * i + 4) << 8);
      ++currPos;
      ++i;
    }
  }
}

inline void XArmDriver::SpinServos(
    const std::initializer_list<unsigned>& idList,
    const std::initializer_list<int>& posList, const unsigned period) {
  auto idListSize = idList.size();
  auto posListSize = posList.size();
  if (idListSize != posListSize) {
    return;
  }

  unsigned per = (period > 5000) ? 5000 : period;
  std::vector<unsigned> argv;
  argv.push_back(static_cast<unsigned>(idListSize));
  argv.push_back(per & 0xFF);
  argv.push_back((per >> 8) & 0xFF);
  auto id = idList.begin();
  auto position = posList.begin();
  while (id != idList.end()) {
    argv.push_back(*id);
    argv.push_back(*position & 0xFF);
    argv.push_back((*position >> 8) & 0xFF);
    ++id;
    ++position;
  }

  myHid_.MakeAndSendCmd(CMD_MULT_SERVO_SPIN, argv);
}

}  // namespace lobot_hardware_interface

#endif  // XARM_DRIVER_H
