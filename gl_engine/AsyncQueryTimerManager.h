#pragma once

#include<string>
#include<vector>
#include<memory>

class QOpenGLExtraFunctions;

namespace gl_engine {

class AsyncQueryTimer {

public:
    std::string name;   // a custom identifying name for this timer


    AsyncQueryTimer(const std::string& name, int queue_size, QOpenGLExtraFunctions* f);
    ~AsyncQueryTimer();

    // starts front-buffer query
    void start();
    // stops front-buffer query and toggles indices
    void stop();
    // fetches back-buffer query
    void fetch_result();
private:
    std::vector<float> m_measurements;  // Array to save the last measurements
    int m_max_queue_size;               // Maximum size of measurements array
    bool m_full_backbuffer = false;
    unsigned int m_query_id[2];   // gpu_ids of the query objects [0..front, 1..back]

    QOpenGLExtraFunctions* m_f; // OpenGL Context

    void add_measurement(float value);

};

class AsyncQueryTimerManager
{
public:
    AsyncQueryTimerManager();
    std::shared_ptr<AsyncQueryTimer> add_timer(const std::string& name, int queue_size = 30);

    // Starts the timer with m_current_running_timer_ind + 1
    void start_next();

    // Start specific timer defined by position in timer list
    void start_timer(int ind);

    // Stops the currently running timer
    void stop_timer();

    // Fetches the results of all timers
    void fetch_results();
private:
    std::vector<std::shared_ptr<AsyncQueryTimer>> m_timer;
    QOpenGLExtraFunctions* m_f;
    int m_current_running_timer_ind = -1;

};

}

