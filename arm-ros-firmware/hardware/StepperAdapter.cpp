#include "arm_ros_firmware/StepperAdapter.hpp"

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>

constexpr boost::log::trivial::severity_level LOG_LEVEL = boost::log::trivial::debug;
constexpr uint32_t TOTAL_LOG_SIZE = 100 * 1024 * 1024; // 100 MiB

void StepperAdapter::init(
        const std::size_t NUM_JOINTS,
        rclcpp::Node& parentNode,
        const std::chrono::duration<int64_t, std::milli>& queryPeriod
) {
    // Setup logging
    boost::log::add_file_log(
            boost::log::keywords::file_name = "[%TimeStamp%]_%N.log",
            boost::log::keywords::rotation_size = TOTAL_LOG_SIZE,
            boost::log::keywords::format = "[%TimeStamp%]: %Message%",
            boost::log::keywords::auto_flush = true
    );
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= LOG_LEVEL);
    boost::log::add_common_attributes();
    BOOST_LOG_TRIVIAL(debug) << "Logging started";

    // Set the size of the vectors
    // See note in header about how important it is for these to not change
    this->positions.resize(NUM_JOINTS);
    this->velocities.resize(NUM_JOINTS);
    this->commands.resize(NUM_JOINTS);
    this->positions_buffer.resize(NUM_JOINTS);
    this->velocities_buffer.resize(NUM_JOINTS);

    // Start the polling loop
    this->polling_thread = std::thread(&StepperAdapter::poll, this);

    // Ask for callbacks for querying joint state
    // Note: This runs in our thread, so we don't have to worry about thread-safety
    parentNode.create_wall_timer(queryPeriod, [this] { this->queryController(); });

    // Register to receive callbacks for responses to getPosition and getSpeed
    // Note: These callbacks will occur in another thread, so they need to be processed carefully
    this->controller.EGetPosition.connect([this](uint8_t joint, int32_t pos) -> void { this->updatePosition(joint, pos); });
    this->controller.EGetSpeed.connect([this](uint8_t joint, int16_t speed) -> void { this->updateVelocity(joint, speed); });

    this->initialized = true;
}

void StepperAdapter::connect(const std::string device, const int baud_rate) {
    initializedCheck();
    this->controller.connect(device, baud_rate);
}

void StepperAdapter::disconnect() {
    initializedCheck();
    this->controller.disconnect();
}

double& StepperAdapter::getPositionRef(size_t index) {
    initializedCheck();
    return this->positions[index];
}

double& StepperAdapter::getVelocityRef(std::size_t index) {
    initializedCheck();
    return this->velocities[index];
}

double& StepperAdapter::getGripperPositionRef() {
    initializedCheck();
    return this->gripper_position;
}

double& StepperAdapter::getCommandRef(std::size_t index) {
    initializedCheck();
    return this->commands[index];
}

double& StepperAdapter::getGripperPositionCommandRef() {
    initializedCheck();
    return this->cmd_gripper_pos;
}

void StepperAdapter::setValues() {
    initializedCheck();
    for (auto i = 0u; i < commands.size(); ++i) {
        // Note that the StepperController speed is specified in units of in 1/10 RPM
        this->controller.setSpeed(i, (int16_t)std::round(10 * this->commands[i]));
    }

    this->controller.setGripper((uint8_t)std::round(this->cmd_gripper_pos));
}

void StepperAdapter::readValues() {
    // Acquire the locks for positions_buffer and velocities_buffer, and transfer the values
    {
        std::scoped_lock lock(this->positions_buffer_mx, this->velocities_buffer_mx);

        // Remember that we can't invalidate references, so we need to manually copy values
        for (auto i = 0u; i < commands.size(); ++i) {
            this->positions[i] = this->positions_buffer[i];
            this->velocities[i] = this->velocities_buffer[i];
        }
    }

    // Simply copy gripper position
    this->gripper_position = this->cmd_gripper_pos;
}

[[noreturn]] void StepperAdapter::poll() {
    // Run update loop forever
    // TODO: Look into a better way of doing the polling loop which isn't so intensive
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        this->controller.update();
    }
}

void StepperAdapter::queryController() {
    for (auto i = 0u; i < commands.size(); ++i) {
        this->controller.getPosition(i);
        this->controller.getSpeed(i);
    }
}

// Reminder: This happens in another thread
void StepperAdapter::updatePosition(const uint8_t joint, const int32_t position) {
    // Acquire the lock for positions_buffer and write the new value
    {
        std::scoped_lock lock(this->positions_buffer_mx);
        this->positions_buffer[joint] = position;
    }
}

// Reminder: This happens in another thread
void StepperAdapter::updateVelocity(const uint8_t joint, const int16_t speed) {
    // Acquire the lock for velocities_buffer and write the new value
    {
        std::scoped_lock lock(this->velocities_buffer_mx);
        this->velocities_buffer[joint] = speed;
    }
}

void StepperAdapter::initializedCheck() {
    if (!this->initialized) { throw std::logic_error("StepperAdapter not initialized"); }
}