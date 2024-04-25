#ifndef SPONGE_LIBSPONGE_TCP_TIMER
#define SPONGE_LIBSPONGE_TCP_TIMER

#include <cstddef>
#include <cstdint>

class TCP_Timer {
    unsigned int _RTO;
    unsigned int _now{0};
    bool _started{false};

  public:
    TCP_Timer(unsigned int rto, unsigned int now) : _RTO(rto), _now(now) {}

    // 开启计时
    void start() { _started = true; }
    // 从0开始重新计数
    void restart(unsigned int now = 0) {
        _started = true;
        _now = now;
    }

    unsigned int getNow() { return _now; }

    // 关闭计时器
    void close() {
        _started = false;
        _now = 0;
    }

    unsigned int get_RTO() const { return _RTO; }

    void set_RTO(unsigned int new_RTO) { _RTO = new_RTO; }

    /**
     * @brief 判断过了@param 是否超时
     *
     * @param ms_since_last_tick
     * @return true
     * @return false
     */
    bool timeOut(const unsigned int ms_since_last_tick) {
        // 未启动不计时
        if (!_started)
            return false;

        _now += ms_since_last_tick;
        return _now >= _RTO;
    };
};

#endif  // SPONGE_LIBSPONGE_TCP_STATE
