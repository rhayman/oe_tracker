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

using namespace std;

void TrackingNodeSettings::updateModule(String name, Parameter *param)
{
}

std::ostream &
operator<<(std::ostream &stream, const TrackingModule &module)
{
    stream << "Name: " << module.m_name.toStdString() << std::endl;
    stream << "Address: " << module.m_address.toStdString() << std::endl;
    stream << "Port: " << module.m_port.toStdString() << std::endl;
    stream << "Colour: " << module.m_color.toStdString() << std::endl;
    return stream;
}

void TrackingModule::createMetaValues()
{
    MetadataDescriptor desc_name = MetadataDescriptor(
        MetadataDescriptor::MetadataType::CHAR,
        64,
        "Source name",
        "Tracking source name",
        "external.tracking.name");
    meta_name = new MetadataValue(desc_name);

    MetadataDescriptor desc_port = MetadataDescriptor(
        MetadataDescriptor::MetadataType::CHAR,
        16,
        "Source port",
        "Tracking source port",
        "external.tracking.port");
    meta_port = new MetadataValue(desc_port);

    MetadataDescriptor desc_address = MetadataDescriptor(
        MetadataDescriptor::MetadataType::CHAR,
        16,
        "Source address",
        "Tracking source address",
        "external.tracking.address");
    meta_address = new MetadataValue(desc_address);

    MetadataDescriptor desc_color = MetadataDescriptor(
        MetadataDescriptor::MetadataType::CHAR,
        16,
        "Source color",
        "Tracking source color",
        "external.tracking.color");
    meta_color = new MetadataValue(desc_color);

    MetadataDescriptor desc_position = MetadataDescriptor(
        MetadataDescriptor::MetadataType::FLOAT,
        4,
        "Source position",
        "Tracking  position",
        "external.tracking.position");
    meta_position = new MetadataValue(desc_position);
}

TTLEventPtr TrackingModule::createEvent(int64 sample_number, EventChannel *chan)
{
    auto *message = m_messageQueue->pop();
    if (!message)
        return nullptr;
    // attach metadata to the TTL Event as BinaryEvents aren't dealt with (yet?)
    // in GenericProcessor::checkForEvents()
    meta_name->setValue(m_name);
    meta_port->setValue(m_port);
    meta_address->setValue(m_address);
    meta_color->setValue(m_color);
    Array<float> pos;
    pos.add(message->position.x);
    pos.add(message->position.y);
    pos.add(message->position.height);
    pos.add(message->position.width);
    meta_position->setValue(pos);

    m_metadata.clear();
    m_metadata.add(meta_name);
    m_metadata.add(meta_port);
    m_metadata.add(meta_address);
    m_metadata.add(meta_color);
    m_metadata.add(meta_position);

    TTLEventPtr event = TTLEvent::createTTLEvent(chan,
                                                 message->timestamp,
                                                 16,
                                                 true,
                                                 m_metadata);

    return event;
};

TrackingNode::TrackingNode()
    : GenericProcessor("Tracker")
{
    addCategoricalParameter(Parameter::GLOBAL_SCOPE, "Source", "Tracking source", {}, 0);
    addStringParameter(Parameter::GLOBAL_SCOPE, "Port", "Tracking source OSC port", "27020");
    addStringParameter(Parameter::GLOBAL_SCOPE, "Address", "Tracking source OSC address", "/red");
    addCategoricalParameter(Parameter::GLOBAL_SCOPE, "Color", "Tracking source color to be displayed",
                            colors,
                            0);
    lastNumInputs = 0;
}

TrackingNode::~TrackingNode()
{
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

void TrackingNode::parameterValueChanged(Parameter *param)
{
    auto src_name = getSelectedSourceName();
    updateModule(param->getStreamId(), src_name, param);
}

void TrackingNode::addModule(uint16 streamID, String moduleName)
{
    if (!sourceNames.contains(moduleName))
    {
        // make sure the UDP port is unique
        int port = DEF_PORT;
        if (!trackingModules.isEmpty())
        {
            std::vector<int> ports;
            for (const auto &tm : trackingModules)
                ports.push_back(std::stoi(tm->m_port.toStdString()));
            auto maxPort = *std::max_element(ports.begin(), ports.end());
            port = maxPort + 1;
        }
        trackingModules.add(new TrackingModule(moduleName, String(port), this));
        sourceNames.add(moduleName);
    }
}

void TrackingNode::updateModule(uint16 streamID, String name, Parameter *param)
{
    if (sourceNames.contains(name))
    {
        for (auto &tm : trackingModules)
        {
            if (tm->m_name == name)
            {
                if (param->getName().equalsIgnoreCase("color"))
                {
                    CategoricalParameter *cparam = (CategoricalParameter *)param;
                    auto new_color = cparam->getSelectedString();
                    tm->m_color = new_color;
                }
                else if (param->getName().equalsIgnoreCase("port"))
                {
                    auto new_port = param->getValueAsString();
                    tm->m_port = new_port;
                }
                else if (param->getName().equalsIgnoreCase("address"))
                {
                    auto new_address = param->getValueAsString();
                    tm->m_address = new_address;
                }
                else if (param->getName().equalsIgnoreCase("source"))
                {
                    CategoricalParameter *cparam = (CategoricalParameter *)param;
                    auto src_name = cparam->getSelectedString();
                    auto *port = getParameter("Port");
                    auto *address = getParameter("Address");
                    auto *col = getParameter("Color");
                    CategoricalParameter *color = (CategoricalParameter *)getParameter("Color");
                    port->currentValue = tm->m_port;
                    address->currentValue = tm->m_address;
                    auto idx = colors.indexOf(tm->m_color);
                    color->setNextValue(idx);
                }
            }
        }
    }
}

void TrackingNode::removeModule(uint16 streamID, String moduleName)
{
    if (sourceNames.contains(moduleName))
    {
        for (auto &tm : trackingModules)
        {
            if (tm->alreadyExists(moduleName))
            {
                trackingModules.removeObject(tm);
                sourceNames.removeString(moduleName);
            }
        }
    }
}

TrackingModule *TrackingNode::getModule(const String &name)
{
    TrackingModule *tm = nullptr;
    for (auto &thismodule : trackingModules)
    {
        if (thismodule->m_name == name)
            tm = thismodule;
    }
    return tm;
}

void TrackingNode::updateSettings()
{
    dataStreams.clear();
    eventChannels.clear();

    DataStream::Settings streamsettings{"TrackingNode datastream",
                                        "Datastream for Tracking data received from Bonsai",
                                        "external.tracking.rawData",
                                        getDefaultSampleRate()};

    dataStreams.add(new DataStream(streamsettings));
    dataStreams.getLast()->addProcessor(processorInfo.get());

    EventChannel *events;

    EventChannel::Settings s{EventChannel::Type::TTL,
                             "Tracking data",
                             "Tracking data received from Bonsai. x, y, width, height",
                             "external.tracking.rawData",
                             dataStreams.getLast()};

    events = new EventChannel(s);
    eventChannel = events;
    String id = "trackingsource";
    events->setIdentifier(id);
    events->addProcessor(processorInfo.get());
    eventChannels.add(events);
    isEnabled = true;
    settings.update(getDataStreams());
}

void TrackingNode::process(AudioBuffer<float> &buffer)
{

    checkForEvents(true);
    if (!m_positionIsUpdated)
        return;

    lock.enter();

    for (auto stream : getDataStreams())
    {
        if ((*stream)["enable_stream"])
        {
            for (auto &module : trackingModules)
            {
                const uint16 streamId = stream->getStreamId();
                const int64 firstSampleInBlock = getFirstSampleNumberForBlock(streamId);
                TTLEventPtr ptr = module->createEvent(firstSampleInBlock, eventChannel);
                addEvent(ptr, firstSampleInBlock);
            }
        }
    }
    lock.exit();
    m_positionIsUpdated = false;
}

void TrackingNode::handleTTLEvent(TTLEventPtr event)
{
}

// TODO: CLEAN THIS

int TrackingNode::getNSources()
{
    return trackingModules.size();
}

void TrackingNode::receiveMessage(int port, String address, const TrackingData &message)
{
    // for (auto stream : getDataStreams())
    // {
    //     if ((*stream)["enable_stream"])
    //     {
    //         TrackingNodeSettings *selectedModule = settings[stream->getStreamId()];
    //         if (CoreServices::getRecordingStatus())
    //         {
    //             if (!m_isRecordingTimeLogged)
    //             {
    //                 m_received_msg = 0;
    //                 m_startingRecTimeMillis = Time::currentTimeMillis();
    //                 m_isRecordingTimeLogged = true;
    //                 std::cout << "Starting Recording Ts: " << m_startingRecTimeMillis << std::endl;
    //                 selectedModule->m_messageQueue->clear();
    //                 CoreServices::sendStatusMessage("Clearing queue before start recording");
    //             }
    //         }
    //         else
    //         {
    //             m_isRecordingTimeLogged = false;
    //         }
    //         if (CoreServices::getAcquisitionStatus()) // && !CoreServices::getRecordingStatus())
    //         {
    //             if (!m_isAcquisitionTimeLogged)
    //             {
    //                 m_startingAcqTimeMillis = Time::currentTimeMillis();
    //                 m_isAcquisitionTimeLogged = true;
    //                 std::cout << "Starting Acquisition at Ts: " << m_startingAcqTimeMillis << std::endl;
    //                 selectedModule->m_messageQueue->clear();
    //                 CoreServices::sendStatusMessage("Clearing queue before start acquisition");
    //             }
    //             int64 ts = CoreServices::getSoftwareTimestamp();

    //             TrackingData outputMessage = message;
    //             outputMessage.timestamp = ts;
    //             selectedModule->m_messageQueue->push(outputMessage);
    //             m_received_msg++;
    //         }
    //         else
    //             m_isAcquisitionTimeLogged = false;

    //         lock.exit();
    //     }
    // }
}

void TrackingNode::saveCustomParametersToXml(XmlElement *parentElement)
{
    // for (auto stream : getDataStreams())
    // {
    //     XmlElement *mainNode = parentElement->createNewChildElement("TrackingNode");
    //     auto module = settings[stream->getStreamId()];
    //     XmlElement *source = new XmlElement(module->m_source_name);
    //     source->setAttribute("port", module->m_port);
    //     source->setAttribute("address", module->m_address);
    //     source->setAttribute("color", module->m_color);
    // }
}

void TrackingNode::loadCustomParametersFromXml(XmlElement *xml)
{
    int streamIndex = 0;
    Array<const DataStream *> availableStreams = getDataStreams();
    for (auto *streamParams : xml->getChildIterator())
    {
        if (streamParams->hasTagName("TrackingNode"))
        {
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
    cout << "Destructing tracking server" << endl;
    // stop the OSC Listener thread running
    //    m_listeningSocket->Break();
    // allow the thread 2 seconds to stop cleanly - should be plenty of time.
    cout << "Destructed tracking server" << endl;
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
            cout << "ERROR: TrackingServer received message with wrong number of arguments. "
                 << "Expected " << argumentCount << ", got " << receivedMessage.ArgumentCount() << endl;
            return;
        }

        for (uint32 i = 0; i < receivedMessage.ArgumentCount(); i++)
        {
            if (receivedMessage.TypeTags()[i] != 'f')
            {
                cout << "TrackingServer only support 'f' (floats), not '"
                     << receivedMessage.TypeTags()[i] << "'" << endl;
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
            //            String address = processor->address();

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
    cout << "SLeeping!" << endl;
    sleep(1000);
    cout << "Running!" << endl;
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
