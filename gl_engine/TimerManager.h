#pragma once

#include<string>
#include<vector>
#include<memory>
#include<map>
#include<chrono>

#include<QMap>
#include <QOpenGLTimerQuery>

class QOpenGLExtraFunctions;

namespace gl_engine {

enum class TimerStates { READY, RUNNING, STOPPED };
enum class TimerTypes { CPU, GPU, GPUAsync };

class GeneralTimer {

public:
    // Starts time-measurement
    void start() {
        //assert(m_state == TimerStates::READY);
        _start();
        m_state = TimerStates::RUNNING;
    }

    // Stops time-measurement
    void stop() {
        //assert(m_state == TimerStates::RUNNING);
        _stop();
        m_state = TimerStates::STOPPED;
    }

    // Fetches the result of the measuring and adds it to the average
    bool fetch_result() {
        if (m_state == TimerStates::STOPPED) {
            float val = _fetch_result();
            add_measurement(val);
            m_state = TimerStates::READY;
            return true;
        }
        return false;
    }

    virtual void _start() = 0;
    virtual void _stop() = 0;
    virtual float _fetch_result() = 0;

    const std::string& get_name()       {   return this->m_name;                    }
    const std::string& get_group()      {   return this->m_group;                   }
    float get_last_measurement()        {   return this->m_last_measurement;        }
    float get_average()                 {   return this->m_avg_measurement;         }
    int get_queue_size()                {   return this->m_queue_size;              }

    GeneralTimer(const std::string& name, const std::string& group, int queue_size)
        :m_name(name), m_group(group), m_queue_size(queue_size)
    {
        m_new_weight = 1.0f / (float)queue_size;
        m_old_weight = 1.0f - m_new_weight;
    }

protected:
    // a custom identifying name for this timer
    std::string m_name;
    std::string m_group;
    int m_queue_size;

private:

    TimerStates m_state = TimerStates::READY;
    float m_new_weight = 1.0/30.0;
    float m_old_weight = 29.0/30.0;
    float m_last_measurement = 0.0f;
    float m_avg_measurement = 0.0f;

    // Adds a measurement by calculating avg and overwriting last measurement
    // value has to be in milliseconds
    void add_measurement(float value) {
        this->m_last_measurement = value;
        this->m_avg_measurement = this->m_avg_measurement * m_old_weight + value * m_new_weight;
    }
};

/// The HostTimer class measures times on the c++ side using the std::chronos library
class HostTimer : public GeneralTimer {
public:
    HostTimer(const std::string& name, const std::string& group, int queue_size);

    // starts front-buffer query
    void _start() override;
    // stops front-buffer query and toggles indices
    void _stop() override;
    // fetches back-buffer query
    float _fetch_result() override;
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_ticks[2];
};

/// The SyncQueryTimer class can only be fetched after cpu-gpu synchronization
/// otherwise the query result might not be yet available
class GpuSyncQueryTimer : public GeneralTimer {
public:
    GpuSyncQueryTimer(const std::string& name, const std::string& group, int queue_size);
    ~GpuSyncQueryTimer();

    // starts front-buffer query
    void _start() override;
    // stops front-buffer query and toggles indices
    void _stop() override;
    // fetches back-buffer query
    float _fetch_result() override;
private:
    QOpenGLTimerQuery* qTmr[2];
};

/// The AsyncQueryTimer class works with a front- and a
/// back-queue to ensure no necessary waiting time for results. Fetching returns the values
/// of the backbuffer (=> the previous frame)
class GpuAsyncQueryTimer : public GeneralTimer {

public:
    GpuAsyncQueryTimer(const std::string& name, const std::string& group, int queue_size);
    ~GpuAsyncQueryTimer();

    // starts front-buffer query
    void _start() override;
    // stops front-buffer query and toggles indices
    void _stop() override;
    // fetches back-buffer query and returns true if new
    float _fetch_result() override;

private:
    // four timers for front and backbuffer (à start-, endtime)
    QOpenGLTimerQuery* m_qTmr[4];

    // true when backbuffer query ready to be fetched
    bool m_full_backbuffer = false;

    int m_current_fb_offset = 0;
    int m_current_bb_offset = 2;

};

struct qTimerReport {
    // value inside timer could be different by now, thats why we send a copy of the value
    float value;
    // Note: Somehow I need the timer definition in the front-end, like name, group, queue_size.
    // I don't want to send all of those values every time, so I'll just send it as shared pointer.
    // Might not be as fast as sending just a pointer, but otherwise it's possible to have a
    // memory issue at deconstruction time of the app.
    std::shared_ptr<GeneralTimer> timer;
};

class TimerManager
{

public:
    TimerManager();

    std::shared_ptr<GeneralTimer> add_timer(const std::string& name, TimerTypes type, const std::string& group = "", int queue_size = 30);

    // Starts the timer with m_current_running_timer_ind + 1
    //void start_next();

    // Start specific timer defined by position in timer list
    void start_timer(const std::string &name);

    // Stops the currently running timer
    void stop_timer(const std::string &name);

    // Fetches the results of all timers and returns the new values
    QList<qTimerReport> fetch_results();


private:
    std::vector<std::shared_ptr<GeneralTimer>> m_timer_in_order;
    std::map<std::string, std::shared_ptr<GeneralTimer>> m_timer;
    QOpenGLExtraFunctions* m_f;

};

}

