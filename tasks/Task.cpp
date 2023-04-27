/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "Task.hpp"
#include <base-logging/Logging.hpp>

using namespace webrtc_rustysignal;
using namespace std;

Task::Task(std::string const& name)
    : TaskBase(name)
{
    Json::CharReaderBuilder builder;
    m_json_reader = builder.newCharReader();

    _connection_timeout.set(base::Time::fromSeconds(1));
    _close_timeout.set(base::Time::fromSeconds(1));
}

Task::~Task()
{
    delete m_json_reader;
}

bool Task::configureHook()
{
    if (!TaskBase::configureHook())
        return false;

    promise<void> promise;
    auto future = promise.get_future();
    m_ws.onOpen([&promise]() {
        LOG_DEBUG_S << "websocket opened";
        promise.set_value();
    });
    m_ws.onError([&promise](string const& error) {
        LOG_DEBUG_S << "websocket error" << error;
        promise.set_exception(make_exception_ptr(runtime_error(error)));
    });

    m_ws.open(_url.get() + "?user=" + _peer_id.get());
    if (future.wait_for(chrono::microseconds(
            _connection_timeout.get().toMicroseconds())) != future_status::ready) {
        throw std::runtime_error(
            "timed out waiting for the websocket connection to open");
    }
    future.get();

    m_error = "";
    m_ws.onError([&](string const& error) { m_error = error; });
    m_ws.onClosed([&]() { this->trigger(); });
    m_ws.onMessage([&](std::variant<rtc::binary, string> const& data) {
        if (!isRunning()) {
            return;
        }

        if (!holds_alternative<string>(data)) {
            m_error = "received binary message, expected string";
            return;
        }

        auto msg = get<string>(data);
        char const* begin = msg.data();
        char const* end = begin + msg.size();

        Json::Value json;
        if (!m_json_reader->parse(begin, end, &json, &m_error)) {
            return;
        }

        signallingOut(json);
    });
    return true;
}
bool Task::startHook()
{
    if (!TaskBase::startHook())
        return false;

    if (m_ws.isClosed()) {
        throw std::runtime_error("websocket closed between configure and start");
    }
    else if (!m_error.empty()) {
        throw std::runtime_error(
            "websocket errored between configure and start: " + m_error);
    }

    return true;
}
void Task::updateHook()
{
    TaskBase::updateHook();

    if (m_ws.isClosed()) {
        throw std::runtime_error("websocket is closed");
    }

    webrtc_base::SignallingMessage msg;
    while (_signalling_in.read(msg) == RTT::NewData) {
        signallingIn(msg);
    }
}
void Task::errorHook()
{
    TaskBase::errorHook();
}
void Task::stopHook()
{
    TaskBase::stopHook();
}
void Task::cleanupHook()
{
    TaskBase::cleanupHook();

    promise<void> promise;
    auto future = promise.get_future();

    m_ws.onClosed([&promise]() { promise.set_value(); });
    m_ws.onError([&promise](string const& error) {
        promise.set_exception(make_exception_ptr(runtime_error(error)));
    });

    m_ws.close();
    future.wait_for(chrono::microseconds(_close_timeout.get().toMicroseconds()));
}

static webrtc_base::SignallingMessageType jsonActionToSignallingType(
    std::string const& action)
{
    if (action == "peer-disconnect") {
        return webrtc_base::SIGNALLING_PEER_DISCONNECT;
    }
    else if (action == "peer-disconnected") {
        return webrtc_base::SIGNALLING_PEER_DISCONNECTED;
    }
    else if (action == "request-offer") {
        return webrtc_base::SIGNALLING_REQUEST_OFFER;
    }
    else if (action == "offer") {
        return webrtc_base::SIGNALLING_OFFER;
    }
    else if (action == "answer") {
        return webrtc_base::SIGNALLING_ANSWER;
    }
    else if (action == "candidate") {
        return webrtc_base::SIGNALLING_ICE_CANDIDATE;
    }
    else {
        throw std::invalid_argument("invalid signalling action '" + action + "'");
    }
}

static std::string signallingTypeToJsonAction(webrtc_base::SignallingMessageType type)
{
    switch (type) {
        case webrtc_base::SIGNALLING_PEER_DISCONNECT:
            return "peer-disconnect";
        case webrtc_base::SIGNALLING_PEER_DISCONNECTED:
            return "peer-disconnected";
        case webrtc_base::SIGNALLING_REQUEST_OFFER:
            return "request-offer";
        case webrtc_base::SIGNALLING_OFFER:
            return "offer";
        case webrtc_base::SIGNALLING_ANSWER:
            return "answer";
        case webrtc_base::SIGNALLING_ICE_CANDIDATE:
            return "candidate";
        default:
            throw std::invalid_argument("invalid signalling message type");
    }
}

void Task::signallingOut(Json::Value const& json)
{
    webrtc_base::SignallingMessage msg;
    msg.from = json["data"]["from"].asString();
    if (json["protocol"] == "one-to-one") {
        msg.to = json["to"].asString();
    }
    msg.type = jsonActionToSignallingType(json["action"].asString());
    if (msg.type == webrtc_base::SIGNALLING_OFFER ||
        msg.type == webrtc_base::SIGNALLING_ANSWER) {
        msg.message = json["data"]["description"].asString();
    }
    else if (msg.type == webrtc_base::SIGNALLING_ICE_CANDIDATE) {
        msg.message = json["data"]["candidate"].asString();
        auto mid = json["data"]["mid"];
        if (mid.isString()) {
            msg.m_line = std::stoi(mid.asString());
        }
        else {
            msg.m_line = mid.asInt();
        }
    }
    else {
        msg.message = json["data"]["message"].asString();
    }

    _signalling_out.write(msg);
}

void Task::signallingIn(webrtc_base::SignallingMessage const& msg)
{
    Json::Value json;
    json["data"]["from"] = msg.from;
    json["to"] = msg.to;
    if (msg.to.empty()) {
        json["protocol"] = "one-to-all";
    }
    else {
        json["protocol"] = "one-to-one";
    }
    json["action"] = signallingTypeToJsonAction(msg.type);
    if (msg.type == webrtc_base::SIGNALLING_OFFER ||
        msg.type == webrtc_base::SIGNALLING_ANSWER) {
        json["data"]["description"] = msg.message;
    }
    else if (msg.type == webrtc_base::SIGNALLING_ICE_CANDIDATE) {
        json["data"]["candidate"] = msg.message;
        json["data"]["mid"] = msg.m_line;
    }
    else {
        json["data"]["message"] = msg.message;
    }

    Json::FastWriter fast;
    m_ws.send(fast.write(json));
}
