#pragma once

#include<string>
#include<vector>
#include<memory>
#include<QDebug>
#include<map>
#include <chrono>

#include <QOpenGLTimerQuery>

class QOpenGLExtraFunctions;

namespace gl_engine {

class GeneralTimer {

public:


    // Starts time-measurement
    virtual void start() = 0;
    // Stops time-measurement
    virtual void stop() = 0;
    // Fetches the result of the measuring and adds it to the average
    virtual void fetch_result() = 0;

    const std::string& get_name() {
        return this->m_name;
    }

    GeneralTimer(const std::string& name, int queue_size)
        :m_name(name)
    {
        m_new_weight = 1.0f / (float)queue_size;
        m_old_weight = 1.0f - m_new_weight;
    }


protected:
    // a custom identifying name for this timer
    std::string m_name;
    // Adds a measurement by calculating avg and overwriting last measurement
    // value has to be in milliseconds
    void add_measurement(float value) {
        this->m_last_measurement = value;
        if (this->m_avg_measurement < 0.0) {
            // It's the first measurement
            this->m_avg_measurement = value;
        } else {
            this->m_avg_measurement = this->m_avg_measurement * m_old_weight + value * m_new_weight;
        }
        //qDebug() << "Timer " << m_name << " added measurement " << value << " avg(" << m_avg_measurement << ")";
    }

private:
    float m_new_weight = 1.0/30.0;
    float m_old_weight = 29.0/30.0;
    float m_last_measurement = -1.0f;
    float m_avg_measurement = -1.0f;
};

/// The HostTimer class measures times on the c++ side using the std::chronos library
class HostTimer : public GeneralTimer {
public:
    HostTimer(const std::string& name, int queue_size);

    // starts front-buffer query
    void start() override;
    // stops front-buffer query and toggles indices
    void stop() override;
    // fetches back-buffer query
    void fetch_result() override;
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_ticks[2];
};

/// The SyncQueryTimer class can only be fetched after cpu-gpu synchronization
/// otherwise the query result might not be yet available
class GpuSyncQueryTimer : public GeneralTimer {
public:
    GpuSyncQueryTimer(const std::string& name, int queue_size);
    ~GpuSyncQueryTimer();

    // starts front-buffer query
    void start() override;
    // stops front-buffer query and toggles indices
    void stop() override;
    // fetches back-buffer query
    void fetch_result() override;
private:
    QOpenGLTimerQuery* qTmr[2];
};

/// The AsyncQueryTimer class works with a front- and a
/// back-queue to ensure no necessary waiting time for results. Fetching returns the values
/// of the backbuffer (=> the previous frame)
/// WARNING: No stacking of timers possible, since we use GL_TIME_ELAPSED
class GpuAsyncQueryTimer : public GeneralTimer {

public:
    GpuAsyncQueryTimer(const std::string& name, int queue_size, QOpenGLExtraFunctions* f);
    ~GpuAsyncQueryTimer();

    // starts front-buffer query
    void start() override;
    // stops front-buffer query and toggles indices
    void stop() override;
    // fetches back-buffer query
    void fetch_result() override;
private:
    // true when backbuffer query ready to be fetched
    bool m_full_backbuffer = false;
    // gpu_ids of the query objects [0..front, 1..back]
    unsigned int m_query_id[2];

    QOpenGLExtraFunctions* m_f; // OpenGL Context

};

enum class TimerTypes {
    CPU,
    GPU,
    GPUAsync
};

class TimerManager
{
public:
    TimerManager();

    std::shared_ptr<GeneralTimer> add_timer(const std::string& name, TimerTypes type, int queue_size = 30);

    // Starts the timer with m_current_running_timer_ind + 1
    //void start_next();

    // Start specific timer defined by position in timer list
    void start_timer(const std::string &name);

    // Stops the currently running timer
    void stop_timer(const std::string &name);

    // Fetches the results of all timers
    void fetch_results();
private:
    std::map<std::string, std::shared_ptr<GeneralTimer>> m_timer;
    QOpenGLExtraFunctions* m_f;

};

}

