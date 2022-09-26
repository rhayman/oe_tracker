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

// preallocate memory for msg
#define BUFFER_MSG_SIZE 256

TTLEventPtr TrackingNodeSettings::createEvent(int sample_number) {
    auto position = tracker->m_messageQueue->pop();
    if (!position)
        return nullptr;
    Array<float> pos;
    pos.add(position->position.x);
    pos.add(position->position.y);
    pos.add(position->position.height);
    pos.add(position->position.width);
    meta_position->setValue(pos);

    meta_name->setValue(m_name);
    meta_address->setValue(m_address);
    meta_port->setValue(m_port);
    meta_color->setValue(m_color);
    
    m_metadata.clear();
    m_metadata.add(meta_name.get());
    m_metadata.add(meta_port.get());
    m_metadata.add(meta_address.get());
    m_metadata.add(meta_color.get());
    m_metadata.add(meta_position.get());

    TTLEventPtr event = TTLEvent::createTTLEvent(eventChannel,
                                                 sample_number,
                                                 0,
                                                 true,
                                                 m_metadata);
    
    return event;
};

std::ostream &
operator<<(std::ostream &stream, const TrackingNodeSettings &module)
{
    stream << "Name: " << module.m_name.toStdString() << std::endl;
    stream << "Address: " << module.m_address.toStdString() << std::endl;
    stream << "Port: " << module.m_port.toStdString() << std::endl;
    stream << "Colour: " << module.m_color.toStdString() << std::endl;
    return stream;
}

TrackingNode::TrackingNode()
    : GenericProcessor("Tracker")
{
    addCategoricalParameter(Parameter::GLOBAL_SCOPE, "Source", "Tracking source", {}, 0);
    addStringParameter(Parameter::GLOBAL_SCOPE, "Port", "Tracking source OSC port", "27020");
    addStringParameter(Parameter::GLOBAL_SCOPE, "Address", "Tracking source OSC address", "/red");
    addCategoricalParameter(Parameter::GLOBAL_SCOPE, "Color", "Tracking source color to be displayed",
                            colors,
                            0);
    m_positionIsUpdated = false;
    m_isRecordingTimeLogged = false;
    m_isAcquisitionTimeLogged = false;
    lastNumInputs = 0;
}

AudioProcessorEditor *TrackingNode::createEditor()
{
    editor = std::make_unique<TrackingNodeEditor>(this);
    return editor.get();
}

int getTrackingModuleIndex(String name, int port, String address)
{
    return 1;
}

const String TrackingNode::getSelectedSourceName()
{
    auto src_param = getParameter("Source");
    CategoricalParameter *cparam = (CategoricalParameter *)src_param;
    return cparam->getSelectedString();
}

String TrackingNode::getParameterValue(Parameter * param) {
    String val;
    if (param->getName().equalsIgnoreCase("color")) {
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        val = cparam->getSelectedString();
    }
    else if (param->getName().equalsIgnoreCase("port")) {
        val = param->getValueAsString();
    }
    else if (param->getName().equalsIgnoreCase("address")) {
        val = param->getValueAsString();
    }
    else if (param->getName().equalsIgnoreCase("source")) {
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        val = cparam->getSelectedString();
    }
    return val;
}

void TrackingNode::addModule(String moduleName)
{
    settings.update(getDataStreams());

    int port_name = DEF_PORT;
    int maxPort = 0;
    if (!getDataStreams().isEmpty()) {
        std::vector<int> ports;
        for (const auto stream : getDataStreams()) {
            TrackingNodeSettings *module = settings[stream->getStreamId()];
            auto p = module->m_port;
            ports.push_back(std::stoi(p.toStdString()));
        }
        maxPort = *std::max_element(ports.begin(), ports.end());
    }
    port_name = maxPort + 1;

    DataStream::Settings streamsettings{"TrackingNode datastream",
                                        "Datastream for Tracking data received from Bonsai",
                                        "external.tracking.rawData",
                                        getDefaultSampleRate()};

    auto stream = new DataStream(streamsettings);
    // Add some metadata to the channel / stream
    MetadataValue name{desc_name};
    name.setValue(moduleName);
    stream->addMetadata(desc_name, name);

    stream->addProcessor(processorInfo.get());

    EventChannel *events;

    EventChannel::Settings s{EventChannel::Type::TTL,
                                "Tracking data",
                                "Tracking data received from Bonsai. x, y, width, height",
                                "external.tracking.rawData",
                                stream};
    events = new EventChannel(s);
    String id = "trackingsource";
    events->setIdentifier(id);
    events->addProcessor(processorInfo.get());

    auto current_color = getParameterValue(getParameter("Color"));
    auto tm = std::make_shared<TrackingModule>(String(port_name), String(DEF_ADDRESS), current_color, this);

    // Add some metadata to the channel / stream
    MetadataValue color{desc_color};
    color.setValue(current_color);
    events->addMetadata(desc_color, color);
    events->addMetadata(desc_name, name);
    
    dataStreams.add(stream);
    eventChannels.add(events);

    settings.update(getDataStreams());

    auto streamId = stream->getStreamId();
    settings[streamId]->eventChannel = events;
    settings[streamId]->tracker = tm;

    String address = getParameterValue(getParameter("Address"));
    String port = getParameterValue(getParameter("Port"));
    String src_name = getParameterValue(getParameter("Source"));
    settings[streamId]->m_name = src_name;
    settings[streamId]->m_address = address;
    settings[streamId]->m_port = port;
    settings[streamId]->m_color = current_color;
    settings.update(getDataStreams());
}

void TrackingNode::parameterValueChanged(Parameter *param)
{
    if (getDataStreams().isEmpty())
        return;

    auto streamId = param->getStreamId();
    
    if (param->getName().equalsIgnoreCase("color"))
    {
        String new_color = getParameterValue(param);
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        String val = cparam->getSelectedString();
        LOGD("GOT HERE");
        LOGD("new color: ", val);
        LOGD("Setttings size = ", settings.size());
        settings[streamId]->m_color = val;
        LOGD("NOW HERE");
    }
    else if (param->getName().equalsIgnoreCase("port"))
    {
        auto new_port = param->getValueAsString();
        settings[streamId]->m_port = new_port;
    }
    else if (param->getName().equalsIgnoreCase("address"))
    {
        auto new_address = param->getValueAsString();
        settings[streamId]->m_address = new_address;
    }
    else if (param->getName().equalsIgnoreCase("source"))
    {
        auto new_name = getParameterValue(param);
        settings[streamId]->m_name = new_name;
    }
}

void TrackingNode::removeModule(const String & moduleToRemove)
{
    for (auto & stream : getDataStreams()) {
        TrackingNodeSettings * module = settings[stream->getStreamId()];
        if (module->m_name == moduleToRemove)
            dataStreams.removeObject(stream);
    }
    settings.update(getDataStreams());
}

void TrackingNode::updateSettings()
{
    dataStreams.clear();
    eventChannels.clear();
    isEnabled = true;
}

bool TrackingNode::startAcquisition(){
    LOGD("StartAcquisition");
    LOGD("m_positionIsUpdated: ", m_positionIsUpdated);
    LOGD("Num datastreams = ", getDataStreams().size());
    return true;
}

bool TrackingNode::stopAcquisition(){
    LOGD("StopAcquisition");
    LOGD("m_positionIsUpdated: ", m_positionIsUpdated);
    LOGD("Num datastreams = ", getDataStreams().size());
    return true;
}

void TrackingNode::process(AudioBuffer<float> &buffer)
{
    LOGD("[Tracking Node] ", "in process");
    if (!m_positionIsUpdated)
        return;
    lock.enter();
    LOGD("Num datastreams = ", getDataStreams().size());
    for (auto stream : getDataStreams())
    {
        auto streamId = stream->getStreamId();
        TrackingNodeSettings* module = settings[streamId];
        auto sample_number = getFirstSampleNumberForBlock(streamId);
        settings[streamId]->createEvent(sample_number);
        std::cout << "sample_number: " << sample_number << std::endl;
    }
    lock.exit();
    m_positionIsUpdated = false;
}

void TrackingNode::handleTTLEvent(TTLEventPtr event)
{
    std::cout << "HANDLETTLEVENT" << std::endl;
}

// TODO: CLEAN THIS

int TrackingNode::getNSources()
{
    return settings.size();
}

void TrackingNode::receiveMessage(int port, String address, const TrackingData &message)
{
    for (auto stream : getDataStreams())
    {
        auto * module = settings[stream->getStreamId()];
        lock.enter();
        if (CoreServices::getRecordingStatus())
        {
            if (!m_isRecordingTimeLogged)
            {
                m_received_msg = 0;
                m_startingRecTimeMillis = Time::currentTimeMillis();
                m_isRecordingTimeLogged = true;
                std::cout << "Starting Recording Ts: " << m_startingRecTimeMillis << std::endl;
                module->tracker->m_messageQueue->clear();
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
                module->tracker->m_messageQueue->clear();
                CoreServices::sendStatusMessage("Clearing queue before start acquisition");
            }

            m_positionIsUpdated = true;

            int64 ts = CoreServices::getSoftwareTimestamp();

            TrackingData outputMessage = message;
            outputMessage.timestamp = ts;
            module->tracker->m_messageQueue->push(outputMessage);
            m_received_msg++;
        }
        else
            m_isAcquisitionTimeLogged = false;
        lock.exit();
    }
}

// TODO: Both I/O methods need finishing
void TrackingNode::saveCustomParametersToXml(XmlElement *parentElement)
{
    for (auto stream : getDataStreams())
    {
        auto * moduleXml = parentElement->createNewChildElement("Tracking_Node");
        TrackingNodeSettings *module = settings[stream->getStreamId()];
        String val;
        module->meta_name->getValue(val);
        moduleXml->setAttribute("Name", val);
        module->meta_port->getValue(val);
        moduleXml->setAttribute("Port", val);
        module->meta_address->getValue(val);
        moduleXml->setAttribute("Address", val);
        module->meta_color->getValue(val);
        moduleXml->setAttribute("Color", val);
    }
}

void TrackingNode::loadCustomParametersFromXml(XmlElement *xml)
{
    auto availableStreams = getDataStreams();
    for (auto * moduleXml : xml->getChildIterator()) {
        if (moduleXml->hasTagName("Tracking_Node")) {
            String name = moduleXml->getStringAttribute("Name");
            String address = moduleXml->getStringAttribute("Address");
            String port = moduleXml->getStringAttribute("Port");
            String color = moduleXml->getStringAttribute("Color");
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
	waitForThreadToExit(2000);
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
            std::cout << "ERROR: TrackingServer received message with wrong number of arguments. "
                 << "Expected " << argumentCount << ", got " << receivedMessage.ArgumentCount() << std::endl;
            return;
        }

        for (uint32 i = 0; i < receivedMessage.ArgumentCount(); i++)
        {
            if (receivedMessage.TypeTags()[i] != 'f')
            {
                std::cout << "TrackingServer only support 'f' (floats), not '"
                     << receivedMessage.TypeTags()[i] << "'" << std::endl;
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
        DBG("error while parsing message: " << receivedMessage.AddressPattern() << ": " << e.what() << "\n");
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
    std::cout << "Sleeping!" << std::endl;
    sleep(1000);
    std::cout << "Running!" << std::endl;
    // Start the oscpack OSC Listener Thread
    try
    {
        m_listeningSocket = new UdpListeningReceiveSocket(IpEndpointName("localhost", std::stoi(m_incomingPort.toStdString())), this);
        sleep(1000);
        m_listeningSocket->Run();
    }
    catch (const std::exception &e)
    {
        std::cout << "Exception in TrackingServer::run(): " << e.what() << std::endl;
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
