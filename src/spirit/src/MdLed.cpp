#include "spirit/include/MdLed.h"

#include <climits>

#include "spirit/include/Error.h"

namespace spirit {

MdLed::MdLed(InterfaceDigitalOut &led0, InterfaceDigitalOut &led1) : _led0(led0), _led1(led1)
{
    mode(Default::blink_mode);
    state(Motor::Default::state);
}

void MdLed::state(const Motor::State type)
{
    _state = type;

    if (_mode == BlinkMode::Error) {
        return;
    }

    _mode = BlinkMode::Normal;
    switch (type) {
        case Motor::State::Coast:
            write(0);
            break;
        case Motor::State::CW:
            write(1);
            break;
        case Motor::State::CCW:
            write(2);
            break;
        case Motor::State::Brake:
            write(3);
            break;
        default:
            // 未定義の値
            constexpr char message_format[] = "Unknown motor state (%d)";
            char           message[sizeof(message_format) + Error::max_uint32_t_length];
            snprintf(message, sizeof(message), message_format, static_cast<uint32_t>(type));
            Error::get_instance().error(Error::Type::UnknownValue, 0, message, __FILE__, __func__, __LINE__);
            break;
    }
}

void MdLed::write(const uint32_t value)
{
    _led0.write(value & 1);
    _led1.write((value & 2) >> 1);
}

uint32_t MdLed::read() const
{
    return (_led1.read() << 1) + _led0.read();
}

void MdLed::mode(const BlinkMode type)
{
    if (_mode == type) {
        return;
    }

    switch (type) {
        case BlinkMode::Normal:
        case BlinkMode::Alternate:
        case BlinkMode::Concurrent:
        case BlinkMode::Error:
            break;
        default:
            constexpr char message_format[] = "Unknown blink mode (%d)";
            char           message[sizeof(message_format) + Error::max_uint32_t_length];
            snprintf(message, sizeof(message), message_format, static_cast<uint32_t>(type));
            Error::get_instance().error(Error::Type::UnknownValue, 0, message, __FILE__, __func__, __LINE__);
            return;
    }

    _counter = 0;
    _mode    = type;

    switch (_mode) {
        case BlinkMode::Alternate:
            alternately_blink();
            break;
        case BlinkMode::Concurrent:
            concurrently_blink();
            break;
        default:
            constexpr char message_format[] = "Unknown blink mode (%d)";
            char           message[sizeof(message_format) + Error::max_uint32_t_length];
            snprintf(message, sizeof(message), message_format, static_cast<uint32_t>(_mode));
            Error::get_instance().error(Error::Type::UnknownValue, 0, message, __FILE__, __func__, __LINE__);
            return;
    }
}

void MdLed::reset_error()
{
    if (_mode != BlinkMode::Error) {
        return;
    }

    _mode = BlinkMode::Normal;
    state(_state);
}

void MdLed::coordinate()
{
    if (_interval <= ++_counter) {
        _counter = 0;
        switch (_mode) {
            case BlinkMode::Normal:
                break;
            case BlinkMode::Alternate:
                alternately_blink();
                break;
            case BlinkMode::Concurrent:
                concurrently_blink();
                break;
            case BlinkMode::Error:
                error_blink();
                break;

                // default に来ることは、MdLed::mode でチェックされていて通常の利用ではありえないため、カバレッジ計測から除外する
                // LCOV_EXCL_START
            default:
                constexpr char message_format[] = "Unknown blink mode (%d)";
                char           message[sizeof(message_format) + Error::max_uint32_t_length];
                snprintf(message, sizeof(message), message_format, static_cast<uint32_t>(_mode));
                Error::get_instance().error(Error::Type::UnknownValue, 0, message, __FILE__, __func__, __LINE__);
                return;
                // LCOV_EXCL_STOP
        }
    }
}

void MdLed::blinking_rate(const uint32_t unit)
{
    _interval = unit;
}

void MdLed::error(const uint32_t status)
{
    if ((_mode == BlinkMode::Error) && (_error == status)) {
        return;
    }

    _mode          = BlinkMode::Error;
    _error         = status;
    _error_section = 0;
    error_blink();
}

void MdLed::alternately_blink()
{
    if (read() == 1) {
        write(2);
    } else {
        write(1);
    }
}

void MdLed::concurrently_blink()
{
    if (read() == 0) {
        write(3);
    } else {
        write(0);
    }
}

void MdLed::error_blink()
{
    if (_error_section == 0) {
        // _error を表現するために必要な最小ビット幅を計算する
        // C++20では、std::bit_width() を使えばいい
        // (現在はmbedがC++20に対応していないため自分で実装)
        //   _error = 1 の場合は 1
        //   _error = 5 の場合は 3
        uint32_t type_bit_size = sizeof(_error) * CHAR_BIT;
        _error_bit_width       = 1;
        for (auto i = type_bit_size - 1; i > 0; i--) {
            if (((_error >> i) & 1) == 1) {
                _error_bit_width = i + 1;
                break;
            }
        }
    }

    if (_error_section == 0) {
        write(3);
    } else if (_error_section % 2 == 0) {
        write(0);
    } else {
        uint32_t bit   = _error_section / 2;
        uint32_t value = ((_error >> bit) & 1) ? 2 : 1;
        write(value);
    }

    _error_section++;

    if (_error_section == _error_bit_width * 2) {
        _error_section = 0;
    }
}

}  // namespace spirit
