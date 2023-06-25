#include "TimerManager.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLContext>
#include <QDebug>



namespace gl_engine {

HostTimer::HostTimer(const std::string &name, int queue_size)
    :GeneralTimer(name, queue_size)
{
}

void HostTimer::start() {
    m_ticks[0] = std::chrono::high_resolution_clock::now();
}

void HostTimer::stop() {
    m_ticks[1] = std::chrono::high_resolution_clock::now();
}

void HostTimer::fetch_result() {
    std::chrono::duration<double> diff = m_ticks[1] - m_ticks[0];
    add_measurement((float)(diff.count() * 1000.0d));
}

GpuSyncQueryTimer::GpuSyncQueryTimer(const std::string &name, int queue_size)
    :GeneralTimer(name, queue_size)
{
    qTmr[0] = new QOpenGLTimerQuery(QOpenGLContext::currentContext());
    qTmr[1] = new QOpenGLTimerQuery(QOpenGLContext::currentContext());
    qTmr[0]->create();
    qTmr[1]->create();
}

GpuSyncQueryTimer::~GpuSyncQueryTimer() {
    delete qTmr[0];
    delete qTmr[1];
}

void GpuSyncQueryTimer::start() {
    qTmr[0]->recordTimestamp();
}

void GpuSyncQueryTimer::stop() {
    qTmr[1]->recordTimestamp();
}

void GpuSyncQueryTimer::fetch_result() {
#ifdef QT_DEBUG
    if (!qTmr[0]->isResultAvailable() || !qTmr[1]->isResultAvailable()) {
        qWarning() << "A timer result is not available yet for timer" << m_name << ". The Thread will be blocked.";
    }
#endif
    GLuint64 elapsed_time = qTmr[1]->waitForResult() - qTmr[0]->waitForResult();
    add_measurement(elapsed_time / 1000000.0);
}



GpuAsyncQueryTimer::GpuAsyncQueryTimer(const std::string& name, int queue_size, QOpenGLExtraFunctions* f)
    :GeneralTimer(name, queue_size), m_f(f)
{
    f->glGenQueries(2, m_query_id);
}

GpuAsyncQueryTimer::~GpuAsyncQueryTimer() {
    m_f->glDeleteQueries(2, m_query_id);
}

void GpuAsyncQueryTimer::start() {
    m_f->glBeginQuery(GL_TIME_ELAPSED, m_query_id[0]);
}

void GpuAsyncQueryTimer::stop() {
    m_f->glEndQuery(GL_TIME_ELAPSED);
    if (m_full_backbuffer)
        qWarning() << "Unread Backbuffer of Timer " << m_name << "gets overwritten.";
    std::swap(m_query_id[0], m_query_id[1]);
    m_full_backbuffer = true;
}

void GpuAsyncQueryTimer::fetch_result() {
    if (!m_full_backbuffer) return;
    GLuint elapsed_time;
    m_f->glGetQueryObjectuiv(m_query_id[1], GL_QUERY_RESULT, &elapsed_time);
    if (!elapsed_time)
        qWarning() << "Return value for Timer " << m_name << " was 0. Maybe not available yet?";
    add_measurement(elapsed_time / 1000000.0);
    m_full_backbuffer = false;
}

TimerManager::TimerManager()
{
    this->m_f = QOpenGLContext::currentContext()->extraFunctions();
}

void TimerManager::start_timer(const std::string &name)
{
    m_timer[name]->start();
}

void TimerManager::stop_timer(const std::string &name)
{
    m_timer[name]->stop();
}

void TimerManager::fetch_results()
{
    for (const auto& kv : m_timer) {
        kv.second->fetch_result();
    }
}

std::shared_ptr<GeneralTimer> TimerManager::add_timer(const std::string &name, TimerTypes type, int queue_size) {
    std::shared_ptr<GeneralTimer> tmr_base;
    switch(type) {

    case TimerTypes::CPU:
        tmr_base = static_pointer_cast<GeneralTimer>(std::make_shared<HostTimer>(name, queue_size));
        break;

    case TimerTypes::GPU:
        tmr_base = static_pointer_cast<GeneralTimer>(std::make_shared<GpuSyncQueryTimer>(name, queue_size));
        break;

    case TimerTypes::GPUAsync:
        tmr_base = static_pointer_cast<GeneralTimer>(std::make_shared<GpuAsyncQueryTimer>(name, queue_size, m_f));
        break;
    }

    m_timer[name] = tmr_base;
    return tmr_base;
}

} // namespace

