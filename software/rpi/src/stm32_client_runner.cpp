#include "stm32_client_runner.h"
#include "protocol_frame_sender.h"

#include <utility>
#include <chrono>
#include <iostream>
#include <thread>

extern "C"
{
#include "protocol_frame_decoder.h"
}

Stm32ClientRunner::Stm32ClientRunner(
    std::string device_path)
    : device_path_(
        std::move(device_path)),
      random_generator_(
        std::random_device{}())
{
}

bool Stm32ClientRunner::send_frame(const ProtocolFrame& frame)
{
    return send_protocol_frame(
        serial_port_,
        frame,
        std::chrono::milliseconds(100));
}

void Stm32ClientRunner::set_wheel_command(const WheelVelocityCommand& command)
{
    latest_wheel_command_ = command;
    latest_wheel_command_time_ = std::chrono::steady_clock::now();
}

void Stm32ClientRunner::poll()
{
    if (state_ == State::Synchronized)
    {
        if (run_synchronized_iteration())
        {
            return;
        }

        disconnect_current_epoch();

        if (transport_failed_)
        {
            std::cerr
                << "STM32 link lost: serial transport disconnected"
                << std::endl;
        }

        std::cout
            << "Reconnecting in 500 ms..."
            << std::endl;

        schedule_reconnect();
        return;
    }

    if (state_ == State::Synchronizing)
    {
        if (run_synchronizing_iteration())
        {
            return;
        }

        if (state_ == State::Synchronized)
        {
            std::cout
                << "STM32 link synchronized"
                << std::endl;

            return;
        }

        disconnect_current_epoch();

        if (transport_failed_)
        {
            std::cerr
                << "STM32 link lost during synchronization"
                << std::endl;
        }
        else
        {
            std::cerr
                << "LINK_SYNC timed out"
                << std::endl;
        }

        schedule_reconnect();
        return;
    }

    if (state_ != State::Disconnected)
    {
        return;
    }

    const auto now =
        std::chrono::steady_clock::now();

    if (reconnect_deadline_ !=
            std::chrono::steady_clock::time_point{} &&
        now < reconnect_deadline_)
    {
        return;
    }

    reconnect_deadline_ = {};
    transport_failed_ = false;

    if (!begin_connection_epoch())
    {
        return;
    }
}

int Stm32ClientRunner::run()
{
    while (true)
    {
        poll();

        if (state_ == State::Disconnected &&
            reconnect_deadline_ !=
                std::chrono::steady_clock::time_point{})
        {
            const auto now =
                std::chrono::steady_clock::now();

            if (now < reconnect_deadline_)
            {
                std::this_thread::sleep_until(
                    reconnect_deadline_);
            }
        }
    }

    return 0;
}

bool Stm32ClientRunner::run_synchronized_iteration()
{
    const auto current_time =
        std::chrono::steady_clock::now();

    const auto elapsed_ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                current_time -
                epoch_start_)
            .count();

    const auto now_ms =
        static_cast<std::uint32_t>(
            elapsed_ms);

    link_session_.check_link_timeout(
        now_ms);

    if (link_session_.state() !=
        Stm32LinkState::Synchronized)
    {
        std::cerr
            << "STM32 link timed out"
            << std::endl;

        return false;
    }

    if (motion_session_.retry_exhausted(
            now_ms))
    {
        std::cerr
            << "MOTION lifecycle retries exhausted"
            << std::endl;

        return false;
    }

    const auto motion_retry =
        motion_session_.retry_if_due(
            now_ms);

    if (motion_retry.has_value())
    {
        if (!send_frame(
                motion_retry.value()))
        {
            transport_failed_ = true;
            return false;
        }

        const auto retry_time =
            std::chrono::steady_clock::now();

        const auto retry_elapsed_ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    retry_time -
                    epoch_start_)
                .count();

        motion_session_.mark_pending_transmitted(
            static_cast<std::uint32_t>(
                retry_elapsed_ms));

        std::cout
            << "MOTION lifecycle retry sent, sequence "
            << motion_retry->sequence
            << std::endl;
    }

    if (motion_session_.state() == Stm32MotionSession::State::Inactive &&
        has_fresh_wheel_command(current_time) &&
        !is_zero_wheel_command(latest_wheel_command_.value()))
    {
        do
        {
            motion_session_id_ =
                static_cast<std::uint32_t>(
                    random_generator_());
        }
        while (motion_session_id_ == 0U);

        const auto motion_start =
            motion_session_.begin_start_session(
                motion_session_id_);

        if (!motion_start.has_value())
        {
            std::cerr
                << "Failed to create MOTION_START"
                << std::endl;

            latest_wheel_command_.reset();
            latest_wheel_command_time_ = {};
            motion_session_id_ = 0U;

            return true;
        }

        if (!send_frame(
                motion_start.value()))
        {
            transport_failed_ = true;
            return false;
        }

        const auto transmit_time =
            std::chrono::steady_clock::now();

        const auto transmit_elapsed_ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    transmit_time -
                    epoch_start_)
                .count();

        motion_session_.mark_pending_transmitted(
            static_cast<std::uint32_t>(
                transmit_elapsed_ms));

        std::cout
            << "MOTION_START sent, sequence "
            << motion_start->sequence
            << ", session "
            << motion_session_id_
            << std::endl;
    }

    if (motion_session_.state() == Stm32MotionSession::State::Active &&
        has_fresh_wheel_command(current_time) &&
        !is_zero_wheel_command(latest_wheel_command_.value()) &&
        (last_wheel_transmit_time_ == std::chrono::steady_clock::time_point{} ||
            current_time - last_wheel_transmit_time_ >=wheel_tx_period_))
    {
        const WheelVelocityCommand& command =
            latest_wheel_command_.value();

        const auto wheel_frame =
            motion_session_.build_wheel_velocity(
                command.left_velocity_mm_s,
                command.right_velocity_mm_s);

        if (!wheel_frame.has_value())
        {
            std::cerr
                << "Failed to create WHEEL_VELOCITY"
                << std::endl;

            return true;
        }

        if (!send_frame(wheel_frame.value()))
        {
            transport_failed_ = true;
            return false;
        }

        last_wheel_transmit_time_ = std::chrono::steady_clock::now();
    }

    if (motion_session_.state() == Stm32MotionSession::State::Active &&
        (!has_fresh_wheel_command(current_time) ||
            is_zero_wheel_command(latest_wheel_command_.value())))
    {
        const auto motion_end =
            motion_session_.begin_end_session(
                motion_session_id_);

        if (!motion_end.has_value())
        {
            std::cerr
                << "Failed to create MOTION_END"
                << std::endl;

            return true;
        }

        if (!send_frame(motion_end.value()))
        {
            transport_failed_ = true;
            return false;
        }

        const auto transmit_time = std::chrono::steady_clock::now();

        const auto transmit_elapsed_ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    transmit_time -
                    epoch_start_)
                .count();

        motion_session_.mark_pending_transmitted(
            static_cast<std::uint32_t>(
                transmit_elapsed_ms));

        last_wheel_transmit_time_ = {};

        std::cout
            << "MOTION_END sent, sequence "
            << motion_end->sequence
            << ", session "
            << motion_session_id_
            << std::endl;
    }

    const auto heartbeat_frame =
        link_session_.heartbeat_if_due(
            now_ms);

    if (heartbeat_frame.has_value())
    {
        if (!send_frame(
                heartbeat_frame.value()))
        {
            transport_failed_ = true;
            return false;
        }

        std::cout
            << "HEARTBEAT sent, sequence "
            << heartbeat_frame->sequence
            << std::endl;
    }

    const LinuxSerialPollResult poll_result =
        serial_port_.wait(
            false,
            50);

    if (poll_result.disconnected)
    {
        transport_failed_ = true;
        return false;
    }

    if (!poll_result.readable)
    {
        return true;
    }

    std::uint8_t read_buffer[128] = {};

    const std::ptrdiff_t bytes_read =
        serial_port_.read_some(
            read_buffer,
            sizeof(read_buffer));

    if (bytes_read < 0)
    {
        transport_failed_ = true;
        return false;
    }

    for (std::ptrdiff_t i = 0;
         i < bytes_read;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (!protocol_frame_decoder_feed_byte(
                &decoder_,
                read_buffer[i],
                &frame))
        {
            continue;
        }

        const auto response_time =
            std::chrono::steady_clock::now();

        const auto response_elapsed_ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    response_time -
                    epoch_start_)
                .count();

        const auto response_now_ms =
            static_cast<std::uint32_t>(
                response_elapsed_ms);

        if (link_session_.handle_heartbeat_response(
                *frame,
                response_now_ms))
        {
            std::cout
                << "HEARTBEAT response accepted, sequence "
                << frame->sequence
                << std::endl;
        }
        else if (link_session_.state() ==
                 Stm32LinkState::Disconnected)
        {
            std::cerr
                << "STM32 reported link unsynchronized"
                << std::endl;

            return false;
        }
        else if (motion_session_.handle_ack(
                     *frame,
                     response_now_ms))
        {
            std::cout
                << "MOTION ACK accepted, sequence "
                << frame->sequence
                << std::endl;
        }
        else
        {
            const auto previous_motion_state =
                motion_session_.state();

            const auto motion_result =
                motion_session_.handle_response(
                    *frame);

            if (motion_result.has_value())
            {
                if (previous_motion_state ==
                    Stm32MotionSession::State::StartPending)
                {
                    std::cout
                        << "MOTION_START response, sequence "
                        << frame->sequence
                        << ", result "
                        << static_cast<unsigned>(
                            motion_result.value())
                        << std::endl;

                    if (motion_session_.state() !=
                        Stm32MotionSession::State::Active)
                    {
                        motion_session_id_ = 0U;
                        last_wheel_transmit_time_ = {};
                    }
                }
                else if (previous_motion_state ==
                         Stm32MotionSession::State::EndPending)
                {
                    std::cout
                        << "MOTION_END response, sequence "
                        << frame->sequence
                        << ", result "
                        << static_cast<unsigned>(
                            motion_result.value())
                        << std::endl;

                    motion_session_id_ = 0U;
                    last_wheel_transmit_time_ = {};
                }
            }
        }
    }

    return true;
}

bool Stm32ClientRunner::run_synchronizing_iteration()
{
    if (std::chrono::steady_clock::now() >=
        sync_deadline_)
    {
        return false;
    }

    const LinuxSerialPollResult poll_result =
        serial_port_.wait(
            false,
            50);

    if (poll_result.disconnected)
    {
        transport_failed_ = true;
        return false;
    }

    if (!poll_result.readable)
    {
        return true;
    }

    std::uint8_t read_buffer[128] = {};

    const std::ptrdiff_t bytes_read =
        serial_port_.read_some(
            read_buffer,
            sizeof(read_buffer));

    if (bytes_read < 0)
    {
        transport_failed_ = true;
        return false;
    }

    for (std::ptrdiff_t i = 0;
         i < bytes_read;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (!protocol_frame_decoder_feed_byte(
                &decoder_,
                read_buffer[i],
                &frame))
        {
            continue;
        }

        const auto current_time =
            std::chrono::steady_clock::now();

        const auto elapsed_ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    current_time -
                    epoch_start_)
                .count();

        if (link_session_.handle_link_sync_ok(
                *frame,
                static_cast<std::uint32_t>(
                    elapsed_ms)))
        {
            state_ = State::Synchronized;
            return false;
        }
    }

    return true;
}

bool Stm32ClientRunner::begin_connection_epoch()
{
    if (!serial_port_.open(
            device_path_))
    {
        std::cerr
            << "Serial device unavailable, retrying..."
            << std::endl;

        schedule_reconnect();
        return false;
    }

    std::cout
        << "Opened serial device: "
        << device_path_
        << std::endl;

    link_session_.disconnect();
    motion_session_.reset();
    motion_session_id_ = 0U;
    latest_wheel_command_.reset();
    latest_wheel_command_time_ = {};
    last_wheel_transmit_time_ = {};

    state_ = State::Disconnected;

    decoder_ = {};

    protocol_frame_decoder_init(
        &decoder_);

    epoch_start_ =
        std::chrono::steady_clock::now();

    const std::uint64_t sync_token =
        random_generator_();

    const ProtocolFrame sync_frame =
        link_session_.begin_synchronization(
            sync_token);

    if (!send_frame(
            sync_frame))
    {
        transport_failed_ = true;

        state_ = State::Disconnected;
        motion_session_.reset();
        link_session_.disconnect();
        serial_port_.close();

        std::cerr
            << "STM32 link lost while sending LINK_SYNC"
            << std::endl;

        schedule_reconnect();
        return false;
    }

    state_ = State::Synchronizing;

    std::cout
        << "LINK_SYNC sent"
        << std::endl;

    sync_deadline_ =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(1000);

    return true;
}

void Stm32ClientRunner::disconnect_current_epoch()
{
    state_ = State::Disconnected;

    link_session_.disconnect();
    motion_session_.reset();
    motion_session_id_ = 0U;

    latest_wheel_command_.reset();
    latest_wheel_command_time_ = {};
    last_wheel_transmit_time_ = {};

    serial_port_.close();
}

void Stm32ClientRunner::schedule_reconnect()
{
    reconnect_deadline_ =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(500);
}

bool Stm32ClientRunner::has_fresh_wheel_command(const std::chrono::steady_clock::time_point now) const
{
    if (!latest_wheel_command_.has_value())
    {
        return false;
    }

    return now - latest_wheel_command_time_ < wheel_command_freshness_;
}

bool Stm32ClientRunner::is_zero_wheel_command(const WheelVelocityCommand& command)
{
    return command.left_velocity_mm_s == 0 &&
           command.right_velocity_mm_s == 0;
}

bool Stm32ClientRunner::is_synchronized() const
{
    return state_ == State::Synchronized;
}