#include "AsyncQueryTimerManager.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLContext>
#include <QDebug>

namespace gl_engine {

AsyncQueryTimer::AsyncQueryTimer(const std::string& name, int queue_size, QOpenGLExtraFunctions* f) :name(name), m_max_queue_size(queue_size), m_f(f)
{
    f->glGenQueries(2, m_query_id);
}

AsyncQueryTimer::~AsyncQueryTimer() {
    m_f->glDeleteQueries(2, m_query_id);
}

void AsyncQueryTimer::start() {
    m_f->glBeginQuery(GL_TIME_ELAPSED, m_query_id[0]);

}

void AsyncQueryTimer::stop() {
    m_f->glEndQuery(GL_TIME_ELAPSED);
    if (m_full_backbuffer)
        qWarning() << "Unread Backbuffer of Timer " << name << "gets overwritten.";
    std::swap(m_query_id[0], m_query_id[1]);
    m_full_backbuffer = true;
}

void AsyncQueryTimer::fetch_result() {
    if (!m_full_backbuffer) return;
    GLuint elapsed_time;
    m_f->glGetQueryObjectuiv(m_query_id[1], GL_QUERY_RESULT, &elapsed_time);
    if (!elapsed_time)
        qWarning() << "Return value for Timer " << name << " was 0. Maybe not available yet?";
    add_measurement(elapsed_time / 1000000.0);
    m_full_backbuffer = false;
}

void AsyncQueryTimer::add_measurement(float value) {
    m_measurements.push_back(value);
    if (m_measurements.size() > m_max_queue_size)
        m_measurements.erase(m_measurements.begin()); // drop oldest measurement)
    qDebug() << "Timer " << name << " added measurement " << value << " count(" << m_measurements.size() << ")";
}

AsyncQueryTimerManager::AsyncQueryTimerManager()
{
    this->m_f = QOpenGLContext::currentContext()->extraFunctions();
}

void AsyncQueryTimerManager::start_next()
{
    start_timer(m_current_running_timer_ind + 1);
}

void AsyncQueryTimerManager::start_timer(int ind)
{
    if (m_current_running_timer_ind > 0)
        this->stop_timer();
    m_timer[ind]->start();
    m_current_running_timer_ind = ind;
}

void AsyncQueryTimerManager::stop_timer()
{
    if (m_current_running_timer_ind < 0) return;
    m_timer[m_current_running_timer_ind]->stop();
    m_current_running_timer_ind = -1;
}

void AsyncQueryTimerManager::fetch_results()
{
    for (int i = 0; i < m_timer.size(); i++) {
        m_timer[i]->fetch_result();
    }
}

std::shared_ptr<AsyncQueryTimer> AsyncQueryTimerManager::add_timer(const std::string &name, int queue_size) {
    auto tmr = std::make_shared<AsyncQueryTimer>(name, queue_size, m_f);
    m_timer.push_back(tmr);
    return tmr;
}

} // namespace

