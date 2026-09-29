#pragma once

#include <random>
#include <string>

#include "linux_serial_port.h"
#include "stm32_link_session.h"
#include "stm32_motion_session.h"
#include <chrono>
#include <optional>

extern "C"
{
#include "wheel_velocity_command.h"
#include "protocol_frame_decoder.h"
}

class Stm32ClientRunner
{
public:
    explicit Stm32ClientRunner(std::string device_path);

    [[nodiscard]] int run();
    void poll();
    void set_wheel_command(const WheelVelocityCommand& command);

    [[nodiscard]] bool is_synchronized() const;
private:
    enum class State
    {
        Disconnected,
        Synchronizing,
        Synchronized
    };

    [[nodiscard]] bool send_frame(const ProtocolFrame& frame);
    [[nodiscard]] bool begin_connection_epoch();
    [[nodiscard]] bool run_synchronizing_iteration();
    [[nodiscard]] bool run_synchronized_iteration();
    [[nodiscard]] bool has_fresh_wheel_command(std::chrono::steady_clock::time_point now) const;
    [[nodiscard]] static bool is_zero_wheel_command(const WheelVelocityCommand& command);
    void disconnect_current_epoch();
    void schedule_reconnect();

    ProtocolFrameDecoder decoder_ = {};
    std::chrono::steady_clock::time_point epoch_start_ = {};
    std::chrono::steady_clock::time_point sync_deadline_ = {};
    std::chrono::steady_clock::time_point reconnect_deadline_ = {};

    State state_ = State::Disconnected;
    bool transport_failed_ = false;
    std::string device_path_;

    LinuxSerialPort serial_port_;
    Stm32LinkSession link_session_;
    Stm32MotionSession motion_session_;
    std::uint32_t motion_session_id_ = 0U;

    std::mt19937_64 random_generator_;

    std::optional<WheelVelocityCommand> latest_wheel_command_;
    std::chrono::steady_clock::time_point latest_wheel_command_time_ = {};
    static constexpr std::chrono::milliseconds wheel_tx_period_{50};
    std::chrono::steady_clock::time_point last_wheel_transmit_time_ = {};
    static constexpr std::chrono::milliseconds wheel_command_freshness_{200};

};