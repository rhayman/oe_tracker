/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2022 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "TrackingNode.h"
#include "TrackingNodeEditor.h"
#include "TrackingMessage.h"
#include "../../../plugin-GUI/Source/Utils/Utils.h"


// preallocate memory for msg
#define BUFFER_MSG_SIZE 256

TTLEventPtr TrackingNodeSettings::createEvent(int64 sample_number, TrackingModule * tracker)
{
    auto position = tracker->m_messageQueue->pop();
    if (!position)
        return nullptr;
    Array<float> pos;
    pos.add(position->position.x);
    pos.add(position->position.y);
    pos.add(position->position.height);
    pos.add(position->position.width);
    
    MetadataValuePtr p_pos = new MetadataValue(*desc_position);
    MetadataValuePtr p_port = new MetadataValue(*desc_port);
    MetadataValuePtr p_addr = new MetadataValue(*desc_address);
    p_pos->setValue(pos);
    p_port->setValue(tracker->m_port);
    p_addr->setValue(tracker->m_address);
    MetadataValueArray metadata;
    metadata.add(p_pos);
    metadata.add(p_port);
    metadata.add(p_addr);
    TTLEventPtr event = TTLEvent::createTTLEvent(tracker->eventChannel,
                                                 sample_number,
                                                 0,
                                                 true,
                                                 metadata);
    return event;
};

std::ostream &
operator<<(std::ostream &stream, const TrackingModule &module)
{
    stream << "Name: " << module.m_name << std::endl;
    stream << "Address: " << module.m_address << std::endl;
    stream << "Port: " << module.m_port << std::endl;
    return stream;
}

TrackingNode::TrackingNode()
    : GenericProcessor("OE Tracker")
{
    addCategoricalParameter(Parameter::GLOBAL_SCOPE, "Name", "Tracking source", {}, 0);
    addStringParameter(Parameter::GLOBAL_SCOPE, "Port", "Tracking source OSC port", "27020");
    addStringParameter(Parameter::GLOBAL_SCOPE, "Address", "Tracking source OSC address", "/red");
    m_positionIsUpdated = false;
    m_isRecordingTimeLogged = false;
    m_isAcquisitionTimeLogged = false;
}

AudioProcessorEditor *TrackingNode::createEditor()
{
    editor = std::make_unique<TrackingNodeEditor>(this);
    return editor.get();
}

String TrackingNode::getParameterValue(Parameter *param)
{
    String val;
    if (param->getName().equalsIgnoreCase("port"))
    {
        val = param->getValueAsString();
    }
    else if (param->getName().equalsIgnoreCase("address"))
    {
        val = param->getValueAsString();
    }
    else if (param->getName().equalsIgnoreCase("name"))
    {
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        val = cparam->getSelectedString();
    }
    return val;
}

void TrackingNode::initialize() {
    if ( getDataStreams().isEmpty() ) {
        DataStream::Settings streamsettings{"TrackingNode datastream",
                                            "Datastream for Tracking data received from Bonsai",
                                            "external.tracking.rawData",
                                            getDefaultSampleRate()};

        auto stream = new DataStream(streamsettings);
        dataStreams.add(stream);
        dataStreams.getLast()->addProcessor(processorInfo.get());
        settings.update(getDataStreams());
    }
}

void TrackingNode::addTracker(String moduleName, String port, String address, String color)
{
    settings.update(getDataStreams());

    for (auto stream : getDataStreams()) {
        if (stream->getName().equalsIgnoreCase("TrackingNode datastream")) {
            if (port.isEmpty())
                port = String(DEF_PORT);
            auto trackers = settings[stream->getStreamId()]->trackers;
            if (!trackers.isEmpty())
            {
                std::vector<int> ports;
                for (const auto tracker : trackers)
                {
                    auto p = tracker->m_port;
                    ports.push_back(std::stoi(p.toStdString()));
                }
                int maxPort = *std::max_element(ports.begin(), ports.end());
                port = String(maxPort + 1);
            }
            String port_name = String(port);
            if (color.isEmpty())
                color = String(DEF_COLOR);
            if (address.isEmpty())
                address = String(DEF_ADDRESS);
            auto tm = new TrackingModule(port_name, address, color, this);
            tm->m_name = moduleName;

            EventChannel *events;
            EventChannel::Settings s{EventChannel::Type::TTL,
                                    "Tracking data",
                                    "Tracking data received from Bonsai. x, y, width, height",
                                    "external.tracking.rawData",
                                    getDataStream(stream->getStreamId())};
            events = new EventChannel(s);
            String id = "trackingsource";
            events->setIdentifier(id);
            events->addProcessor(processorInfo.get());
            // add metadata
            meta_name = new MetadataValue(*desc_name);
            meta_name->setValue(moduleName);
            
            meta_port = new MetadataValue(*desc_port);
            meta_port->setValue(port_name);
            meta_address = new MetadataValue(*desc_address);
            meta_address->setValue(address);

            // add some dummy pos data for now
            Array<float> pos;
            pos.add(-1);
            pos.add(-1);
            pos.add(-1);
            pos.add(-1);
            meta_position = new MetadataValue(*desc_position);
            meta_position->setValue(pos);
            events->addMetadata(desc_position.get(), meta_position);
            events->addMetadata(desc_name.get(), meta_name);
            events->addMetadata(desc_address.get(), meta_address);
            events->addMetadata(desc_port.get(), meta_port);
            events->addEventMetadata(*desc_position);
            events->addEventMetadata(*desc_port);
            events->addEventMetadata(*desc_address);
            eventChannels.add(events);
            tm->eventChannel = events;
            settings[stream->getStreamId()]->trackers.add(tm);
        }

    }
    settings.update(getDataStreams());
}

void TrackingNode::removeTracker(const String &moduleToRemove)
{
    for (auto &stream : getDataStreams())
    {
        if (stream->getName().equalsIgnoreCase("TrackingNode datastream")) {
            auto trackers = settings[stream->getStreamId()]->trackers;
            for (const auto tracker : trackers) {
                if (tracker->m_name == moduleToRemove)
                    trackers.remove(&tracker);
            }
        }
    }
    settings.update(getDataStreams());
}

void TrackingNode::parameterValueChanged(Parameter *param)
{
    if (getDataStreams().isEmpty())
        return;
    auto src_name = getParameterValue(getParameter("Name"));
    for (auto stream : getDataStreams()) {
        if (stream->getName().equalsIgnoreCase("TrackingNode datastream")) {
            auto trackers = settings[stream->getStreamId()]->trackers;
            for (auto tracker : trackers) {
                if (tracker->m_name == src_name) {
                    if (param->getName().equalsIgnoreCase("port"))
                    {
                        auto new_port = getParameterValue(param);
                        tracker->m_port = new_port;
                    }
                    else if (param->getName().equalsIgnoreCase("address"))
                    {
                        auto new_address = getParameterValue(param);
                        tracker->m_address = new_address;
                    }
                    else if (param->getName().equalsIgnoreCase("name"))
                    {
                        auto new_name = getParameterValue(param);
                        tracker->m_name = new_name;
                        auto *port = getParameter("Port");
                        auto *address = getParameter("Address");
                        port->currentValue = tracker->m_port;
                        address->currentValue = tracker->m_address;
                    }
                }
            }
        }
    }
}

void TrackingNode::updateSettings()
{
    dataStreams.clear();
    eventChannels.clear();
    initialize();
    isEnabled = true;
}

void TrackingNode::process(AudioBuffer<float> &buffer)
{
    if (!m_positionIsUpdated)
        return;
    lock.enter();
    for (auto stream : getDataStreams())
    {
        if (stream->getName().equalsIgnoreCase("TrackingNode datastream")) {
            auto streamId = stream->getStreamId();
            auto trackers = settings[streamId]->trackers;
            for (auto tracker : trackers) {
                TTLEventPtr event = settings[streamId]->createEvent(0, tracker);
                addEvent(event, 0);
            }
        }
    }
    lock.exit();
    m_positionIsUpdated = false;
}

void TrackingNode::receiveMessage(int port, String address, const TrackingData &message)
{
    for (auto stream : getDataStreams())
    {
        if ( stream->getName().equalsIgnoreCase("TrackingNode datastream")) {
            lock.enter();
            auto trackers = settings[stream->getStreamId()]->trackers;
            for (auto tracker : trackers) {
                if (CoreServices::getRecordingStatus())
                {
                    if (!m_isRecordingTimeLogged)
                    {
                        m_received_msg = 0;
                        m_startingRecTimeMillis = Time::currentTimeMillis();
                        m_isRecordingTimeLogged = true;
                        std::cout << "Starting Recording Ts: " << m_startingRecTimeMillis << std::endl;
                        tracker->m_messageQueue->clear();
                        CoreServices::sendStatusMessage("Clearing queue before start recording");
                    }
                }
                else
                {
                    m_isRecordingTimeLogged = false;
                }
                if (CoreServices::getAcquisitionStatus()) // && !CoreServices::getRecordingStatus())
                {
                    if (!m_isAcquisitionTimeLogged)
                    {
                        m_startingAcqTimeMillis = Time::currentTimeMillis();
                        m_isAcquisitionTimeLogged = true;
                        std::cout << "Starting Acquisition at Ts: " << m_startingAcqTimeMillis << std::endl;
                        tracker->m_messageQueue->clear();
                        CoreServices::sendStatusMessage("Clearing queue before start acquisition");
                    }

                    m_positionIsUpdated = true;

                    int64 ts = CoreServices::getSoftwareTimestamp();

                    TrackingData outputMessage = message;
                    outputMessage.timestamp = ts;
                    tracker->m_messageQueue->push(outputMessage);
                    m_received_msg++;
                }
                else
                    m_isAcquisitionTimeLogged = false;
            }
            lock.exit();
        }
    }
}

// TODO: Both I/O methods need finishing
void TrackingNode::saveCustomParametersToXml(XmlElement *parentElement)
{
    for (auto stream : getDataStreams())
    {
        auto *moduleXml = parentElement->createNewChildElement("Tracking_Node");
        TrackingNodeSettings *module = settings[stream->getStreamId()];
        String val;
        for (auto tracker : module->trackers) {
            moduleXml->setAttribute("Name", tracker->m_name);
            moduleXml->setAttribute("Port", tracker->m_port);
            moduleXml->setAttribute("Address", tracker->m_address);
        }
    }
}

void TrackingNode::loadCustomParametersFromXml(XmlElement *xml)
{
    for (auto *moduleXml : xml->getChildIterator())
    {
        if (moduleXml->hasTagName("Tracking_Node"))
        {
            String name = moduleXml->getStringAttribute("Name");
            String address = moduleXml->getStringAttribute("Address");
            String port = moduleXml->getStringAttribute("Port");

            addTracker(name, port, address);
        }
    }
}

// Class TrackingQueue methods
TrackingQueue::TrackingQueue()
    : m_head(-1), m_tail(-1)
{
    memset(m_buffer, 0, BUFFER_SIZE);
}

TrackingQueue::~TrackingQueue() {}

void TrackingQueue::push(const TrackingData &message)
{
    m_head = (m_head + 1) % BUFFER_SIZE;
    m_buffer[m_head] = message;
}

TrackingData *TrackingQueue::pop()
{
    if (isEmpty())
        return nullptr;

    m_tail = (m_tail + 1) % BUFFER_SIZE;
    return &(m_buffer[m_tail]);
}

bool TrackingQueue::isEmpty()
{
    return m_head == m_tail;
}

void TrackingQueue::clear()
{
    m_tail = -1;
    m_head = -1;
}

// Class TrackingServer methods
TrackingServer::TrackingServer()
    : Thread("OscListener Thread"), m_incomingPort(0), m_address("")
{
}

TrackingServer::TrackingServer(String port, String address)
    : Thread("OscListener Thread"), m_incomingPort(port), m_address(address)
{
}

TrackingServer::~TrackingServer()
{
    // stop the OSC Listener thread running
    stop();
    stopThread(1000);
    waitForThreadToExit(1000);
    delete m_listeningSocket;
}

void TrackingServer::ProcessMessage(const osc::ReceivedMessage &receivedMessage,
                                    const IpEndpointName &)
{
    int64 ts = CoreServices::getGlobalTimestamp();
    try
    {
        uint32 argumentCount = 4;

        if (receivedMessage.ArgumentCount() != argumentCount)
        {
            LOGD("ERROR: TrackingServer received message with wrong number of arguments. ",
                "Expected ", argumentCount, ", got ", receivedMessage.ArgumentCount());
            return;
        }

        for (uint32 i = 0; i < receivedMessage.ArgumentCount(); i++)
        {
            if (receivedMessage.TypeTags()[i] != 'f')
            {
                LOGD("TrackingServer only support 'f' (floats), not '", String(receivedMessage.TypeTags()[i]));
                return;
            }
        }

        osc::ReceivedMessageArgumentStream args = receivedMessage.ArgumentStream();

        TrackingData trackingData;

        // Arguments:
        args >> trackingData.position.x;      // 0 - x
        args >> trackingData.position.y;      // 1 - y
        args >> trackingData.position.width;  // 2 - box width
        args >> trackingData.position.height; // 3 - box height
        args >> osc::EndMessage;

        for (TrackingNode *processor : m_processors)
        {
            if (std::strcmp(receivedMessage.AddressPattern(), m_address.toStdString().c_str()) != 0)
            {
                continue;
            }
            // add trackingmodule to receive message call: processor->receiveMessage (m_incomingPort, m_address, trackingData);
            processor->receiveMessage(std::stoi(m_incomingPort.toStdString()), m_address, trackingData);
        }
    }
    catch (osc::Exception &e)
    {
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        LOGD("error while parsing message: ", String(receivedMessage.AddressPattern()), ": ", String(e.what()));
    }
}

void TrackingServer::addProcessor(TrackingNode *processor)
{
    m_processors.push_back(processor);
}

void TrackingServer::removeProcessor(TrackingNode *processor)
{
    m_processors.erase(std::remove(m_processors.begin(), m_processors.end(), processor), m_processors.end());
}

void TrackingServer::run()
{
    CoreServices::sendStatusMessage("Server sleeping!");
    sleep(1000);
    CoreServices::sendStatusMessage("Server running");
    // Start the oscpack OSC Listener Thread
    try
    {
        m_listeningSocket = new UdpListeningReceiveSocket(IpEndpointName("localhost", std::stoi(m_incomingPort.toStdString())), this);
        sleep(1000);
        m_listeningSocket->Run();
    }
    catch (const std::exception &e)
    {
        LOGD("Exception in TrackingServer::run(): ", String(e.what()));
    }
}

void TrackingServer::stop()
{
    // Stop the oscpack OSC Listener Thread
    if (!isThreadRunning())
    {
        return;
    }

    m_listeningSocket->AsynchronousBreak();
}
