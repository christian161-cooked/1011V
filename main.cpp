#include "main.h"
#include "lemlib/api.hpp" // IWYU pragma: keep
#include "lemlib/chassis/chassis.hpp"
#include "pros/adi.hpp"
#include "pros/rtos.hpp"

// controller
pros::Controller controller(pros::E_CONTROLLER_MASTER);

// motor groups
pros::MotorGroup leftMotors({-11, -12, -2}, pros::MotorGearset::blue); // left motor group - ports 11 (reversed), 12 (reversed), 2 (reversed)
pros::MotorGroup rightMotors({8, 7, 13 }, pros::MotorGearset::blue); // right motor group - ports 8, 7, 13

// Inertial Sensor on port 4
pros::Imu imu(4);

// tracking wheel
// vertical tracking wheel encoder. Rotation sensor, port 11, reversed
pros::Rotation verticalEnc(14);
// vertical tracking wheel. 2" diameter, 0.5" offset, left of the robot (negative)
lemlib::TrackingWheel vertical(&verticalEnc, lemlib::Omniwheel::NEW_2, -0.5);

// drivetrain settings
lemlib::Drivetrain drivetrain(&leftMotors, // left motor group
                              &rightMotors, // right motor group
                              12, // 12 inch track width
                              lemlib::Omniwheel::NEW_325, // using new 3.25" omnis
                              450, // drivetrain rpm is 450
                              8 // horizontal drift is 2. If we had traction wheels, it would have been 8
);

// lateral motion controller for PID
lemlib::ControllerSettings linearController(5, // proportional gain (kP)
                                            0, // integral gain (kI)
                                            4, // derivative gain (kD)
                                            3, // anti windup
                                            1, // small error range, in inches
                                            100, // small error range timeout, in milliseconds
                                            3, // large error range, in inches
                                            500, // large error range timeout, in milliseconds
                                            20 // maximum acceleration (slew)
);

// angular motion controller for PID
lemlib::ControllerSettings angularController(2, // proportional gain (kP)
                                             0, // integral gain (kI)
                                             10, // derivative gain (kD)
                                             3, // anti windup
                                             1, // small error range, in degrees
                                             100, // small error range timeout, in milliseconds
                                             3, // large error range, in degrees
                                             500, // large error range timeout, in milliseconds
                                             0 // maximum acceleration (slew)
);

// sensors for odometry for tracking of robot
lemlib::OdomSensors sensors(&vertical, // vertical tracking wheel
                            nullptr, // vertical tracking wheel 2, set to nullptr as we don't have a second one
                            nullptr, // horizontal tracking wheel
                            nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
                            &imu // inertial sensor
);

// input curve for throttle input during driver control (less you move joystick slower it goes)
lemlib::ExpoDriveCurve throttleCurve(3, // joystick deadband out of 127
                                     10, // minimum output where drivetrain will move out of 127
                                     1.019 // expo curve gain
);

// input curve for steer input during driver control (less you move joystick slower it goes)
lemlib::ExpoDriveCurve steerCurve(3, // joystick deadband out of 127
                                  10, // minimum output where drivetrain will move out of 127
                                  1.019 // expo curve gain
);

// create the chassis/drivetrain
lemlib::Chassis chassis(drivetrain, linearController, angularController, sensors, &throttleCurve, &steerCurve);

/**
 * Runs initialization code. This occurs as soon as the program is started.
 *
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */
void initialize() {
    pros::lcd::initialize(); // initialize brain screen
    chassis.calibrate(); // calibrate sensors

    // the default rate is 50. however, if you need to change the rate, you
    // can do the following.
    // lemlib::bufferedStdout().setRate(...);
    // If you use bluetooth or a wired connection, you will want to have a rate of 10ms

    // for more information on how the formatting for the loggers
    // works, refer to the fmtlib docs

    // thread to for brain screen and position logging
    pros::Task screenTask([&]() {
        while (true) {
            // print robot location to the brain screen
            pros::lcd::print(0, "X: %f", chassis.getPose().x); // x
            pros::lcd::print(1, "Y: %f", chassis.getPose().y); // y
            pros::lcd::print(2, "Theta: %f", chassis.getPose().theta); // heading
            // log position telemetry
            lemlib::telemetrySink()->info("Chassis pose: {}", chassis.getPose());
            // delay to save resources
            pros::delay(50);
        }
    });
}

/**
 * Runs while the robot is disabled
 */
void disabled() {}

/**
 * runs after initialize if the robot is connected to field control
 */
void competition_initialize() {}

// get a path used for pure pursuit
// this needs to be put outside a function
ASSET(example_txt); // '.' replaced with "_" to make c++ happy

/**
 * Runs during auto
 *
 */
void autonomous() {
    pros::adi::DigitalOut piston ('A');
    pros::Motor motor (-10);
    chassis.setPose(-4, -2, -140);
    piston.set_value(true);
    motor.move(100);
    chassis.moveToPoint(4, 7, 1000, {.forwards = false});
    pros::delay(1000);
    piston.set_value(false);
    chassis.turnToPoint(11, 25, 1000);
    chassis.moveToPoint(11, 25, 1000);
}

//driving function called in driver control (used for code organization)
void drive() {
  leftMotors.move_velocity(10);
  rightMotors.move_velocity(10);
  while (true) {
    // get joystick positions
    int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
    int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
    // move the chassis with curvature drive
    chassis.arcade(leftY, rightX);
    pros::delay(20);
  }
}

//intake function called in driver control (used for code organization)
void intake(){
  pros::Motor motor (-10);
  while (true) {
  if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R1)) {
    motor.move(100);
  }
  else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)){
    motor.move(-100);
  }
  else
  {
    motor.move(0);
  }
    pros::delay(20);
  }
}

//clamp function called in driver control (used for code organization)
void clamp(){
  pros::adi::DigitalOut piston ('A');
  piston.set_value(true);
  bool isClamped = false;
  while(true){
    if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_L1)) isClamped = !isClamped;
    if (isClamped == true) piston.set_value(false);
    else piston.set_value(true);
    }
    pros::delay(20);

  }


/**
 * Runs in driver control
 */
void opcontrol() {
  //calls the functions
  pros::Task drive_task(drive);
  pros::Task intake_task(intake);
  pros::Task clamp_task(clamp);

}
