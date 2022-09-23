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

TTLEventPtr TrackingNodeSettings::createEvent(int sample_number, TrackingData * position) {
    if (!position)
        return nullptr;
    Array<float> pos;
    pos.add(position->position.x);
    pos.add(position->position.y);
    pos.add(position->position.height);
    pos.add(position->position.width);
    meta_position->setValue(pos);

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
    meta_name = std::make_unique<MetadataValue>(desc_name);
    meta_port = std::make_unique<MetadataValue>(desc_port);
    meta_address = std::make_unique<MetadataValue>(desc_address);
    meta_color = std::make_unique<MetadataValue>(desc_color);
    meta_position = std::make_unique<MetadataValue>(desc_position);
};

// TTLEventPtr TrackingModule::createEvent(int64 sample_number)
// {
//     std::cout << "TRACKINGMODULE CREATEEVENT" << std::endl;
//     auto *message = m_messageQueue->pop();
//     if (!message)
//         return nullptr;
//     // attach metadata to the TTL Event as BinaryEvents aren't dealt with (yet?)
//     // in GenericProcessor::checkForEvents()
//     meta_name->setValue(m_name);
//     meta_port->setValue(m_port);
//     meta_address->setValue(m_address);
//     meta_color->setValue(m_color);
//     Array<float> pos;
//     pos.add(message->position.x);
//     pos.add(message->position.y);
//     pos.add(message->position.height);
//     pos.add(message->position.width);
//     meta_position->setValue(pos);

//     m_metadata.clear();
//     m_metadata.add(meta_name.get());
//     m_metadata.add(meta_port.get());
//     m_metadata.add(meta_address.get());
//     m_metadata.add(meta_color.get());
//     m_metadata.add(meta_position.get());

//     TTLEventPtr event = TTLEvent::createTTLEvent(eventChannel,
//                                                  sample_number,
//                                                  0,
//                                                  true,
//                                                  m_metadata);
    
//     return event;
// };

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

TrackingNode::~TrackingNode()
{
    if (!trackingModules.isEmpty()) {
        for (auto & tm : trackingModules) {
            removeModule(tm->m_name);
        }
    }
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
    updateModule(src_name, param);
}

void TrackingNode::addModule(String moduleName)
{
    if (!sourceNames.contains(moduleName))
    {
        // make sure the UDP port is unique
        int port_name = DEF_PORT;
        if (!trackingModules.isEmpty())
        {
            std::vector<int> ports;
            for (const auto &tm : trackingModules)
                ports.push_back(std::stoi(tm->m_port.toStdString()));
            auto maxPort = *std::max_element(ports.begin(), ports.end());
            port_name = maxPort + 1;
        }
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

        auto tm = new TrackingModule(moduleName, String(port_name), this);

        // Add some metadata to the channel / stream
        events->addMetadata(desc_name, name);

        dataStreams.add(stream);
        eventChannels.add(events);
        trackingModules.add(tm);
        sourceNames.add(moduleName);
        auto streamId = stream->getStreamId();
        settings[streamId]->eventChannel = events;
        settings[streamId]->meta_address->setValue(tm->m_address);
        settings[streamId]->meta_port->setValue(tm->m_port);
        settings[streamId]->meta_color->setValue(tm->m_color);
        settings[streamId]->meta_name->setValue(tm->m_name);
        settings.update(getDataStreams());
        LOGD("logd output");
        std::cout << "cout output" << std::endl;
    }

}

void TrackingNode::updateModule(String name, Parameter *param)
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
                    settings[param->getStreamId()]->meta_color->setValue(new_color);
                }
                else if (param->getName().equalsIgnoreCase("port"))
                {
                    auto new_port = param->getValueAsString();
                    tm->m_port = new_port;
                    settings[param->getStreamId()]->meta_port->setValue(new_port);
                }
                else if (param->getName().equalsIgnoreCase("address"))
                {
                    auto new_address = param->getValueAsString();
                    tm->m_address = new_address;
                    settings[param->getStreamId()]->meta_address->setValue(new_address);
                }
                else if (param->getName().equalsIgnoreCase("source"))
                {
                    CategoricalParameter *cparam = (CategoricalParameter *)param;
                    auto new_name = cparam->getSelectedString();
                    settings[param->getStreamId()]->meta_name->setValue(new_name);
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
        settings.update(getDataStreams());
    }
}

void TrackingNode::removeModule(String moduleName)
{
    if (sourceNames.contains(moduleName))
    {
        for (auto &tm : trackingModules)
        {
            if (tm->alreadyExists(moduleName))
            {
                tm->m_server->stop();
                tm->m_server->stopThread(-1);
                tm->m_server->waitForThreadToExit(-1);
                for (auto stream : getDataStreams())
                {
                    for (auto chan : getEventChannels())
                    {
                        auto idx = chan->findMetadata(desc_name.getType(), desc_name.getLength(), desc_name.getIdentifier());
                        auto val = chan->getMetadataValue(idx);
                        String name;
                        val->getValue(name);
                        if (name.equalsIgnoreCase(moduleName))
                        {
                            eventChannels.removeObject(chan, true);
                        }
                    }
                    auto idx = stream->findMetadata(desc_name.getType(), desc_name.getLength(), desc_name.getIdentifier());
                    auto val = stream->getMetadataValue(idx);
                    String name;
                    val->getValue(name);
                    if (name.equalsIgnoreCase(moduleName))
                    {
                        std::cout << "Removing object from datastream" << std::endl;
                        dataStreams.removeObject(stream, true);
                    }
                }

                trackingModules.removeObject(tm, true);
                sourceNames.removeString(moduleName);
            }
        }
        settings.update(getDataStreams());
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
    isEnabled = true;
}

void TrackingNode::process(AudioBuffer<float> &buffer)
{
    if (!m_positionIsUpdated)
        return;
    std::cout << "in process" << std::endl;

    for (auto stream : getDataStreams())
    {
        auto streamId = stream->getStreamId();
        TrackingNodeSettings* module = settings[streamId];
        auto sample_number = getFirstSampleNumberForBlock(streamId);
        for (auto &tm: trackingModules) {
            if (tm->eventChannel == module->eventChannel) {
                auto * message = tm->m_messageQueue->pop();
                settings[streamId]->createEvent(sample_number, message);
            }
        }
        std::cout << "sample_number: " << 0 << std::endl;
    }
    m_positionIsUpdated = false;
}

void TrackingNode::handleTTLEvent(TTLEventPtr event)
{
    std::cout << "HANDLETTLEVENT" << std::endl;
}

// TODO: CLEAN THIS

int TrackingNode::getNSources()
{
    return trackingModules.size();
}

void TrackingNode::receiveMessage(int port, String address, const TrackingData &message)
{
    for (auto &tm : trackingModules)
    {
        lock.enter();
        if (CoreServices::getRecordingStatus())
        {
            if (!m_isRecordingTimeLogged)
            {
                m_received_msg = 0;
                m_startingRecTimeMillis = Time::currentTimeMillis();
                m_isRecordingTimeLogged = true;
                std::cout << "Starting Recording Ts: " << m_startingRecTimeMillis << std::endl;
                tm->m_messageQueue->clear();
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
                tm->m_messageQueue->clear();
                CoreServices::sendStatusMessage("Clearing queue before start acquisition");
            }

            m_positionIsUpdated = true;

            int64 ts = CoreServices::getSoftwareTimestamp();

            TrackingData outputMessage = message;
            outputMessage.timestamp = ts;
            tm->m_messageQueue->push(outputMessage);
            m_received_msg++;
        }
        else
            m_isAcquisitionTimeLogged = false;
        lock.exit();
    }
}

void TrackingNode::saveCustomParametersToXml(XmlElement *parentElement)
{
    // for (auto stream : getDataStreams())
    // {
    //     XmlElement *mainNode = parentElement->createNewChildElement("TrackingNode");
    //     if ((*stream)["enable_stream"])
    //     {
    //         for (auto &module : trackingModules)
    //         {

    //             XmlElement *source = new XmlElement(module->m_name);
    //             source->setAttribute("Port", module->m_port);
    //             source->setAttribute("Address", module->m_address);
    //             source->setAttribute("Color", module->m_color);
    //         }
    //     }
    // }
}

void TrackingNode::loadCustomParametersFromXml(XmlElement *xml)
{
    // int streamIndex = 0;
    // Array<const DataStream *> availableStreams = getDataStreams();
    // for (auto *streamParams : xml->getChildIterator())
    // {
    //     if (streamParams->hasTagName("TrackingNode"))
    //     {
    //     }
    // }
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
    std::cout << "TRACKINGSERVER RUN" << std::endl;
    std::cout << "Sleeping!" << endl;
    sleep(1000);
    std::cout << "Running!" << endl;
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
